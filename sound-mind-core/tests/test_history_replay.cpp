#include <memory>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/history_replay.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::HistoryReplayer;
using sound_mind::core::Layer;
using sound_mind::core::LayerBase;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::ReplayOptions;

namespace {

ProjectSettings smallSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 32;
    settings.canvasHeight = 16;
    return settings;
}

/// A recipe the fake runner understands: {"kind": "marked", "marker": x}.
StreamImage marked(const ProjectSettings& settings, float marker) {
    StreamImage image = sound_mind::core::silentContentFor(settings);
    image.leftMagnitudeDb.at(0) = marker;
    return image;
}

ReplayOptions fakeRunnerOptions(int* calls = nullptr) {
    ReplayOptions options;
    options.runRecipe = [calls](const nlohmann::json& recipe, LayerId, const Project& project,
                                const sound_mind::core::ReplayRenderedLookup&,
                                std::string* error) -> std::optional<LayerBase> {
        if (calls != nullptr) {
            ++*calls;
        }
        if (!recipe.is_object() || recipe.value("kind", "") != "marked") {
            if (error != nullptr) {
                *error = "unknown recipe";
            }
            return std::nullopt;
        }
        return LayerBase{std::make_shared<const StreamImage>(marked(project.settings(), recipe.at("marker"))),
                         recipe, 0};
    };
    return options;
}

LayerId addMarkedLayer(Project& project, float marker) {
    Layer layer(0, "Layer", LayerType::Normal);
    layer.setBaseContent(marked(project.settings(), marker), nlohmann::json{{"kind", "marked"}, {"marker", marker}});
    return project.addLayer(std::move(layer));
}

void paintOn(Project& project, LayerId layer) {
    auto& log = project.operationLog();
    sound_mind::core::Path path;
    sound_mind::core::PathNode node;
    node.anchor = sound_mind::core::TimeFrequencyPoint{0.05, 400.0};
    path.addNode(node);
    log.append(std::make_unique<sound_mind::core::PaintOperation>(
        log.reserveId(), layer, std::move(path), std::make_unique<sound_mind::core::ProceduralConfiguration>()));
}

/// What the Studio's PaintController renders for a layer.
StreamImage renderedLive(const Project& project, LayerId id) {
    const Layer& layer = *project.layerById(id);
    const ProjectSettings settings = project.settings();
    return sound_mind::core::rebuildPaintedContent(*layer.baseContent(),
                                                   project.operationLog().activeOperationsTargeting(id),
                                                   sound_mind::core::frequencyToTimeScaleFor(settings), {}, {}, &settings);
}

}  // namespace

TEST_CASE("A new project's history starts at its creation; a loaded old one's doesn't", "[core][replay]") {
    Project project = Project::createNew(smallSettings());
    REQUIRE(project.history().startsAtCreation());
    const Project roundTripped = nlohmann::json(project).get<Project>();
    REQUIRE(roundTripped.history().startsAtCreation());
    nlohmann::json old = project;
    old.erase("history");
    REQUIRE_FALSE(old.get<Project>().history().startsAtCreation());
}

TEST_CASE("Creating a project is deterministic, so a replay starts from the original's exact state",
          "[core][replay]") {
    const Project a = Project::createNew(smallSettings());
    const Project b = Project::createNew(smallSettings());
    REQUIRE(nlohmann::json(a.layers()) == nlohmann::json(b.layers()));
}

TEST_CASE("Replaying a history rebuilds the project: layers, operations, rendering, resources", "[core][replay]") {
    Project live = Project::createNew(smallSettings());
    const LayerId a = addMarkedLayer(live, -2.0f);
    live.history().commit(live, "Imported A");
    const LayerId b = addMarkedLayer(live, -3.0f);
    live.history().commit(live, "Imported B");
    paintOn(live, a);
    live.layerById(a)->setOpacity(0.6f);
    live.history().commit(live, "Painted");
    live.addMindWave("W", sound_mind::core::MindWave{});
    live.history().commit(live, "MindWave");
    live.removeLayer(b);
    live.history().commit(live, "Deleted B");
    live.layerById(a)->setContent(renderedLive(live, a));

    HistoryReplayer replayer(live.settings(), fakeRunnerOptions());
    std::string error;
    REQUIRE(replayer.applyAll(live.history(), live.history().size(), &error));
    Project replayed = replayer.takeProject();

    REQUIRE(replayed.layers().size() == live.layers().size());
    for (std::size_t i = 0; i < live.layers().size(); ++i) {
        REQUIRE(nlohmann::json(replayed.layers()[i]) == nlohmann::json(live.layers()[i]));
    }
    REQUIRE(replayed.layerById(b) == nullptr);
    REQUIRE(replayed.layerById(a)->content()->leftMagnitudeDb == live.layerById(a)->content()->leftMagnitudeDb);
    REQUIRE(replayed.mindWaves().size() == 1);
    REQUIRE(replayed.operationLog().size() == live.operationLog().size());
}

