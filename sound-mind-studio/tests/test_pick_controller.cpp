#include "test_pick_controller.h"

#include <cmath>
#include <memory>
#include <vector>

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

/// @brief Appends a two-node PaintOperation directly to `project`'s own
/// OperationLog, whose *first* node is Smooth with explicit, independent
/// handles - the fixture path-editing's own handle-drag tests need.
OperationId addSmoothPaintOperation(Project& project, LayerId layer, TimeFrequencyPoint anchor,
                                     TimeFrequencyPoint handleIn, TimeFrequencyPoint handleOut,
                                     TimeFrequencyPoint secondAnchor, const ToolConfiguration& config) {
    Path path;
    PathNode smooth;
    smooth.anchor = anchor;
    smooth.type = PathNodeType::Smooth;
    smooth.handleIn = handleIn;
    smooth.handleOut = handleOut;
    path.addNode(smooth);
    PathNode corner;
    corner.anchor = secondAnchor;
    corner.type = PathNodeType::Corner;
    path.addNode(corner);

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

void PickControllerTest::pickPadsAFillOperationsHitTestingByAMinimumForgivenessMargin() {
    // A Fill's own bounds() already exactly matches its real, visible
    // footprint, so it used to get zero hit-test padding - fine for a
    // visibly-colored Fill, where a real click naturally lands well
    // inside it, but not for a Cut's own silence Fill: nothing renders
    // there to click confidently away from the exact edge, and a real
    // mouse is far less precise than this test's own exact coordinate.
    // See kMinimumPickPaddingSeconds's own docs in pick_controller.cpp.
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    // 0.01s outside the Fill's own raw [0.2, 0.4] time bounds - within
    // the minimum forgiveness margin, but not inside the raw bounds
    // themselves.
    const bool picked = controller.pick(layerId, TimeFrequencyPoint{0.19, 500.0});

    QVERIFY(picked);
    QVERIFY(controller.hasSelection());
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

void PickControllerTest::bringToFrontMovesTheSelectionToTheTopOfItsStack() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.01);
    const OperationId aId = addPaintOperation(project, layerId, 0.1, 300.0, 0.12, 320.0, config);
    const OperationId bId = addPaintOperation(project, layerId, 0.4, 300.0, 0.42, 320.0, config);
    const OperationId cId = addPaintOperation(project, layerId, 0.7, 300.0, 0.72, 320.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.11, 310.0}));  // A.
    QSignalSpy contentSpy(&controller, &PickController::contentChanged);

    controller.bringToFront();

    QCOMPARE(contentSpy.count(), 1);
    QVERIFY(controller.hasSelection());  // still selected - same object, just moved.
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    std::vector<OperationId> ids;
    for (const auto* op : active) ids.push_back(op->id());
    QCOMPARE(ids, (std::vector<OperationId>{bId, cId, aId}));
}

void PickControllerTest::sendToBackMovesTheSelectionToTheBottomOfItsStack() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.01);
    const OperationId aId = addPaintOperation(project, layerId, 0.1, 300.0, 0.12, 320.0, config);
    const OperationId bId = addPaintOperation(project, layerId, 0.4, 300.0, 0.42, 320.0, config);
    const OperationId cId = addPaintOperation(project, layerId, 0.7, 300.0, 0.72, 320.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.71, 310.0}));  // C.

    controller.sendToBack();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    std::vector<OperationId> ids;
    for (const auto* op : active) ids.push_back(op->id());
    QCOMPARE(ids, (std::vector<OperationId>{cId, aId, bId}));
}

void PickControllerTest::bringForwardSwapsTheSelectionWithTheOneAboveIt() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.01);
    const OperationId aId = addPaintOperation(project, layerId, 0.1, 300.0, 0.12, 320.0, config);
    const OperationId bId = addPaintOperation(project, layerId, 0.4, 300.0, 0.42, 320.0, config);
    const OperationId cId = addPaintOperation(project, layerId, 0.7, 300.0, 0.72, 320.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.11, 310.0}));  // A.

    controller.bringForward();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    std::vector<OperationId> ids;
    for (const auto* op : active) ids.push_back(op->id());
    QCOMPARE(ids, (std::vector<OperationId>{bId, aId, cId}));
}

