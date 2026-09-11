#include "test_pick_controller.h"

#include <memory>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/pick_controller.h"

using sound_mind::core::Clip;
using sound_mind::core::FillOperation;
using sound_mind::core::Gradient;
using sound_mind::core::GradientStop;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::OperationId;
using sound_mind::core::PaintOperation;
using sound_mind::core::PasteOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ToolConfiguration;
using sound_mind::studio::PaintController;
using sound_mind::studio::PickController;

namespace {

/// @brief Small, fast project settings - matching test_paint_controller.cpp's
/// own testSettings(), so frequencyToTimeScaleFor() derives the same
/// sensible, checkable value (2000 Hz per second-equivalent).
ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

/// @brief A Normal layer, added to `project`, with real (blank) content -
/// matching test_paint_controller.cpp's own addBlankNormalLayer(), so
/// rebuildLayerContent() (called by every PickController commit) always
/// has a real base to replay onto.
LayerId addBlankNormalLayer(Project& project) {
    Layer layer(0, "Test Layer", LayerType::Normal);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, 0.0f);
    content.rightMagnitudeDb.assign(pixelCount, 0.0f);
    content.sharedPhaseRadians.assign(pixelCount, 0.0f);
    layer.setContent(content);
    return project.addLayer(std::move(layer));
}

/// @brief A Procedural tool configuration with a real, distinctly-sized
/// brush - matching test_paint_controller.cpp's own makeOpaqueTool().
ToolConfiguration makeOpaqueTool(double size = 0.02, float falloff = 0.0f, float intensity = -10.0f) {
    ToolConfiguration config;
    config.setSize(size);
    config.setFalloff(falloff);
    auto stop = config.defaultGradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    config.defaultGradient().setStopValues(0, stop);
    config.defaultGradient().setStopValues(1, stop);
    return config;
}

/// @brief Appends a real, two-Corner-node diagonal PaintOperation directly
/// to `project`'s own OperationLog - the "already painted, ready to be
/// Picked" starting state every test here needs.
OperationId addPaintOperation(Project& project, LayerId layer, double startTime, double startFrequency,
                               double endTime, double endFrequency, const ToolConfiguration& config) {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{startTime, startFrequency};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{endTime, endFrequency};
    end.type = PathNodeType::Corner;
    path.addNode(end);

    auto& log = project.operationLog();
    const OperationId id = log.reserveId();
    log.append(std::make_unique<PaintOperation>(id, layer, std::move(path), config));
    return id;
}

TimeFrequencyRect makeTestBounds(double startTime, double startFrequency, double endTime, double endFrequency) {
    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = startTime;
    bounds.lowFrequencyHz = startFrequency;
    bounds.endTimeSeconds = endTime;
    bounds.highFrequencyHz = endFrequency;
    return bounds;
}

/// @brief A uniform (same value at both stops), fully opaque gradient.
Gradient makeOpaqueGradient(float intensity) {
    Gradient gradient;
    GradientStop stop;
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    gradient.setStopValues(0, stop);
    gradient.setStopValues(1, stop);
    return gradient;
}

/// @brief Appends a real FillOperation directly to `project`'s own
/// OperationLog - the "already filled, ready to be Picked" starting state.
OperationId addFillOperation(Project& project, LayerId layer, double startTime, double startFrequency,
                              double endTime, double endFrequency, float intensity = -10.0f) {
    auto& log = project.operationLog();
    const OperationId id = log.reserveId();
    log.append(std::make_unique<FillOperation>(
        id, layer, makeTestBounds(startTime, startFrequency, endTime, endFrequency), makeOpaqueGradient(intensity)));
    return id;
}

/// @brief Appends a real PasteOperation directly to `project`'s own
/// OperationLog - the "already pasted, ready to be Picked" starting state.
OperationId addPasteOperation(Project& project, LayerId layer, double startTime, double startFrequency,
                               double endTime, double endFrequency) {
    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.sharedPhaseRadians = {0.0f, 0.0f, 0.0f, 0.0f};

    auto& log = project.operationLog();
    const OperationId id = log.reserveId();
    log.append(std::make_unique<PasteOperation>(
        id, layer, makeTestBounds(startTime, startFrequency, endTime, endFrequency), clip));
    return id;
}

}  // namespace