TEST_CASE("Replay uses kept bases unless told not to", "[core][replay]") {
    Project live = Project::createNew(smallSettings());
    const LayerId id = addMarkedLayer(live, -1.0f);
    live.history().commit(live, "Imported");
    live.layerById(id)->setBaseContent(marked(live.settings(), -4.0f), nlohmann::json{{"kind", "marked"}, {"marker", -4.0f}});
    live.history().commit(live, "Recorded over");
    live.history().undo(live);  // keeps both bases of the second entry in memory.
    live.history().redo(live);

    int calls = 0;
    HistoryReplayer kept(live.settings(), fakeRunnerOptions(&calls));
    REQUIRE(kept.applyAll(live.history(), 2));
    const int keptCalls = calls;

    calls = 0;
    ReplayOptions options = fakeRunnerOptions(&calls);
    options.useKeptBases = false;
    HistoryReplayer fresh(live.settings(), std::move(options));
    REQUIRE(fresh.applyAll(live.history(), 2));
    REQUIRE(calls > keptCalls);
    REQUIRE(fresh.rendered(id)->leftMagnitudeDb.at(0) == -4.0f);
}

TEST_CASE("A layer without content replays without any recipe", "[core][replay]") {
    Project live = Project::createNew(smallSettings());
    Layer filter(0, "Filter", LayerType::Filter);
    const LayerId id = live.addLayer(std::move(filter));
    live.history().commit(live, "Added filter layer");

    ReplayOptions options;  // no recipe runner at all.
    HistoryReplayer replayer(live.settings(), std::move(options));
    REQUIRE(replayer.applyAll(live.history(), 1));
    REQUIRE(replayer.project().layerById(id) != nullptr);
    REQUIRE(replayer.project().layerById(id)->type() == LayerType::Filter);
}

TEST_CASE("A recipe that can't be remade stops the replay with a reason, changing nothing", "[core][replay]") {
    Project live = Project::createNew(smallSettings());
    Layer layer(0, "Mystery", LayerType::Normal);
    layer.setBaseContent(marked(live.settings(), -1.0f), nlohmann::json{{"kind", "unknownThing"}});
    live.addLayer(std::move(layer));
    live.history().commit(live, "Mystery import");

    HistoryReplayer replayer(live.settings(), fakeRunnerOptions());
    const std::size_t before = replayer.project().layers().size();
    std::string error;
    REQUIRE_FALSE(replayer.applyAll(live.history(), 1, &error));
    REQUIRE(error.find("Mystery import") != std::string::npos);
    REQUIRE(error.find("unknown recipe") != std::string::npos);
    REQUIRE(replayer.project().layers().size() == before);
}

TEST_CASE("Replay reports progress and can be cancelled", "[core][replay]") {
    Project live = Project::createNew(smallSettings());
    for (int i = 0; i < 4; ++i) {
        live.layers().front().setOpacity(0.1f * static_cast<float>(i + 1));
        live.history().commit(live, "Opacity");
    }
    std::size_t reported = 0;
    ReplayOptions options;
    options.progress = [&](std::size_t done, std::size_t total) {
        REQUIRE(total == 4);
        reported = done;
    };
    options.shouldCancel = [&]() { return reported >= 2; };
    HistoryReplayer replayer(live.settings(), std::move(options));
    std::string error;
    REQUIRE_FALSE(replayer.applyAll(live.history(), 4, &error));
    REQUIRE(reported == 2);
    REQUIRE(error == "Cancelled.");
}

TEST_CASE("A transform adapts each entry before it's applied", "[core][replay]") {
    Project live = Project::createNew(smallSettings());
    const LayerId id = addMarkedLayer(live, -1.0f);
    live.layerById(id)->setTranslationColumns(10);
    live.history().commit(live, "Imported and moved");

    ReplayOptions options = fakeRunnerOptions();
    options.transformEntry = [](sound_mind::core::HistoryEntry& entry) {
        for (auto& change : entry.layers) {
            if (change.after.is_object()) {
                change.after["translationColumns"] = 20;
            }
        }
    };
    HistoryReplayer replayer(live.settings(), std::move(options));
    REQUIRE(replayer.applyAll(live.history(), 1));
    REQUIRE(replayer.project().layerById(id)->translationColumns() == 20);
}