void PickControllerTest::sendBackwardSwapsTheSelectionWithTheOneBelowIt() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.01);
    const OperationId aId = addPaintOperation(project, layerId, 0.1, 300.0, 0.12, 320.0, config);
    const OperationId bId = addPaintOperation(project, layerId, 0.4, 300.0, 0.42, 320.0, config);
    const OperationId cId = addPaintOperation(project, layerId, 0.7, 300.0, 0.72, 320.0, config);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.71, 310.0}));  // C.

    controller.sendBackward();

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    std::vector<OperationId> ids;
    for (const auto* op : active) ids.push_back(op->id());
    QCOMPARE(ids, (std::vector<OperationId>{aId, cId, bId}));
}

void PickControllerTest::reorderMethodsAreNoOpsWithNoSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.1, 300.0, 0.12, 320.0, makeOpaqueTool(0.01));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy contentSpy(&controller, &PickController::contentChanged);

    controller.bringToFront();
    controller.sendToBack();
    controller.bringForward();
    controller.sendBackward();

    QCOMPARE(contentSpy.count(), 0);
}

void PickControllerTest::reorderMethodsEmitNoContentChangedWhenAlreadyAtTheRequestedEnd() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto config = makeOpaqueTool(0.01);
    addPaintOperation(project, layerId, 0.1, 300.0, 0.12, 320.0, config);
    addPaintOperation(project, layerId, 0.4, 300.0, 0.42, 320.0, config);
    addPaintOperation(project, layerId, 0.7, 300.0, 0.72, 320.0, config);  // C, already topmost.

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.71, 310.0}));  // C.
    QSignalSpy contentSpy(&controller, &PickController::contentChanged);

    controller.bringToFront();  // already topmost - no change.
    controller.bringForward();  // already topmost - no change.

    QCOMPARE(contentSpy.count(), 0);
}

void PickControllerTest::beginPathEditIsANoOpWithNoSelection() {
    Project project = Project::createNew(testSettings());
    addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);

    QVERIFY(!controller.beginPathEdit());
    QVERIFY(!controller.isPathEditActive());
}

void PickControllerTest::beginPathEditIsANoOpForAFillSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addFillOperation(project, layerId, 0.2, 400.0, 0.4, 600.0);

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    QVERIFY(!controller.beginPathEdit());
    QVERIFY(!controller.isPathEditActive());
}

void PickControllerTest::beginPathEditSucceedsForAPaintSelectionAndCopiesItsPath() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QSignalSpy pathSpy(&controller, &PickController::pathChanged);

    QVERIFY(controller.beginPathEdit());

    QVERIFY(controller.isPathEditActive());
    QCOMPARE(pathSpy.count(), 1);
    QCOMPARE(controller.currentPreviewPath().nodes().size(), std::size_t{2});
    QVERIFY(!controller.selectedPathNodeIndex().has_value());
}

void PickControllerTest::selectPathNodeNearSelectsTheClosestNodeWithinTolerance() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());

    // Well within tolerance (see pick_controller.cpp's own
    // kNodeHitToleranceSeconds - a few hundredths of a second/Hz here).
    const bool hit = controller.pick(layerId, TimeFrequencyPoint{0.201, 401.0});

    QVERIFY(hit);
    QVERIFY(controller.selectedPathNodeIndex().has_value());
    QCOMPARE(*controller.selectedPathNodeIndex(), std::size_t{0});
}

void PickControllerTest::selectPathNodeNearDeselectsWhenNothingIsClose() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.2, 400.0}));  // select node 0 first.

    const bool hit = controller.pick(layerId, TimeFrequencyPoint{0.9, 1900.0});  // far from every node.

    QVERIFY(!hit);
    QVERIFY(!controller.selectedPathNodeIndex().has_value());
    QVERIFY(controller.isPathEditActive());  // the session itself stays active.
}

