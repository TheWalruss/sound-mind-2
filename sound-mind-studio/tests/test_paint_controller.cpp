#include "test_paint_controller.h"

#include <algorithm>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::PaintController;

namespace {

/// @brief Small, fast project settings - matching test_main_window.cpp's
/// own smallCanvasProjectSettings(), plus a real, distinct frequency
/// range so frequencyToTimeScale() derives a sensible, checkable value.
ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;  // a round 2000 Hz range.
    settings.timestepMs = 10.0;         // 100 * 10ms = 1 second total duration.
    return settings;
}

/// @brief A Normal layer, added to `project`, with real (blank) content -
/// PaintController needs a layer with *some* content already (imported or
/// otherwise rendered) to capture as its own pre-paint base.
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

/// @brief A Procedural tool configuration with a real (non-transparent)
/// uniform gradient - a fresh ToolConfiguration's own default gradient is
/// deliberately fully transparent (see ToolConfiguration's own docs), so
/// any test that needs painting to actually leave a visible mark needs
/// one of these instead.
sound_mind::core::ToolConfiguration makeOpaqueTool(double size = 0.05, float falloff = 0.0f,
                                                    float intensity = -10.0f) {
    sound_mind::core::ToolConfiguration config;
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

}  // namespace

void PaintControllerTest::freshControllerHasNoStrokeInProgress() {
    const PaintController controller;
    QVERIFY(!controller.isStrokeInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
}

void PaintControllerTest::freshControllerCannotUndoOrRedo() {
    const PaintController controller;
    QVERIFY(!controller.canUndo());
    QVERIFY(!controller.canRedo());
}

void PaintControllerTest::beginStrokeDoesNothingWithNoProjectSet() {
    PaintController controller;
    controller.beginStroke(LayerId{1}, TimeFrequencyPoint{0.0, 100.0});
    QVERIFY(!controller.isStrokeInProgress());
}

void PaintControllerTest::beginStrokeStartsAStroke() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);

    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});

    QVERIFY(controller.isStrokeInProgress());
}

void PaintControllerTest::beginStrokeDoesNothingWhileAStrokeIsAlreadyInProgress() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});

    controller.beginStroke(layerId, TimeFrequencyPoint{0.2, 600.0});  // ignored - a stroke's already in progress.

    controller.continueStroke(TimeFrequencyPoint{0.3, 500.0});
    QVERIFY(controller.currentPreviewPath().nodes().front().anchor.timeSeconds == 0.1);
}

void PaintControllerTest::continueStrokeEmitsPathChangedOnceThereAreTwoPoints() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    QSignalSpy spy(&controller, &PaintController::pathChanged);

    controller.continueStroke(TimeFrequencyPoint{0.2, 500.0});

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.currentPreviewPath().nodes().empty());
}

void PaintControllerTest::continueStrokeDoesNothingWithNoStrokeInProgress() {
    PaintController controller;
    QSignalSpy spy(&controller, &PaintController::pathChanged);

    controller.continueStroke(TimeFrequencyPoint{0.1, 500.0});

    QCOMPARE(spy.count(), 0);
}

void PaintControllerTest::endStrokeAppendsAPaintOperationAndEmitsContentChanged() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    controller.continueStroke(TimeFrequencyPoint{0.5, 500.0});
    QSignalSpy spy(&controller, &PaintController::contentChanged);

    controller.endStroke();

    QVERIFY(!controller.isStrokeInProgress());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<LayerId>(), layerId);
    QCOMPARE(project.operationLog().size(), std::size_t{1});
}

void PaintControllerTest::endStrokeWithOnlyOnePointStillPaintsATap() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});

    controller.endStroke();

    QCOMPARE(project.operationLog().size(), std::size_t{1});
}

void PaintControllerTest::endStrokeActuallyChangesTheLayersStoredContent() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.setToolConfiguration(makeOpaqueTool());

    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    const auto& content = *project.layers().back().content();
    const bool anyPainted = std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(anyPainted);
}

void PaintControllerTest::cancelStrokeDiscardsTheStrokeWithoutAppendingAnOperation() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    controller.continueStroke(TimeFrequencyPoint{0.5, 500.0});

    controller.cancelStroke();

    QVERIFY(!controller.isStrokeInProgress());
    QVERIFY(controller.currentPreviewPath().nodes().empty());
    QCOMPARE(project.operationLog().size(), std::size_t{0});
}

void PaintControllerTest::undoRevertsTheLayersContentAndRedoReappliesIt() {
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    PaintController controller;
    controller.setProject(&project);
    controller.setToolConfiguration(makeOpaqueTool());
    controller.beginStroke(layerId, TimeFrequencyPoint{0.3, 500.0});
    controller.endStroke();

    const auto paintedContent = *project.layers().back().content();
    const bool wasPainted = std::any_of(paintedContent.leftMagnitudeDb.begin(), paintedContent.leftMagnitudeDb.end(),
                                         [](float value) { return value != 0.0f; });
    QVERIFY(wasPainted);

    QVERIFY(controller.canUndo());
    controller.undo();
    const auto undoneContent = *project.layers().back().content();
    QVERIFY(std::all_of(undoneContent.leftMagnitudeDb.begin(), undoneContent.leftMagnitudeDb.end(),
                         [](float value) { return value == 0.0f; }));

    QVERIFY(controller.canRedo());
    controller.redo();
    const auto redoneContent = *project.layers().back().content();
    QVERIFY(std::any_of(redoneContent.leftMagnitudeDb.begin(), redoneContent.leftMagnitudeDb.end(),
                         [](float value) { return value != 0.0f; }));
}

void PaintControllerTest::setProjectClearsAnyInProgressStroke() {
    Project firstProject = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(firstProject);
    PaintController controller;
    controller.setProject(&firstProject);
    controller.beginStroke(layerId, TimeFrequencyPoint{0.1, 500.0});
    QVERIFY(controller.isStrokeInProgress());

    Project secondProject = Project::createNew(testSettings());
    controller.setProject(&secondProject);

    QVERIFY(!controller.isStrokeInProgress());
}
