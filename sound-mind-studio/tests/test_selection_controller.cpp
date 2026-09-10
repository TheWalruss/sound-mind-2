#include "test_selection_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/selection_controller.h"

using sound_mind::core::binIndexToFrequency;
using sound_mind::core::frameIndexToTime;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::Gradient;
using sound_mind::core::GradientStop;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::timeToFrameIndex;
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

/// @brief The row-major `[bin][frame]` index a plain StreamImage pixel
/// lives at - the same layout every content buffer in this codebase uses.
std::size_t pixelIndex(const sound_mind::codec::StreamImage& content, int frame, int bin) {
    return static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
}

/// @brief Directly sets one pixel's own left/right dB value on a layer's
/// content, bypassing any Operation - used to give a test a distinctive,
/// known value to later assert copy/cut/paste actually moved (or cleared).
void setPixel(Project& project, LayerId layerId, int frame, int bin, float dbValue) {
    Layer* layer = project.layerById(layerId);
    sound_mind::codec::StreamImage content = *layer->content();
    const std::size_t index = pixelIndex(content, frame, bin);
    content.leftMagnitudeDb[index] = dbValue;
    content.rightMagnitudeDb[index] = dbValue;
    layer->setContent(std::move(content));
}

/// @brief Reads one pixel's own left-channel dB value back off a layer's
/// current content.
float readPixel(const Project& project, LayerId layerId, int frame, int bin) {
    const Layer* layer = project.layerById(layerId);
    const auto& content = *layer->content();
    return content.leftMagnitudeDb[pixelIndex(content, frame, bin)];
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

sound_mind::codec::StreamCodecConfig testConfig() { return sound_mind::core::streamCodecConfigFor(testSettings()); }

/// @brief Drags out and commits a rectangular selection covering exactly
/// `[frameLow, frameHigh] x [binLow, binHigh]`, in the same time/frequency
/// space captureClip()/applyPasteOperation() would resolve those bounds
/// back to.
void selectRect(SelectionController& controller, LayerId layerId, const sound_mind::codec::StreamCodecConfig& config,
                 int frameLow, int frameHigh, int binLow, int binHigh) {
    const TimeFrequencyPoint anchor{frameIndexToTime(frameLow, config),
                                     binIndexToFrequency(static_cast<float>(binLow), config)};
    const TimeFrequencyPoint far{frameIndexToTime(frameHigh, config),
                                  binIndexToFrequency(static_cast<float>(binHigh), config)};
    controller.beginSelectionDrag(layerId, anchor);
    controller.continueSelectionDrag(far);
    controller.endSelectionDrag();
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

void SelectionControllerTest::freshControllerHasNoClipboard() {
    PaintController paintController;
    const SelectionController controller(&paintController);
    QVERIFY(!controller.hasClipboard());
}

void SelectionControllerTest::copySelectionCapturesTheSelectionOntoTheClipboard() {
    const auto config = testConfig();
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    setPixel(project, layerId, 25, 10, -3.0f);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    selectRect(controller, layerId, config, 20, 30, 5, 15);

    controller.copySelection();

    QVERIFY(controller.hasClipboard());
    // Copy alone never logs an Operation - it's a read, not an edit.
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void SelectionControllerTest::copySelectionIsANoOpWithNoCommittedSelection() {
    Project project = Project::createNew(testSettings());
    addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);

    controller.copySelection();

    QVERIFY(!controller.hasClipboard());
}

void SelectionControllerTest::cutSelectionCopiesThenSilencesTheSourceRegion() {
    const auto config = testConfig();
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    setPixel(project, layerId, 25, 10, -3.0f);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    selectRect(controller, layerId, config, 20, 30, 5, 15);
    QSignalSpy contentSpy(&controller, &SelectionController::contentChanged);

    controller.cutSelection();

    QVERIFY(controller.hasClipboard());
    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(project.operationLog().size(), std::size_t{1});  // the silencing FillOperation.
    // The source region is now silent - well below the original -3dB.
    QVERIFY(readPixel(project, layerId, 25, 10) < -90.0f);

    // But the clipboard still holds the *original* (pre-clear) value -
    // cutSelection() copies before it clears. Pasting it right back
    // proves that.
    controller.pasteInto(layerId);
    QCOMPARE(readPixel(project, layerId, 25, 10), -3.0f);
}

void SelectionControllerTest::cutSelectionIsANoOpWithNoCommittedSelection() {
    Project project = Project::createNew(testSettings());
    addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);

    controller.cutSelection();

    QVERIFY(!controller.hasClipboard());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void SelectionControllerTest::pasteIntoWritesTheClipboardOntoTheGivenLayer() {
    const auto config = testConfig();
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    setPixel(project, layerId, 25, 10, -3.0f);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    selectRect(controller, layerId, config, 20, 30, 5, 15);
    controller.copySelection();
    controller.clearSelection();
    QSignalSpy contentSpy(&controller, &SelectionController::contentChanged);

    controller.pasteInto(layerId);

    QCOMPARE(contentSpy.count(), 1);
    QCOMPARE(contentSpy.takeFirst().at(0).value<LayerId>(), layerId);
    QCOMPARE(project.operationLog().size(), std::size_t{1});
    QCOMPARE(readPixel(project, layerId, 25, 10), -3.0f);
}

void SelectionControllerTest::pasteIntoCanTargetADifferentLayerThanItWasCopiedFrom() {
    const auto config = testConfig();
    Project project = Project::createNew(testSettings());
    const LayerId sourceLayerId = addBlankNormalLayer(project);
    const LayerId targetLayerId = addBlankNormalLayer(project);
    setPixel(project, sourceLayerId, 25, 10, -3.0f);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    selectRect(controller, sourceLayerId, config, 20, 30, 5, 15);

    controller.cutSelection();
    controller.pasteInto(targetLayerId);

    // Landed on the *target* layer, not back on the source.
    QCOMPARE(readPixel(project, targetLayerId, 25, 10), -3.0f);
    // The source layer's own region is still silenced (Cut's own clear),
    // not restored by pasting elsewhere.
    QVERIFY(readPixel(project, sourceLayerId, 25, 10) < -90.0f);

    const auto sourceActive = project.operationLog().activeOperationsTargeting(sourceLayerId);
    const auto targetActive = project.operationLog().activeOperationsTargeting(targetLayerId);
    QCOMPARE(sourceActive.size(), std::size_t{1});  // the Cut's own silencing FillOperation.
    QCOMPARE(targetActive.size(), std::size_t{1});  // the PasteOperation.
}

void SelectionControllerTest::pasteIntoIsANoOpWithNoClipboard() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);

    controller.pasteInto(layerId);

    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void SelectionControllerTest::pasteIntoUpdatesTheCommittedSelectionToThePastedRegion() {
    const auto config = testConfig();
    Project project = Project::createNew(testSettings());
    const LayerId sourceLayerId = addBlankNormalLayer(project);
    const LayerId targetLayerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    selectRect(controller, sourceLayerId, config, 20, 30, 5, 15);
    controller.copySelection();
    QSignalSpy boundsSpy(&controller, &SelectionController::boundsChanged);
    QSignalSpy selectionSpy(&controller, &SelectionController::selectionChanged);

    controller.pasteInto(targetLayerId);

    QCOMPARE(boundsSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);
    QVERIFY(controller.hasSelection());
    const auto bounds = *controller.displayBounds();
    QCOMPARE(bounds.startTimeSeconds, frameIndexToTime(20, config));
    QCOMPARE(bounds.endTimeSeconds, frameIndexToTime(30, config));
}

void SelectionControllerTest::setProjectClearsTheClipboardToo() {
    const auto config = testConfig();
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController paintController;
    paintController.setProject(&project);
    SelectionController controller(&paintController);
    controller.setProject(&project);
    selectRect(controller, layerId, config, 20, 30, 5, 15);
    controller.copySelection();
    QVERIFY(controller.hasClipboard());

    controller.setProject(nullptr);

    QVERIFY(!controller.hasClipboard());
}
