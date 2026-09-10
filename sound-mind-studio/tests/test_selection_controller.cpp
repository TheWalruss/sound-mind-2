#include "test_selection_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/selection_controller.h"

using sound_mind::core::Gradient;
using sound_mind::core::GradientStop;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::PaintController;
using sound_mind::studio::SelectionController;

namespace {

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

Gradient makeUniformGradient(float intensity, float opacity) {
    Gradient gradient;
    GradientStop stop;
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = opacity;
    stop.rightOpacity = opacity;
    gradient.setStopValues(0, stop);
    gradient.setStopValues(1, stop);
    return gradient;
}

}  // namespace

void SelectionControllerTest::freshControllerHasNoSelection() {
    PaintController paintController;
    const SelectionController controller(&paintController);
    QVERIFY(!controller.hasSelection());
    QVERIFY(!controller.displayBounds().has_value());
}

void SelectionControllerTest::beginSelectionDragShowsADegenerateRectAtTheAnchor() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    QSignalSpy spy(&controller, &SelectionController::boundsChanged);

    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.3, 500.0});

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.displayBounds().has_value());
    const auto bounds = *controller.displayBounds();
    QCOMPARE(bounds.startTimeSeconds, bounds.endTimeSeconds);
    QCOMPARE(bounds.lowFrequencyHz, bounds.highFrequencyHz);
    QVERIFY(!controller.hasSelection());  // not committed yet.
}

void SelectionControllerTest::continueSelectionDragNormalizesTheRectRegardlessOfDragDirection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);

    // Dragging "backward" (up and to the left) from the anchor.
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.5, 700.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.2, 300.0});

    const auto bounds = *controller.displayBounds();
    QCOMPARE(bounds.startTimeSeconds, 0.2);
    QCOMPARE(bounds.endTimeSeconds, 0.5);
    QCOMPARE(bounds.lowFrequencyHz, 300.0);
    QCOMPARE(bounds.highFrequencyHz, 700.0);
}

void SelectionControllerTest::endSelectionDragWithRealMovementCommitsTheSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.2, 300.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.5, 700.0});
    QSignalSpy boundsSpy(&controller, &SelectionController::boundsChanged);
    QSignalSpy selectionSpy(&controller, &SelectionController::selectionChanged);

    controller.endSelectionDrag();

    QCOMPARE(boundsSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);
    QVERIFY(controller.hasSelection());
    const auto bounds = *controller.displayBounds();
    QCOMPARE(bounds.startTimeSeconds, 0.2);
    QCOMPARE(bounds.endTimeSeconds, 0.5);
}

void SelectionControllerTest::endSelectionDragWithoutMovementClearsAnyExistingSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    // Commit a real selection first.
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.2, 300.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.5, 700.0});
    controller.endSelectionDrag();
    QVERIFY(controller.hasSelection());

    // A plain click (no continueSelectionDrag() call) elsewhere - not a
    // drag - clears it instead of committing a zero-area selection.
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.8, 1500.0});
    controller.endSelectionDrag();

    QVERIFY(!controller.hasSelection());
    QVERIFY(!controller.displayBounds().has_value());
}

void SelectionControllerTest::cancelSelectionDragRevertsToThePriorCommittedSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.2, 300.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.5, 700.0});
    controller.endSelectionDrag();
    const auto committed = *controller.displayBounds();

    // Start a second drag, then abandon it.
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.8, 1500.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.9, 1800.0});
    controller.cancelSelectionDrag();

    QVERIFY(controller.hasSelection());
    const auto bounds = *controller.displayBounds();
    QCOMPARE(bounds.startTimeSeconds, committed.startTimeSeconds);
    QCOMPARE(bounds.endTimeSeconds, committed.endTimeSeconds);
}

void SelectionControllerTest::clearSelectionIsANoOpWhenNothingIsSelected() {
    PaintController paintController;
    SelectionController controller(&paintController);
    QSignalSpy spy(&controller, &SelectionController::selectionChanged);

    controller.clearSelection();

    QCOMPARE(spy.count(), 0);
}

void SelectionControllerTest::fillAppendsAFillOperationOverTheCommittedSelection() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.2, 300.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.5, 700.0});
    controller.endSelectionDrag();
    QSignalSpy contentSpy(&controller, &SelectionController::contentChanged);

    controller.fill(makeUniformGradient(-10.0f, 1.0f));

    QCOMPARE(project.operationLog().size(), std::size_t{1});
    QCOMPARE(contentSpy.count(), 1);
    const auto active = project.operationLog().activeOperationsTargeting(layerId);
    QCOMPARE(active.size(), std::size_t{1});
}

void SelectionControllerTest::fillIsANoOpWithNoCommittedSelection() {
    Project project = Project::createNew(testSettings());
    addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);

    controller.fill(makeUniformGradient(-10.0f, 1.0f));

    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void SelectionControllerTest::setProjectClearsSelectionAndAnyInProgressDrag() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    controller.beginSelectionDrag(layerId, TimeFrequencyPoint{0.2, 300.0});
    controller.continueSelectionDrag(TimeFrequencyPoint{0.5, 700.0});
    controller.endSelectionDrag();
    QVERIFY(controller.hasSelection());

    controller.setProject(nullptr);

    QVERIFY(!controller.hasSelection());
    QVERIFY(!controller.displayBounds().has_value());
}
