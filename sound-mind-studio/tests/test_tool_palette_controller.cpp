#include "test_tool_palette_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/tool_configuration_panel.h"
#include "sound_mind/studio/tool_palette_controller.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::CanvasWidget;
using sound_mind::studio::ToolConfigurationPanel;
using sound_mind::studio::ToolPaletteController;

namespace {

/// @brief Small, fast project settings - matching test_paint_controller.cpp's
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
/// matching test_paint_controller.cpp's own addBlankNormalLayer().
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

void ToolPaletteControllerTest::freshControllerHasNoSelection() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    const ToolPaletteController controller(&canvas, &panel);
    QVERIFY(!controller.hasSelection());
}

void ToolPaletteControllerTest::setProjectDoesNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());

    controller.setProject(&project);
    controller.setProject(nullptr);
}

void ToolPaletteControllerTest::beginAndCancelEachToolDoesNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    controller.setProject(&project);

    const TimeFrequencyPoint point{0.1, 500.0f};
    controller.beginPaintStroke(layerId, point);
    controller.cancelPaintStroke();
    controller.beginPick(layerId, point);
    controller.clearPickSelection();
    controller.beginSelectionDrag(layerId, point);
    controller.cancelSelectionDrag();
    controller.placePathNode(layerId, point);
    controller.cancelPathPlacement();
}

void ToolPaletteControllerTest::undoAndRedoOnEmptyHistoryDoNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    controller.setProject(&project);

    controller.undo();
    controller.redo();
}

void ToolPaletteControllerTest::setPathPlacesSmoothNodesDoesNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);

    controller.setPathPlacesSmoothNodes(true);
    controller.setPathPlacesSmoothNodes(false);
}

void ToolPaletteControllerTest::setGridSnappingDoesNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);

    controller.setGridSnapping(true, sound_mind::studio::FrequencyGridConfig{}, sound_mind::studio::TimingGridConfig{});
    controller.setGridSnapping(false, sound_mind::studio::FrequencyGridConfig{}, sound_mind::studio::TimingGridConfig{});
}

void ToolPaletteControllerTest::pasteIntoWithNothingCopiedReturnsNullopt() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    controller.setProject(&project);

    QVERIFY(!controller.pasteInto(layerId).has_value());
}

void ToolPaletteControllerTest::copyAndCutWithNoSelectionDoNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    controller.setProject(&project);

    controller.copySelection();
    controller.cutSelection();
}

void ToolPaletteControllerTest::fillWithNoSelectionDoesNotCrash() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    controller.setProject(&project);

    controller.fill(sound_mind::core::Gradient{});
}

void ToolPaletteControllerTest::applyFilterToSelectionWithNoSelectionDoesNotCrash() {
    // v0.Y.46.1 Installment E ("Apply Filter to Selection").
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    controller.setProject(&project);

    controller.applyFilterToSelection(sound_mind::core::FilterConfiguration{});
}

void ToolPaletteControllerTest::contentChangedAggregatesAPaintStroke() {
    CanvasWidget canvas;
    ToolConfigurationPanel panel;
    ToolPaletteController controller(&canvas, &panel);
    Project project = Project::createNew(testSettings());
    const LayerId layerId = addBlankNormalLayer(project);
    controller.setProject(&project);

    QSignalSpy spy(&controller, &ToolPaletteController::contentChanged);

    // A single-point stroke (a tap) is enough to append a PaintOperation
    // and rebuild the target layer's own content - see PaintController::
    // beginStroke()'s own docs. paintStrokeStarted isn't wired internally
    // (see this class's own docs), so the stroke starts via
    // beginPaintStroke() directly; "ended" is wired internally to
    // canvas_'s own signal, driven here the same way a real mouse release
    // would.
    controller.beginPaintStroke(layerId, TimeFrequencyPoint{0.1, 500.0f});
    emit canvas.paintStrokeEnded();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<LayerId>(spy.at(0).at(0)), layerId);
}
