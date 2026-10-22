#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/mind_wave_dependencies.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::FilterConfiguration;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;

namespace {

ProjectSettings smallSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 16;
    settings.canvasHeight = 8;
    return settings;
}

LayerId addLayer(Project& project) {
    Layer layer(0, "Layer", LayerType::Normal);
    layer.setBaseContent(sound_mind::core::silentContentFor(project.settings()));
    return project.addLayer(std::move(layer));
}

void paintWithSizeBoundTo(Project& project, LayerId layer, MindWaveId wave) {
    auto tool = std::make_unique<ProceduralConfiguration>();
    tool->setSizeMindWave(wave);
    Path path;
    PathNode node;
    node.anchor = TimeFrequencyPoint{0.1, 200.0};
    path.addNode(node);
    auto& log = project.operationLog();
    log.append(std::make_unique<PaintOperation>(log.reserveId(), layer, std::move(path), std::move(tool)));
}

}  // namespace

// v0.1.47.4: a MindWave edit only needs to redo what uses it.
TEST_CASE("JSON refers to a MindWave through any key ending in MindWave or MindWaveId", "[core][mind_wave_dependencies]") {
    using sound_mind::core::jsonRefersToMindWave;
    CHECK(jsonRefersToMindWave(nlohmann::json{{"opacityMindWaveId", 3}}, 3));
    CHECK(jsonRefersToMindWave(nlohmann::json{{"nested", {{"list", {{{"blurSigmaMindWaveId", 3}}}}}}}, 3));
    CHECK(jsonRefersToMindWave(nlohmann::json{{"customSpectrumMindWave", 3}}, 3));
    CHECK_FALSE(jsonRefersToMindWave(nlohmann::json{{"opacityMindWaveId", 4}}, 3));
    CHECK_FALSE(jsonRefersToMindWave(nlohmann::json{{"opacityMindWaveId", nullptr}}, 3));
    CHECK_FALSE(jsonRefersToMindWave(nlohmann::json{{"period", 3}}, 3));  // a 3 that isn't a MindWave.
}

TEST_CASE("A MindWave's dependents are the layers bound to it, by opacity, filter or stroke", "[core][mind_wave_dependencies]") {
    Project project = Project::createNew(smallSettings());
    const MindWaveId wave = project.addMindWave("Wave", MindWave{});
    const MindWaveId other = project.addMindWave("Other", MindWave{});

    const LayerId unrelated = addLayer(project);
    const LayerId byOpacity = addLayer(project);
    project.layerById(byOpacity)->setOpacityMindWave(wave);
    const LayerId byStroke = addLayer(project);
    paintWithSizeBoundTo(project, byStroke, wave);
    Layer filter(0, "Filter", LayerType::Filter);
    FilterConfiguration config;
    config.setBlurSigmaMindWave(wave);
    filter.setFilterConfiguration(config);
    const LayerId byFilter = project.addLayer(std::move(filter));
    const LayerId boundElsewhere = addLayer(project);
    project.layerById(boundElsewhere)->setOpacityMindWave(other);

    const auto dependents = sound_mind::core::mindWaveDependents(project, wave);
    CHECK(dependents.layers == std::vector<LayerId>{byOpacity, byStroke, byFilter});
    CHECK(dependents.paintedLayers == std::vector<LayerId>{byStroke});
    REQUIRE(dependents.lowestStackIndex.has_value());
    CHECK(project.layers()[*dependents.lowestStackIndex].id() == byOpacity);
    (void)unrelated;

    CHECK(sound_mind::core::mindWaveDependents(project, 999).layers.empty());
    CHECK_FALSE(sound_mind::core::mindWaveDependents(project, 999).lowestStackIndex.has_value());
}

TEST_CASE("A generated layer's recipe doesn't make it a dependent - its content is baked", "[core][mind_wave_dependencies]") {
    Project project = Project::createNew(smallSettings());
    const MindWaveId wave = project.addMindWave("Wave", MindWave{});
    const LayerId generated = addLayer(project);
    project.layerById(generated)->setBaseRecipe(
        nlohmann::json{{"kind", "generate"}, {"configuration", {{"verticalResolutionMindWave", wave}}}});
    CHECK(sound_mind::core::mindWaveDependents(project, wave).layers.empty());
}

TEST_CASE("An undone stroke bound to a MindWave no longer makes its layer a dependent", "[core][mind_wave_dependencies]") {
    Project project = Project::createNew(smallSettings());
    const MindWaveId wave = project.addMindWave("Wave", MindWave{});
    const LayerId layer = addLayer(project);
    project.history().setBaseline(project);
    paintWithSizeBoundTo(project, layer, wave);
    REQUIRE(project.history().commit(project, "Stroke").has_value());
    CHECK(sound_mind::core::mindWaveDependents(project, wave).paintedLayers == std::vector<LayerId>{layer});
    REQUIRE(project.history().undo(project).has_value());
    CHECK(sound_mind::core::mindWaveDependents(project, wave).layers.empty());
}
