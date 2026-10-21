#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/history.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::History;
using sound_mind::core::HistoryChange;
using sound_mind::core::Layer;
using sound_mind::core::LayerBase;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindWave;
using sound_mind::core::OperationId;
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

/// A recognizable image: silence with one cell set to `marker`.
StreamImage markedImage(const Project& project, float marker) {
    StreamImage image = sound_mind::core::silentContentFor(project.settings());
    image.leftMagnitudeDb.at(0) = marker;
    return image;
}

float markerOf(const Layer& layer) { return layer.content()->leftMagnitudeDb.at(0); }

LayerId addLayerWithContent(Project& project, float marker) {
    Layer layer(0, "Layer", LayerType::Normal);
    layer.setBaseContent(markedImage(project, marker), nlohmann::json{{"kind", "test"}});
    return project.addLayer(std::move(layer));
}

void paintOn(Project& project, LayerId layer) {
    auto& log = project.operationLog();
    Path path;
    PathNode node;
    node.anchor = TimeFrequencyPoint{0.1, 200.0};
    path.addNode(node);
    log.append(std::make_unique<PaintOperation>(log.reserveId(), layer, std::move(path),
                                                std::make_unique<ProceduralConfiguration>()));
}

}  // namespace

TEST_CASE("A new project's history has a baseline and no entries", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    REQUIRE(project.history().hasBaseline());
    REQUIRE(project.history().size() == 0);
    REQUIRE_FALSE(project.history().canUndo());
    REQUIRE_FALSE(project.history().commit(project, "Nothing").has_value());
}

TEST_CASE("Committing a layer property change records an entry that undoes and redoes", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = project.layers().front().id();
    project.layerById(id)->setOpacity(0.25f);

    const auto entryId = project.history().commit(project, "Changed opacity");
    REQUIRE(entryId.has_value());
    REQUIRE(project.history().size() == 1);
    REQUIRE(project.history().entry(0).description == "Changed opacity");

    const auto undone = project.history().undo(project);
    REQUIRE(undone.has_value());
    REQUIRE(undone->layers.contains(id));
    REQUIRE(undone->contentLayers.empty());  // a property, not content.
    REQUIRE(project.layerById(id)->opacity() == 1.0f);

    REQUIRE(project.history().redo(project).has_value());
    REQUIRE(project.layerById(id)->opacity() == 0.25f);
}

TEST_CASE("Undoing an added layer removes it; redoing brings back its exact content", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const std::size_t layersBefore = project.layers().size();
    const LayerId id = addLayerWithContent(project, -3.0f);
    project.history().commit(project, "Imported");

    const auto undone = project.history().undo(project);
    REQUIRE(undone.has_value());
    REQUIRE(undone->layerListChanged);
    REQUIRE(project.layers().size() == layersBefore);
    REQUIRE(project.layerById(id) == nullptr);

    const auto redone = project.history().redo(project);
    REQUIRE(redone.has_value());
    REQUIRE(redone->contentLayers.contains(id));
    REQUIRE(project.layers().size() == layersBefore + 1);
    REQUIRE(markerOf(*project.layerById(id)) == -3.0f);
    REQUIRE(project.layerById(id)->baseRecipe().at("kind") == "test");
}

TEST_CASE("Undoing a layer removal restores the layer, its content and its position", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId first = addLayerWithContent(project, -1.0f);
    const LayerId second = addLayerWithContent(project, -2.0f);
    project.history().commit(project, "Imported two");
    const auto orderBefore = [&]() {
        std::vector<LayerId> ids;
        for (const Layer& layer : project.layers()) {
            ids.push_back(layer.id());
        }
        return ids;
    }();

    REQUIRE(project.removeLayer(first));
    project.history().commit(project, "Deleted layer");
    REQUIRE(project.layerById(first) == nullptr);

    REQUIRE(project.history().undo(project).has_value());
    std::vector<LayerId> orderAfter;
    for (const Layer& layer : project.layers()) {
        orderAfter.push_back(layer.id());
    }
    REQUIRE(orderAfter == orderBefore);
    REQUIRE(markerOf(*project.layerById(first)) == -1.0f);
    REQUIRE(markerOf(*project.layerById(second)) == -2.0f);
}

TEST_CASE("Replacing a layer's base is undone exactly, revision included", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    project.history().commit(project, "Imported");
    const std::uint64_t revision = project.layerById(id)->baseRevision();

    project.layerById(id)->setBaseContent(markedImage(project, -9.0f), nlohmann::json{{"kind", "record"}});
    project.history().commit(project, "Recorded over");
    REQUIRE(project.history().entry(1).layers.front().baseChanged);

    const auto undone = project.history().undo(project);
    REQUIRE(undone.has_value());
    REQUIRE(undone->contentLayers.contains(id));
    REQUIRE(markerOf(*project.layerById(id)) == -1.0f);
    REQUIRE(project.layerById(id)->baseRevision() == revision);
    REQUIRE(project.layerById(id)->baseRecipe().at("kind") == "test");

    REQUIRE(project.history().redo(project).has_value());
    REQUIRE(markerOf(*project.layerById(id)) == -9.0f);
}