void PickControllerTest::freshControllerHasNoSelection() {
    PaintController paintController;
    const PickController controller(&paintController);
    QVERIFY(!controller.hasSelection());
    QVERIFY(!controller.selectedConfiguration().has_value());
    QVERIFY(!controller.selectionBounds().has_value());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PickControllerTest::pickSelectsAnOperationUnderThePoint() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    const bool picked = controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0});

    QVERIFY(picked);
    QVERIFY(controller.hasSelection());
    QVERIFY(controller.selectedConfiguration().has_value());
    QCOMPARE(controller.selectedConfiguration()->size(), config.size());
    QVERIFY(controller.selectionBounds().has_value());
}

void PickControllerTest::pickReturnsFalseAndClearsSelectionWhenNothingIsUnderThePoint() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));  // select something first.

    const bool picked = controller.pick(layerId, TimeFrequencyPoint{0.9, 1900.0});  // far away.

    QVERIFY(!picked);
    QVERIFY(!controller.hasSelection());
}

void PickControllerTest::pickPrefersTheMostRecentOverlappingOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto olderConfig = makeOpaqueTool(0.01);
    const auto newerConfig = makeOpaqueTool(0.03);  // a distinct size, to tell them apart.
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, olderConfig);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, newerConfig);  // same footprint, painted later.

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    QCOMPARE(controller.selectedConfiguration()->size(), newerConfig.size());
}

void PickControllerTest::pickOnAnAlreadySelectedOperationCyclesToTheOccludedOneUnderneath() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto bottomConfig = makeOpaqueTool(0.01);
    const auto middleConfig = makeOpaqueTool(0.02);
    const auto topConfig = makeOpaqueTool(0.03);
    // Three fully-overlapping strokes (same footprint), oldest to newest -
    // painted one directly behind the other, the exact scenario a plain
    // "topmost always wins" pick() could never see past.
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, bottomConfig);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, middleConfig);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, topConfig);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    const TimeFrequencyPoint clickPoint{0.3, 500.0};
    QVERIFY(controller.pick(layerId, clickPoint));
    QCOMPARE(controller.selectedConfiguration()->size(), topConfig.size());

    QVERIFY(controller.pick(layerId, clickPoint));  // same spot - the already-selected object clicked again.
    QCOMPARE(controller.selectedConfiguration()->size(), middleConfig.size());

    QVERIFY(controller.pick(layerId, clickPoint));
    QCOMPARE(controller.selectedConfiguration()->size(), bottomConfig.size());

    QVERIFY(controller.pick(layerId, clickPoint));  // wraps back to the topmost after the occluded-most one.
    QCOMPARE(controller.selectedConfiguration()->size(), topConfig.size());
}

void PickControllerTest::pickOnADifferentUnselectedOperationDoesNotCycle() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto stackedBottomConfig = makeOpaqueTool(0.01);
    const auto stackedTopConfig = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, stackedBottomConfig);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, stackedTopConfig);
    const auto elsewhereConfig = makeOpaqueTool(0.01);
    addPaintOperation(project, layerId, 0.7, 1500.0, 0.8, 1700.0, elsewhereConfig);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QCOMPARE(controller.selectedConfiguration()->size(), stackedTopConfig.size());

    // A different, non-overlapping stroke elsewhere - not the same object
    // clicked again, so this selects it directly rather than cycling
    // through the first click's own stack.
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.75, 1600.0}));

    QCOMPARE(controller.selectedConfiguration()->size(), elsewhereConfig.size());
}

void PickControllerTest::pickPadsHitTestingByTheOperationsOwnBrushSize() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.1);  // a real, sizable brush radius.

    // A single-tap (one-node) Path - its own raw bounds() is a single,
    // zero-area point, per Path::bounds()'s own docs - so this only picks
    // at all if hit-testing pads by the brush's own size.
    Path path;
    PathNode tap;
    tap.anchor = TimeFrequencyPoint{0.3, 500.0};
    tap.type = PathNodeType::Corner;
    path.addNode(tap);
    auto& log = project.operationLog();
    const OperationId id = log.reserveId();
    log.append(std::make_unique<PaintOperation>(id, layerId, std::move(path), config));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    // 0.05s away from the tap's own single point - within the 0.1s
    // brush-size padding, but well outside the raw (zero-area) bounds.
    const bool picked = controller.pick(layerId, TimeFrequencyPoint{0.35, 500.0});

    QVERIFY(picked);
}