void PickControllerTest::draggingASelectedNodesAnchorMovesItAndItsHandles() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const TimeFrequencyPoint anchor{0.3, 500.0};
    const TimeFrequencyPoint handleIn{0.25, 480.0};
    const TimeFrequencyPoint handleOut{0.35, 520.0};
    addSmoothPaintOperation(project, layerId, anchor, handleIn, handleOut, TimeFrequencyPoint{0.6, 700.0},
                             makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, anchor));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, anchor));  // selects node 0's own anchor.
    QCOMPARE(*controller.selectedPathNodeIndex(), std::size_t{0});

    controller.continueMove(TimeFrequencyPoint{anchor.timeSeconds + 0.1, anchor.frequencyHz + 100.0});

    const auto& node = controller.currentPreviewPath().nodes().front();
    QCOMPARE(node.anchor.timeSeconds, anchor.timeSeconds + 0.1);
    QCOMPARE(node.anchor.frequencyHz, anchor.frequencyHz + 100.0);
    QVERIFY(node.handleIn.has_value());
    QCOMPARE(node.handleIn->timeSeconds, handleIn.timeSeconds + 0.1);
    QCOMPARE(node.handleIn->frequencyHz, handleIn.frequencyHz + 100.0);
    QVERIFY(node.handleOut.has_value());
    QCOMPARE(node.handleOut->timeSeconds, handleOut.timeSeconds + 0.1);
    QCOMPARE(node.handleOut->frequencyHz, handleOut.frequencyHz + 100.0);
}

void PickControllerTest::draggingASelectedSmoothNodesHandleMirrorsTheOppositeHandle() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const TimeFrequencyPoint anchor{0.3, 500.0};
    const TimeFrequencyPoint handleIn{0.25, 480.0};
    const TimeFrequencyPoint handleOut{0.35, 520.0};
    addSmoothPaintOperation(project, layerId, anchor, handleIn, handleOut, TimeFrequencyPoint{0.6, 700.0},
                             makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, anchor));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, handleOut));  // selects node 0's own handleOut specifically.
    QCOMPARE(*controller.selectedPathNodeIndex(), std::size_t{0});

    // Drag handleOut straight up in frequency, well past tolerance.
    const TimeFrequencyPoint newHandleOut{handleOut.timeSeconds, handleOut.frequencyHz + 200.0};
    controller.continueMove(newHandleOut);

    const auto& node = controller.currentPreviewPath().nodes().front();
    QCOMPARE(node.handleOut->timeSeconds, newHandleOut.timeSeconds);
    QCOMPARE(node.handleOut->frequencyHz, newHandleOut.frequencyHz);
    // handleIn mirrors through the anchor: anchor - (newHandleOut - anchor).
    QCOMPARE(node.handleIn->timeSeconds, 2.0 * anchor.timeSeconds - newHandleOut.timeSeconds);
    QCOMPARE(node.handleIn->frequencyHz, 2.0 * anchor.frequencyHz - newHandleOut.frequencyHz);
}

void PickControllerTest::deleteSelectedPathNodeRemovesItButRefusesToEmptyThePath() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.2, 400.0}));

    controller.deleteSelectedPathNode();
    QCOMPARE(controller.currentPreviewPath().nodes().size(), std::size_t{1});
    QVERIFY(!controller.selectedPathNodeIndex().has_value());

    // Select the one remaining node and try again - refused.
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.4, 600.0}));
    controller.deleteSelectedPathNode();

    QCOMPARE(controller.currentPreviewPath().nodes().size(), std::size_t{1});
}

void PickControllerTest::toggleSelectedPathNodeTypeConvertsCornerToSmoothAndBack() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.2, 400.0}));
    QCOMPARE(controller.currentPreviewPath().nodes().front().type, PathNodeType::Corner);

    controller.toggleSelectedPathNodeType();

    // Node 0 has only one neighbor (node 1, at (0.4, 600.0)) - handleOut
    // extends a third of the way toward it, handleIn the same distance
    // the opposite way - see smoothedHandleTangent()'s own docs for why
    // this reduces to exactly a third of the raw anchor-to-neighbor delta
    // here (the fraction branch, not the floor, wins for this fixture's
    // own distances).
    const auto& smoothed = controller.currentPreviewPath().nodes().front();
    QCOMPARE(smoothed.type, PathNodeType::Smooth);
    QVERIFY(smoothed.handleIn.has_value());
    QVERIFY(smoothed.handleOut.has_value());
    const double expectedDeltaTime = (0.4 - 0.2) / 3.0;
    const double expectedDeltaFrequency = (600.0 - 400.0) / 3.0;
    QCOMPARE(smoothed.handleOut->timeSeconds, smoothed.anchor.timeSeconds + expectedDeltaTime);
    QCOMPARE(smoothed.handleOut->frequencyHz, smoothed.anchor.frequencyHz + expectedDeltaFrequency);
    QCOMPARE(smoothed.handleIn->timeSeconds, smoothed.anchor.timeSeconds - expectedDeltaTime);
    QCOMPARE(smoothed.handleIn->frequencyHz, smoothed.anchor.frequencyHz - expectedDeltaFrequency);

    controller.toggleSelectedPathNodeType();

    const auto& cornered = controller.currentPreviewPath().nodes().front();
    QCOMPARE(cornered.type, PathNodeType::Corner);
    QVERIFY(!cornered.handleIn.has_value());
    QVERIFY(!cornered.handleOut.has_value());
}

