#include "test_path_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/path_controller.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::PaintOperation;
using sound_mind::core::PathNodeType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::PaintController;
using sound_mind::studio::PathController;

namespace {

/// @brief Small, fast project settings - matching test_pick_controller.cpp's
/// own testSettings().
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
/// matching test_pick_controller.cpp's own addBlankNormalLayer(), so
/// rebuildLayerContent() (called by every PathController commit) always
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

}  // namespace

void PathControllerTest::freshControllerHasNoPlacementInProgress() {
    PaintController paintController;
    const PathController controller(&paintController);
    QVERIFY(!controller.isPlacementInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PathControllerTest::placeNodeStartsPlacementAndAppendsToThePath() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy spy(&controller, &PathController::pathChanged);

    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});

    QVERIFY(controller.isPlacementInProgress());
    QCOMPARE(spy.count(), 1);
    const sound_mind::core::Path preview = controller.currentPreviewPath();
    const auto& nodes = preview.nodes();
    QCOMPARE(nodes.size(), std::size_t{1});
    QCOMPARE(nodes.front().anchor.timeSeconds, 0.2);
    QCOMPARE(nodes.front().anchor.frequencyHz, 400.0);
    QCOMPARE(nodes.front().type, PathNodeType::Corner);
}

void PathControllerTest::placeNodeIsANoOpWithNoProject() {
    PaintController paintController;
    PathController controller(&paintController);

    controller.placeNode(LayerId{1}, TimeFrequencyPoint{0.2, 400.0});

    QVERIFY(!controller.isPlacementInProgress());
}

void PathControllerTest::placeNodeASecondTimeAppendsToTheSameInProgressPathRegardlessOfLayerArgument() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const LayerId otherLayerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);

    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});
    // A different layer id here changes nothing - the target was already
    // captured at the first placeNode() of this session.
    controller.placeNode(otherLayerId, TimeFrequencyPoint{0.4, 600.0});
    controller.finishPath();

    const auto activeOnFirst = project.operationLog().activeOperationsTargeting(layerId);
    const auto activeOnOther = project.operationLog().activeOperationsTargeting(otherLayerId);
    QCOMPARE(activeOnFirst.size(), std::size_t{1});
    QCOMPARE(activeOnOther.size(), std::size_t{0});
    const auto* painted = dynamic_cast<const PaintOperation*>(activeOnFirst.front());
    QVERIFY(painted != nullptr);
    QCOMPARE(painted->path().nodes().size(), std::size_t{2});
}

void PathControllerTest::defaultNodeTypeDefaultsToCorner() {
    PaintController paintController;
    const PathController controller(&paintController);
    QCOMPARE(controller.defaultNodeType(), PathNodeType::Corner);
}

void PathControllerTest::setDefaultNodeTypeAffectsSubsequentlyPlacedNodesOnly() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);

    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});  // still Corner.
    controller.setDefaultNodeType(PathNodeType::Smooth);
    controller.placeNode(layerId, TimeFrequencyPoint{0.4, 600.0});  // now Smooth.

    const sound_mind::core::Path preview = controller.currentPreviewPath();
    const auto& nodes = preview.nodes();
    QCOMPARE(nodes.size(), std::size_t{2});
    QCOMPARE(nodes.at(0).type, PathNodeType::Corner);
    QCOMPARE(nodes.at(1).type, PathNodeType::Smooth);
}

void PathControllerTest::placingASmoothNodeCollapsesBothHandlesOntoItsAnchor() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    controller.setDefaultNodeType(PathNodeType::Smooth);

    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});

    const sound_mind::core::Path preview = controller.currentPreviewPath();
    const auto& node = preview.nodes().front();
    QVERIFY(node.handleIn.has_value());
    QVERIFY(node.handleOut.has_value());
    QCOMPARE(node.handleIn->timeSeconds, node.anchor.timeSeconds);
    QCOMPARE(node.handleIn->frequencyHz, node.anchor.frequencyHz);
    QCOMPARE(node.handleOut->timeSeconds, node.anchor.timeSeconds);
    QCOMPARE(node.handleOut->frequencyHz, node.anchor.frequencyHz);
}

void PathControllerTest::updateCursorAddsATransientNodeToThePreviewPathButNotTheRealPath() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});

    controller.updateCursor(TimeFrequencyPoint{0.5, 900.0});

    const sound_mind::core::Path preview = controller.currentPreviewPath();
    const auto& previewNodes = preview.nodes();
    QCOMPARE(previewNodes.size(), std::size_t{2});
    QCOMPARE(previewNodes.back().anchor.timeSeconds, 0.5);
    QCOMPARE(previewNodes.back().anchor.frequencyHz, 900.0);

    // The transient cursor node never became a real, committed node -
    // finishing now paints only the one node actually placed.
    controller.finishPath();
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    const auto* painted = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(painted != nullptr);
    QCOMPARE(painted->path().nodes().size(), std::size_t{1});
}

void PathControllerTest::updateCursorIsANoOpWithNoPlacementInProgress() {
    PaintController paintController;
    PathController controller(&paintController);
    QSignalSpy spy(&controller, &PathController::pathChanged);

    controller.updateCursor(TimeFrequencyPoint{0.5, 900.0});

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PathControllerTest::finishPathAppendsANewPaintOperationAndRebuildsTheLayer() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});
    controller.placeNode(layerId, TimeFrequencyPoint{0.4, 600.0});
    QSignalSpy contentSpy(&controller, &PathController::contentChanged);

    controller.finishPath();

    QCOMPARE(project.operationLog().size(), std::size_t{1});
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(contentSpy.takeFirst().at(0).value<LayerId>(), layerId);

    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
    const auto* painted = dynamic_cast<const PaintOperation*>(active.front());
    QVERIFY(painted != nullptr);
    QVERIFY(!painted->supersedes().has_value());  // a fresh path, not an edit of an existing one.
    QCOMPARE(painted->path().nodes().size(), std::size_t{2});
}

void PathControllerTest::finishPathClearsThePlacementAndThePreview() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});

    controller.finishPath();

    QVERIFY(!controller.isPlacementInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PathControllerTest::finishPathIsANoOpWithNoPlacementInProgress() {
    Project project = Project::createNew(testSettings());
    addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);

    controller.finishPath();

    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PathControllerTest::cancelPathDiscardsWithoutCommittingAnything() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});
    QSignalSpy spy(&controller, &PathController::pathChanged);

    controller.cancelPath();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.isPlacementInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PathControllerTest::cancelPathIsANoOpWithNoPlacementInProgress() {
    PaintController paintController;
    PathController controller(&paintController);
    QSignalSpy spy(&controller, &PathController::pathChanged);

    controller.cancelPath();

    QCOMPARE(spy.count(), 0);
}

void PathControllerTest::setProjectCancelsAnyInProgressPlacement() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    controller.placeNode(layerId, TimeFrequencyPoint{0.2, 400.0});
    QVERIFY(controller.isPlacementInProgress());

    controller.setProject(nullptr);

    QVERIFY(!controller.isPlacementInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}
