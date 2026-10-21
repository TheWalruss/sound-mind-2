#include "test_path_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/path_controller.h"
#include "sound_mind/studio/project_history.h"

using sound_mind::core::GuidePathOperation;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::PaintController;
using sound_mind::studio::PathController;
using sound_mind::studio::ProjectHistory;

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

/// @brief A Normal layer with real (blank) content.
LayerId addBlankNormalLayer(Project& project) {
    Layer layer(0, "Test Layer", LayerType::Normal);
    sound_mind::codec::StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(project.settings());
    content.frameCount = project.settings().canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, -96.0f);
    content.rightMagnitudeDb.assign(pixelCount, -96.0f);
    content.sharedPhaseRadians.assign(pixelCount, 0.0f);
    layer.setBaseContent(content);
    return project.addLayer(std::move(layer));
}

/// @brief Drags from 0.1 s / 300 Hz to 0.6 s / 900 Hz in a few steps.
void dragAcross(PathController& controller, LayerId layer) {
    controller.beginPath(layer, TimeFrequencyPoint{0.1, 300.0});
    for (int step = 1; step <= 5; ++step) {
        controller.continuePath(TimeFrequencyPoint{0.1 + 0.1 * step, 300.0 + 120.0 * step});
    }
}

}  // namespace

void PathControllerTest::freshControllerHasNoPathInProgress() {
    PaintController paintController;
    const PathController controller(&paintController);
    QVERIFY(!controller.isPathInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PathControllerTest::beginPathIsANoOpWithNoProject() {
    PaintController paintController;
    PathController controller(&paintController);
    controller.beginPath(LayerId{1}, TimeFrequencyPoint{0.1, 300.0});
    QVERIFY(!controller.isPathInProgress());
}

void PathControllerTest::continuePathFitsALivePreview() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy spy(&controller, &PathController::pathChanged);

    dragAcross(controller, layerId);

    QVERIFY(controller.isPathInProgress());
    QCOMPARE(spy.count(), 5);
    QVERIFY(controller.currentPreviewPath().nodes().size() >= 2);
    QCOMPARE(project.operationLog().size(), std::size_t{0});  // nothing committed mid-drag.
}

void PathControllerTest::endPathCommitsASilentGuidePathOnTheTargetLayer() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    const auto contentBefore = project.layerById(layerId)->content()->leftMagnitudeDb;
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy contentSpy(&controller, &PathController::contentChanged);

    dragAcross(controller, layerId);
    const auto id = controller.endPath();

    QVERIFY(id.has_value());
    QVERIFY(!controller.isPathInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
    QCOMPARE(project.operationLog().size(), std::size_t{1});
    const auto* guide = dynamic_cast<const GuidePathOperation*>(&project.operationLog().at(0));
    QVERIFY(guide != nullptr);
    QCOMPARE(guide->id(), *id);
    QCOMPARE(*guide->targetLayer(), layerId);
    QCOMPARE(guide->branches().size(), std::size_t{1});
    QVERIFY(guide->branches().front().path.nodes().size() >= 2);
    QCOMPARE(contentSpy.count(), 1);
    // Silent: the layer's own content is exactly what it was.
    QVERIFY(project.layerById(layerId)->content()->leftMagnitudeDb == contentBefore);
}

void PathControllerTest::endPathAfterAPressWithNoDragCommitsNothing() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);

    controller.beginPath(layerId, TimeFrequencyPoint{0.2, 400.0});
    QVERIFY(!controller.endPath().has_value());

    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PathControllerTest::endPathIsUndoableAsOneStep() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    ProjectHistory undoStack;
    PaintController paintController(&undoStack);
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);

    dragAcross(controller, layerId);
    controller.endPath();
    QVERIFY(undoStack.canUndo());

    undoStack.undo();

    QVERIFY(project.operationLog().activeOperationsTargeting(layerId).empty());
}

void PathControllerTest::cancelPathDiscardsWithoutCommittingAnything() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);

    dragAcross(controller, layerId);
    controller.cancelPath();

    QVERIFY(!controller.isPathInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
    QVERIFY(!controller.endPath().has_value());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PathControllerTest::setProjectCancelsAnyPathInProgress() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    PathController controller(&paintController);
    controller.setProject(&project);
    dragAcross(controller, layerId);

    controller.setProject(&project);

    QVERIFY(!controller.isPathInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}