void PickControllerTest::continueMoveUpdatesTheLivePreviewPath() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QSignalSpy spy(&controller, &PickController::pathChanged);

    controller.continueMove(TimeFrequencyPoint{0.4, 600.0});  // +0.1s, +100Hz from the pick point.

    QCOMPARE(spy.count(), 1);
    const auto& nodes = controller.currentPreviewPath().nodes();
    QCOMPARE(nodes.size(), std::size_t{2});
    QCOMPARE(nodes.at(0).anchor.timeSeconds, 0.3);   // 0.2 + 0.1 delta.
    QCOMPARE(nodes.at(0).anchor.frequencyHz, 500.0);  // 400 + 100 delta.
}

void PickControllerTest::endMoveCommitsATranslatedSupersedingOperationAndKeepsItSelected() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    const OperationId originalId = addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QSignalSpy contentSpy(&controller, &PickController::contentChanged);
    QSignalSpy selectionSpy(&controller, &PickController::selectionChanged);

    controller.continueMove(TimeFrequencyPoint{0.4, 700.0});  // +0.1s, +200Hz.
    controller.endMove();

    QCOMPARE(project.operationLog().size(), std::size_t{2});
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* moved = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(moved != nullptr);
    QVERIFY(moved->supersedes().has_value());
    QCOMPARE(*moved->supersedes(), originalId);
    QCOMPARE(moved->path().nodes().at(0).anchor.timeSeconds, 0.3);
    QCOMPARE(moved->path().nodes().at(0).anchor.frequencyHz, 600.0);

    // Still selected - now the new, moved operation.
    QVERIFY(controller.hasSelection());
    QCOMPARE(controller.selectedConfiguration()->size(), config.size());
}

void PickControllerTest::endMoveWithoutAnyRealMovementDoesNotCommitAnything() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.endMove();  // no continueMove() call in between - a plain click.

    QCOMPARE(project.operationLog().size(), std::size_t{1});
    QVERIFY(controller.hasSelection());  // still selected, unchanged.
}

void PickControllerTest::applyToolConfigurationCommitsANewOperationWithTheSameGeometry() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    const OperationId originalId = addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    const auto newConfig = makeOpaqueTool(0.09, 0.5f, -3.0f);
    controller.applyToolConfiguration(newConfig);

    QCOMPARE(project.operationLog().size(), std::size_t{2});
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* modified = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(modified != nullptr);
    QVERIFY(modified->supersedes().has_value());
    QCOMPARE(*modified->supersedes(), originalId);
    QCOMPARE(modified->config().size(), newConfig.size());
    // Same geometry - unaffected by a tool-configuration-only edit.
    QCOMPARE(modified->path().nodes().at(0).anchor.timeSeconds, 0.2);
    QCOMPARE(modified->path().nodes().at(0).anchor.frequencyHz, 400.0);
}

void PickControllerTest::deleteSelectionCommitsATombstoneAndClearsSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.deleteSelection();

    QCOMPARE(project.operationLog().size(), std::size_t{2});
    QVERIFY(!controller.hasSelection());
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* tombstone = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(tombstone != nullptr);
    QVERIFY(tombstone->path().nodes().empty());
}

void PickControllerTest::clearSelectionEmitsSelectionChangedOnlyWhenSomethingWasSelected() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    QSignalSpy spy(&controller, &PickController::selectionChanged);
    controller.clearSelection();  // nothing selected yet.
    QCOMPARE(spy.count(), 0);

    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QCOMPARE(spy.count(), 1);

    controller.clearSelection();
    QCOMPARE(spy.count(), 2);
}

void PickControllerTest::setProjectClearsSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.setProject(nullptr);

    QVERIFY(!controller.hasSelection());
}

void PickControllerTest::pickSelectsAFillOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    const bool picked = controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0});

    QVERIFY(picked);
    QVERIFY(controller.hasSelection());
    QVERIFY(controller.selectionBounds().has_value());
}

void PickControllerTest::pickSelectsAPasteOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPasteOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    const bool picked = controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0});

    QVERIFY(picked);
    QVERIFY(controller.hasSelection());
    QVERIFY(controller.selectionBounds().has_value());
}

void PickControllerTest::selectedConfigurationIsNullForAFillOrPasteSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);
    addPasteOperation(project, layerId, 1.0, 400.0, 1.2, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(!controller.selectedConfiguration().has_value());

    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{1.1, 500.0}));
    QVERIFY(!controller.selectedConfiguration().has_value());
}