TEST_CASE("Appended operations are undone by truncating and redone from their JSON", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    project.history().commit(project, "Imported");
    paintOn(project, id);
    paintOn(project, id);
    project.history().commit(project, "Painted");
    REQUIRE(project.history().entry(1).appendedOperations.size() == 2);
    REQUIRE_FALSE(project.history().entry(1).stackOrderAfter.has_value());  // append-only.

    const auto undone = project.history().undo(project);
    REQUIRE(undone->operationsChanged);
    REQUIRE(undone->contentLayers.contains(id));
    REQUIRE(project.operationLog().size() == 0);
    REQUIRE(project.operationLog().nextId() == OperationId{1});

    project.history().redo(project);
    REQUIRE(project.operationLog().size() == 2);
    REQUIRE(project.operationLog().activeOperationsTargeting(id).size() == 2);
}

TEST_CASE("A reordering of operations is recorded and undone", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    paintOn(project, id);
    paintOn(project, id);
    project.history().commit(project, "Painted");

    REQUIRE(project.operationLog().bringToFront(OperationId{1}));
    project.history().commit(project, "Bring to Front");
    REQUIRE(project.history().entry(1).stackOrderAfter.has_value());

    project.history().undo(project);
    REQUIRE(project.operationLog().stackOrder() == std::vector<OperationId>{1, 2});
    project.history().redo(project);
    REQUIRE(project.operationLog().stackOrder() == std::vector<OperationId>{2, 1});
}

TEST_CASE("Baking a layer is recorded with its new base and undone together", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    paintOn(project, id);
    project.history().commit(project, "Painted");

    project.layerById(id)->setBaseContent(markedImage(project, -5.0f), nlohmann::json{{"kind", "pool"}});
    project.operationLog().bakeLayer(id);
    project.history().commit(project, "Pooled");
    REQUIRE(project.operationLog().activeOperationsTargeting(id).empty());

    const auto undone = project.history().undo(project);
    REQUIRE(undone->contentLayers.contains(id));
    REQUIRE(project.operationLog().activeOperationsTargeting(id).size() == 1);
    REQUIRE(markerOf(*project.layerById(id)) == -1.0f);
}

TEST_CASE("Resource changes are recorded per library and undone", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    project.addMindWave("Wobble", MindWave{});
    project.history().commit(project, "Added MindWave");
    REQUIRE(project.history().entry(0).resources.size() == 1);
    REQUIRE(project.history().entry(0).resources.front().collection == "mindWaves");

    const auto undone = project.history().undo(project);
    REQUIRE(undone->resources.contains("mindWaves"));
    REQUIRE(project.mindWaves().empty());
    project.history().redo(project);
    REQUIRE(project.mindWaves().size() == 1);
    REQUIRE(project.mindWaves().front().name == "Wobble");
}

TEST_CASE("Undo first commits uncommitted changes, so they're what gets undone", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    project.layers().front().setOpacity(0.5f);
    project.history().commit(project, "Changed opacity");
    project.layers().front().setOpacity(0.1f);  // not committed.
    REQUIRE(project.history().hasUncommittedChanges(project));

    project.history().undo(project);
    REQUIRE(project.layers().front().opacity() == 0.5f);
    REQUIRE(project.history().size() == 2);
    REQUIRE(project.history().entry(1).description == "Edit");
}

TEST_CASE("Reserving an id that's never used isn't a change", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    (void)project.operationLog().reserveId();
    REQUIRE_FALSE(project.history().hasUncommittedChanges(project));
    REQUIRE_FALSE(project.history().commit(project, "Nothing").has_value());
}

TEST_CASE("Committing after an undo drops the undone entries", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    project.layers().front().setOpacity(0.5f);
    project.history().commit(project, "First");
    project.layers().front().setOpacity(0.4f);
    project.history().commit(project, "Second");
    project.history().undo(project);

    project.layers().front().setBalance(0.2f);
    project.history().commit(project, "Third");
    REQUIRE(project.history().size() == 2);
    REQUIRE(project.history().entry(1).description == "Third");
    REQUIRE_FALSE(project.history().canRedo());
}

TEST_CASE("jumpTo steps through several entries and merges what changed", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId a = addLayerWithContent(project, -1.0f);
    project.history().commit(project, "A");
    const LayerId b = addLayerWithContent(project, -2.0f);
    project.history().commit(project, "B");

    const auto change = project.history().jumpTo(project, 0);
    REQUIRE(change.has_value());
    REQUIRE(change->layers.contains(a));
    REQUIRE(change->layers.contains(b));
    REQUIRE(project.history().position() == 0);
    REQUIRE(project.layerById(a) == nullptr);

    project.history().jumpTo(project, 2);
    REQUIRE(markerOf(*project.layerById(b)) == -2.0f);
}