void PickControllerTest::toggleSelectedPathNodeTypeExtendsHandlesPerpendicularToTheCornersBisector() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);

    // A symmetric right-angle corner - equal-length edges on both sides
    // (0.2s/400Hz-equivalent each) - so smoothedHandleTangent()'s own
    // perpendicular-to-the-bisector result is easy to check by hand: the
    // prev edge runs due earlier in time, the next edge runs due higher
    // in frequency, so the corner's own bisector is (-1, 1) in this
    // normalized space, and the tangent perpendicular to it should have
    // zero dot product against that.
    Path path;
    PathNode prevNode;
    prevNode.anchor = TimeFrequencyPoint{0.3, 1000.0};
    prevNode.type = PathNodeType::Corner;
    path.addNode(prevNode);
    PathNode cornerNode;
    cornerNode.anchor = TimeFrequencyPoint{0.5, 1000.0};
    cornerNode.type = PathNodeType::Corner;
    path.addNode(cornerNode);
    PathNode nextNode;
    nextNode.anchor = TimeFrequencyPoint{0.5, 1400.0};
    nextNode.type = PathNodeType::Corner;
    path.addNode(nextNode);

    auto& log = project.operationLog();
    const OperationId id = log.reserveId();
    log.append(std::make_unique<PaintOperation>(id, layerId, std::move(path), makeOpaqueTool(0.02)));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.4, 1200.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.5, 1000.0}));  // selects the corner node.
    QCOMPARE(*controller.selectedPathNodeIndex(), std::size_t{1});

    controller.toggleSelectedPathNodeType();

    const auto& smoothed = controller.currentPreviewPath().nodes()[1];
    QCOMPARE(smoothed.type, PathNodeType::Smooth);
    QVERIFY(smoothed.handleIn.has_value());
    QVERIFY(smoothed.handleOut.has_value());

    const double scale = sound_mind::core::frequencyToTimeScaleFor(project.settings());

    // The anchor is the exact midpoint between its own two handles.
    QCOMPARE(smoothed.handleOut->timeSeconds + smoothed.handleIn->timeSeconds, 2.0 * smoothed.anchor.timeSeconds);
    QCOMPARE(smoothed.handleOut->frequencyHz + smoothed.handleIn->frequencyHz, 2.0 * smoothed.anchor.frequencyHz);

    // Not collapsed onto the anchor - comfortably farther than the node
    // hit-test tolerance, so it's actually its own clickable target.
    const double dtOut = smoothed.handleOut->timeSeconds - smoothed.anchor.timeSeconds;
    const double dfOutNormalized = (smoothed.handleOut->frequencyHz - smoothed.anchor.frequencyHz) / scale;
    const double handleDistance = std::sqrt(dtOut * dtOut + dfOutNormalized * dfOutNormalized);
    QVERIFY(handleDistance > 0.03);  // kNodeHitToleranceSeconds, in pick_controller.cpp.

    // Perpendicular to the corner's own bisector.
    constexpr double bisectorDt = -1.0;
    constexpr double bisectorDfNormalized = 1.0;
    const double dot = dtOut * bisectorDt + dfOutNormalized * bisectorDfNormalized;
    QVERIFY(std::abs(dot) < 1e-9);

    // Oriented toward the *next* neighbor's own side (positive frequency
    // direction here), not the previous one's.
    QVERIFY(dfOutNormalized > 0.0);
}