void PickControllerTest::endMoveOnAFillOperationCommitsATranslatedSupersedingFillOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const OperationId originalId = addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.continueMove(TimeFrequencyPoint{0.4, 700.0});  // +0.1s, +200Hz.
    controller.endMove();

    QCOMPARE(project.operationLog().size(), std::size_t{2});
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* moved = dynamic_cast<const FillOperation*>(active.front());
    QVERIFY(moved != nullptr);
    QVERIFY(moved->supersedes().has_value());
    QCOMPARE(*moved->supersedes(), originalId);
    QCOMPARE(moved->bounds().startTimeSeconds, 0.3);
    QCOMPARE(moved->bounds().lowFrequencyHz, 600.0);

    // Still selected - now the new, moved operation.
    QVERIFY(controller.hasSelection());
}

void PickControllerTest::endMoveOnAPasteOperationCommitsATranslatedSupersedingPasteOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const OperationId originalId = addPasteOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.continueMove(TimeFrequencyPoint{0.4, 700.0});
    controller.endMove();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* moved = dynamic_cast<const PasteOperation*>(active.front());
    QVERIFY(moved != nullptr);
    QVERIFY(moved->supersedes().has_value());
    QCOMPARE(*moved->supersedes(), originalId);
    QCOMPARE(moved->clip().leftMagnitudeDb.size(), std::size_t{4});  // the clip itself carries over unchanged.
}

void PickControllerTest::continueMoveOnAFillOperationShowsARectangularOutlinePreview() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.continueMove(TimeFrequencyPoint{0.4, 700.0});

    // A closed, 5-node rectangular outline - see currentPreviewPath()'s own docs.
    QCOMPARE(controller.currentPreviewPath().nodes().size(), std::size_t{5});
}

void PickControllerTest::applyToolConfigurationIsANoOpWhenAFillOperationIsSelected() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.applyToolConfiguration(makeOpaqueTool(0.05));

    QCOMPARE(project.operationLog().size(), std::size_t{1});  // nothing new committed.
}

void PickControllerTest::deleteSelectionOnAFillOperationCommitsASilenceFillTombstone() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const OperationId originalId = addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.deleteSelection();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* tombstone = dynamic_cast<const FillOperation*>(active.front());
    QVERIFY(tombstone != nullptr);
    QVERIFY(tombstone->supersedes().has_value());
    QCOMPARE(*tombstone->supersedes(), originalId);
    QCOMPARE(tombstone->gradient().stops().front().leftIntensity, -96.0f);
    QCOMPARE(tombstone->gradient().stops().front().leftOpacity, 1.0f);
    QVERIFY(!controller.hasSelection());
}

void PickControllerTest::deleteSelectionOnAPasteOperationCommitsASilenceFillTombstone() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const OperationId originalId = addPasteOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.deleteSelection();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    // A PasteOperation can't become a literal zero-effect copy of itself
    // (it always overwrites outright) - it's superseded by a silence Fill
    // over the same bounds instead, the same "clear this region"
    // mechanism Cut's own source-clearing already uses.
    const auto* tombstone = dynamic_cast<const FillOperation*>(active.front());
    QVERIFY(tombstone != nullptr);
    QVERIFY(tombstone->supersedes().has_value());
    QCOMPARE(*tombstone->supersedes(), originalId);
}

void PickControllerTest::endMovePreservesTheMovedOperationsOwnStackPosition() {
    // The actual reported bug: paint A, then paint B on top of the same
    // spot; moving A (reached by cycling past B) must NOT promote it
    // above B - B must stay on top afterward.
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    const OperationId aId = addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);  // B, same footprint, painted later.

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));  // selects B (topmost).
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));  // cycles to A (occluded).

    controller.continueMove(TimeFrequencyPoint{0.5, 900.0});
    controller.endMove();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{2});
    // A's own move must still sit *below* B - not promoted to the top.
    QVERIFY(active[0]->supersedes().has_value());
    QCOMPARE(*active[0]->supersedes(), aId);
    QVERIFY(!active[1]->supersedes().has_value());  // B, unchanged and still on top.
}

void PickControllerTest::deleteSelectionPreservesStackPositionOfOperationsAboveIt() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.02);
    const OperationId aId = addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, config);  // B, on top.

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));  // B.
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));  // cycles to A.

    controller.deleteSelection();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{2});
    QVERIFY(active[0]->supersedes().has_value());
    QCOMPARE(*active[0]->supersedes(), aId);  // A's own tombstone, still in A's old slot.
    QVERIFY(!active[1]->supersedes().has_value());  // B, unaffected.
}
