#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::isLayerAbove;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::mindGrainOperationsBrokenByReorder;
using sound_mind::core::mindGrainOperationsBrokenByRemovingLayer;
using sound_mind::core::NamedMindGrain;
using sound_mind::core::OperationId;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;

namespace {

Path makeTestPath() {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{0.0, 100.0};
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{1.0, 500.0};
    path.addNode(end);
    return path;
}

/// @brief A fresh project with two extra Normal layers added on top of the
/// default Background/Equalizer pair - per Project::addLayer()'s own docs,
/// each is inserted just below the Equalizer, so the resulting bottom-to-
/// top order is Background, `lower`, `upper`, Equalizer.
struct TestProjectWithTwoLayers {
    Project project = Project::createNew(ProjectSettings{});
    LayerId lower = 0;
    LayerId upper = 0;

    TestProjectWithTwoLayers() {
        lower = project.addLayer(Layer(0, "Lower", LayerType::Normal));
        upper = project.addLayer(Layer(0, "Upper", LayerType::Normal));
    }
};

/// @brief Appends an active PaintOperation targeting `targetLayer`, painted
/// with a MindGrainConfiguration referencing `sourceLayer` - the shape
/// mindGrainOperationsBrokenByRemovingLayer()/mindGrainOperationsBrokenByReorder()
/// scan for.
OperationId appendMindGrainPaint(Project& project, LayerId targetLayer, LayerId sourceLayer) {
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, sourceLayer, TimeFrequencyRect{});
    const OperationId id = project.operationLog().reserveId();
    project.operationLog().append(
        std::make_unique<PaintOperation>(id, targetLayer, makeTestPath(), std::move(config)));
    return id;
}

}  // namespace

TEST_CASE("A NamedMindGrain round-trips through JSON unchanged", "[core][mind_grain]") {
    NamedMindGrain original;
    original.id = 7;
    original.name = "Rain Texture";
    original.sourceLayerId = 3;
    original.bounds = TimeFrequencyRect{0.5, 1.5, 200.0, 800.0};

    const nlohmann::json json = original;
    const auto restored = json.get<NamedMindGrain>();

    REQUIRE(restored.id == original.id);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.sourceLayerId == original.sourceLayerId);
    REQUIRE(restored.bounds.startTimeSeconds == original.bounds.startTimeSeconds);
    REQUIRE(restored.bounds.endTimeSeconds == original.bounds.endTimeSeconds);
    REQUIRE(restored.bounds.lowFrequencyHz == original.bounds.lowFrequencyHz);
    REQUIRE(restored.bounds.highFrequencyHz == original.bounds.highFrequencyHz);
}

TEST_CASE("isLayerAbove is true only when the layer is strictly above the other", "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;

    REQUIRE(isLayerAbove(fixture.project, fixture.upper, fixture.lower));
    REQUIRE_FALSE(isLayerAbove(fixture.project, fixture.lower, fixture.upper));
    REQUIRE_FALSE(isLayerAbove(fixture.project, fixture.lower, fixture.lower));
}

TEST_CASE("isLayerAbove is false for either id not existing in the project", "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;
    constexpr LayerId kUnknown = 999;

    REQUIRE_FALSE(isLayerAbove(fixture.project, kUnknown, fixture.lower));
    REQUIRE_FALSE(isLayerAbove(fixture.project, fixture.upper, kUnknown));
}

TEST_CASE("mindGrainOperationsBrokenByRemovingLayer finds an operation whose source would be removed",
          "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;
    const OperationId opId = appendMindGrainPaint(fixture.project, fixture.upper, fixture.lower);

    const auto broken = mindGrainOperationsBrokenByRemovingLayer(fixture.project, fixture.lower);

    REQUIRE(broken.size() == 1);
    REQUIRE(broken.front() == opId);
}

TEST_CASE("mindGrainOperationsBrokenByRemovingLayer ignores removing the operation's own target layer",
          "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;
    appendMindGrainPaint(fixture.project, fixture.upper, fixture.lower);

    // Removing the layer the stroke was painted ONTO (not its source)
    // doesn't break the Mind Grain reference itself - the whole layer
    // (operation included) simply ceases to exist.
    const auto broken = mindGrainOperationsBrokenByRemovingLayer(fixture.project, fixture.upper);

    REQUIRE(broken.empty());
}

TEST_CASE("mindGrainOperationsBrokenByRemovingLayer finds nothing with no Mind Grain strokes at all",
          "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;

    const auto broken = mindGrainOperationsBrokenByRemovingLayer(fixture.project, fixture.lower);

    REQUIRE(broken.empty());
}

TEST_CASE("mindGrainOperationsBrokenByReorder finds an operation the proposed order would put at-or-below its source",
          "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;
    const OperationId opId = appendMindGrainPaint(fixture.project, fixture.upper, fixture.lower);

    // Swaps lower/upper - the stroke's own target would no longer be above
    // its source.
    const std::vector<LayerId> swapped{1, fixture.upper, fixture.lower, 2};
    const auto broken = mindGrainOperationsBrokenByReorder(fixture.project, swapped);

    REQUIRE(broken.size() == 1);
    REQUIRE(broken.front() == opId);
}

TEST_CASE("mindGrainOperationsBrokenByReorder finds nothing when the proposed order preserves the ordering",
          "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;
    appendMindGrainPaint(fixture.project, fixture.upper, fixture.lower);

    const std::vector<LayerId> unchanged{1, fixture.lower, fixture.upper, 2};
    const auto broken = mindGrainOperationsBrokenByReorder(fixture.project, unchanged);

    REQUIRE(broken.empty());
}

TEST_CASE("mindGrainOperationsBrokenByReorder conservatively treats a missing layer as broken",
          "[core][mind_grain]") {
    TestProjectWithTwoLayers fixture;
    const OperationId opId = appendMindGrainPaint(fixture.project, fixture.upper, fixture.lower);

    // fixture.lower is missing entirely from this proposed order.
    const std::vector<LayerId> missingSource{1, fixture.upper, 2};
    const auto broken = mindGrainOperationsBrokenByReorder(fixture.project, missingSource);

    REQUIRE(broken.size() == 1);
    REQUIRE(broken.front() == opId);
}