void PickControllerTest::toggleSelectedPathNodeTypeCollapsesHandlesForAnIsolatedSingleNodePath() {
    // No neighbor at all to take a direction from - smoothedHandleTangent()
    // has nothing to extend toward, so this is the one case that still
    // falls back to the old collapsed-on-anchor placement.
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);

    Path path;
    PathNode onlyNode;
    onlyNode.anchor = TimeFrequencyPoint{0.3, 500.0};
    onlyNode.type = PathNodeType::Corner;
    path.addNode(onlyNode);

    auto& log = project.operationLog();
    const OperationId id = log.reserveId();
    log.append(std::make_unique<PaintOperation>(id, layerId, std::move(path), makeOpaqueTool(0.02)));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));

    controller.toggleSelectedPathNodeType();

    const auto& smoothed = controller.currentPreviewPath().nodes().front();
    QCOMPARE(smoothed.type, PathNodeType::Smooth);
    QVERIFY(smoothed.handleIn.has_value());
    QVERIFY(smoothed.handleOut.has_value());
    QCOMPARE(smoothed.handleIn->timeSeconds, smoothed.anchor.timeSeconds);
    QCOMPARE(smoothed.handleIn->frequencyHz, smoothed.anchor.frequencyHz);
    QCOMPARE(smoothed.handleOut->timeSeconds, smoothed.anchor.timeSeconds);
    QCOMPARE(smoothed.handleOut->frequencyHz, smoothed.anchor.frequencyHz);
}

void PickControllerTest::commitPathEditSupersedesTheOriginalWithEditedGeometryKeepingItsGradient() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const OperationId originalId =
        addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02, 0.0f, -10.0f));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.2, 400.0}));
    controller.continueMove(TimeFrequencyPoint{0.25, 450.0});
    QSignalSpy contentSpy(&controller, &PickController::contentChanged);

    controller.commitPathEdit();

    QVERIFY(!controller.isPathEditActive());
    QCOMPARE(contentSpy.count(), 1);
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* edited = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(edited != nullptr);
    QVERIFY(edited->supersedes().has_value());
    QCOMPARE(*edited->supersedes(), originalId);
    QCOMPARE(edited->path().nodes().front().anchor.timeSeconds, 0.25);
    // The gradient is untouched - addPaintOperation()'s own fixture path
    // never sets one, so it's still the plain, fully-transparent default
    // (leftIntensity 0.0f) - not re-seeded from the tool's own -10.0f
    // default gradient the way applyToolConfiguration() would.
    QCOMPARE(edited->path().gradient().stops().front().leftIntensity, 0.0f);
}

void PickControllerTest::cancelPathEditDiscardsChangesAndLeavesTheOriginalSelected() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const OperationId originalId = addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.2, 400.0}));
    controller.continueMove(TimeFrequencyPoint{0.25, 450.0});

    controller.cancelPathEdit();

    QVERIFY(!controller.isPathEditActive());
    QCOMPARE(project.operationLog().size(), std::size_t{1});  // nothing committed.
    QVERIFY(controller.hasSelection());
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.front()->id(), originalId);  // the original, unedited operation.
}

void PickControllerTest::deleteSelectionDeletesTheSelectedNodeWhileEditingInsteadOfTheWholeObject() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.2, 400.0}));

    controller.deleteSelection();

    QCOMPARE(controller.currentPreviewPath().nodes().size(), std::size_t{1});
    QCOMPARE(project.operationLog().size(), std::size_t{1});  // nothing committed - still just the original.
    QVERIFY(controller.isPathEditActive());
}

void PickControllerTest::clearSelectionExitsAnActivePathEditSessionWithoutCommitting() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    addPaintOperation(project, layerId, 0.2, 400.0, 0.4, 600.0, makeOpaqueTool(0.02));

    PaintController paintController;
    paintController.setProject(&project);
    PickController controller(&paintController);
    controller.setProject(&project);
    QVERIFY(controller.pick(layerId, TimeFrequencyPoint{0.3, 500.0}));
    QVERIFY(controller.beginPathEdit());

    controller.clearSelection();

    QVERIFY(!controller.isPathEditActive());
    QVERIFY(!controller.hasSelection());
    QCOMPARE(project.operationLog().size(), std::size_t{1});  // nothing committed.
}