TEST_CASE("The history is saved with the project; a base needed after reloading comes from the resolver",
          "[core][history]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-history" / "History.smproj";
    std::filesystem::remove_all(path.parent_path());
    std::filesystem::create_directories(path.parent_path());

    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    project.history().commit(project, "Imported");
    project.layerById(id)->setBaseContent(markedImage(project, -7.0f), nlohmann::json{{"kind", "record"}});
    project.history().commit(project, "Recorded over");
    project.layers().front().setOpacity(0.3f);
    project.history().commit(project, "Changed opacity");
    project.save(path);

    Project loaded = Project::load(path);
    REQUIRE(loaded.history().size() == 3);
    REQUIRE(loaded.history().position() == 3);
    REQUIRE(markerOf(*loaded.layerById(id)) == -7.0f);

    // A property change undoes without any images.
    REQUIRE(loaded.history().undo(loaded).has_value());
    REQUIRE(loaded.layers().front().opacity() == 1.0f);

    // The replaced base isn't in memory any more: without a resolver,
    // nothing happens.
    REQUIRE_FALSE(loaded.history().undo(loaded).has_value());
    REQUIRE(markerOf(*loaded.layerById(id)) == -7.0f);

    std::size_t askedPosition = 99;
    const auto resolver = [&](std::size_t position, LayerId layer) -> std::optional<LayerBase> {
        askedPosition = position;
        REQUIRE(layer == id);
        return LayerBase{std::make_shared<const StreamImage>(markedImage(loaded, -1.0f)), nullptr,
                         nlohmann::json{{"kind", "test"}}, 0};
    };
    REQUIRE(loaded.history().undo(loaded, resolver).has_value());
    REQUIRE(askedPosition == 1);
    REQUIRE(markerOf(*loaded.layerById(id)) == -1.0f);
    std::filesystem::remove_all(path.parent_path());
}

TEST_CASE("A painted layer's base is saved apart from its rendered content", "[core][history]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-history-base" / "Base.smproj";
    std::filesystem::remove_all(path.parent_path());
    std::filesystem::create_directories(path.parent_path());

    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    project.layerById(id)->setContent(markedImage(project, -4.0f));  // a render on top.
    REQUIRE(project.layerById(id)->baseContent()->leftMagnitudeDb.at(0) == -1.0f);
    project.save(path);

    const Project loaded = Project::load(path);
    REQUIRE(markerOf(*loaded.layerById(id)) == -4.0f);
    REQUIRE(loaded.layerById(id)->baseContent()->leftMagnitudeDb.at(0) == -1.0f);
    std::filesystem::remove_all(path.parent_path());
}

TEST_CASE("The kept-base budget drops the oldest images first", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -1.0f);
    project.history().commit(project, "Imported");
    project.layerById(id)->setBaseContent(markedImage(project, -2.0f));
    project.history().commit(project, "Replaced");
    REQUIRE(project.history().keptBaseBytes() > 0);

    project.history().setBaseBudgetBytes(0);
    REQUIRE(project.history().keptBaseBytes() == 0);
    REQUIRE_FALSE(project.history().undo(project).has_value());  // needs a resolver now.
}

TEST_CASE("applyEntry replays recorded entries into a fresh project", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    const LayerId id = addLayerWithContent(project, -6.0f);
    project.history().commit(project, "Imported");
    paintOn(project, id);
    project.layerById(id)->setOpacity(0.7f);
    project.history().commit(project, "Painted");

    // A new layer's base lives in the project, not the entry - a replay
    // supplies it (from the recipe, in a real one).
    Project replay = Project::createNew(smallSettings());
    auto entry = project.history().entry(0);
    REQUIRE_FALSE(History::applyEntry(replay, entry).has_value());
    const auto supplyBase = [&](LayerId layer) -> std::optional<LayerBase> {
        return LayerBase{std::make_shared<const StreamImage>(*project.layerById(layer)->baseContent()), nullptr,
                         project.layerById(layer)->baseRecipe(), 0};
    };
    for (std::size_t i = 0; i < project.history().size(); ++i) {
        auto replayed = project.history().entry(i);
        REQUIRE(History::applyEntry(replay, replayed, supplyBase).has_value());
    }
    REQUIRE(replay.layerById(id) != nullptr);
    REQUIRE(markerOf(*replay.layerById(id)) == -6.0f);
    REQUIRE(replay.layerById(id)->opacity() == 0.7f);
    REQUIRE(replay.operationLog().activeOperationsTargeting(id).size() == 1);
}

TEST_CASE("A history entry round-trips through JSON", "[core][history]") {
    Project project = Project::createNew(smallSettings());
    addLayerWithContent(project, -1.0f);
    paintOn(project, project.layers().back().id());
    project.addMindWave("W", MindWave{});
    project.history().commit(project, "Several", nlohmann::json{{"source", 3}});

    const auto& entry = project.history().entry(0);
    const auto parsed = nlohmann::json(entry).get<sound_mind::core::HistoryEntry>();
    REQUIRE(nlohmann::json(parsed) == nlohmann::json(entry));
    REQUIRE(parsed.metadata.at("source") == 3);
}
