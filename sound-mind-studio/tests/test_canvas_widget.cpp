#include "test_canvas_widget.h"

#include <QtTest/QtTest>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::Layer;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::CanvasWidget;

void CanvasWidgetTest::sizeHintFallsBackWithNoProject() {
    const CanvasWidget widget;
    QVERIFY(widget.sizeHint().isValid());
}

void CanvasWidgetTest::sizeHintMatchesProjectCanvasDimensions() {
    ProjectSettings settings;
    settings.canvasWidth = 640;
    settings.canvasHeight = 480;
    const Project project = Project::createNew(settings);

    CanvasWidget widget;
    widget.setProject(&project);

    QCOMPARE(widget.sizeHint(), QSize(640, 480));
}

void CanvasWidgetTest::rendersALayersContentInsteadOfThePlaceholder() {
    // canvasWidth matches the content's own frameCount below - as of
    // v0.Y.21.1 (Layer Time Alignment), renderLayer() always pads/crops to
    // the project's canvasWidth, so a mismatch here would put the sampled
    // center pixel in black padding rather than the layer's own content.
    ProjectSettings settings;
    settings.canvasWidth = 2;
    Project project = Project::createNew(settings);

    // A deliberately distinctive, easy-to-check color: left = 0 dB (full
    // scale) -> red 255, right = -96 dB (the floor) -> green 0, phase = 0
    // -> a mid-range blue.
    StreamImage content;
    content.config.binCount = 2;
    content.frameCount = 2;
    content.leftMagnitudeDb.assign(4, 0.0f);
    content.rightMagnitudeDb.assign(4, -96.0f);
    content.sharedPhaseRadians.assign(4, 0.0f);

    Layer layer(0, "Imported", LayerType::Normal);
    layer.setContent(content);
    project.addLayer(std::move(layer));

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(20, 20);

    const QImage rendered = widget.grab().toImage();
    const QColor centerPixel = rendered.pixelColor(rendered.width() / 2, rendered.height() / 2);

    QCOMPARE(centerPixel.red(), 255);
    QCOMPARE(centerPixel.green(), 0);
    QVERIFY(centerPixel.blue() > 120 && centerPixel.blue() < 135);
}

void CanvasWidgetTest::skipsAHiddenTopmostLayerInFavorOfTheOneBelowIt() {
    // Per the Layers Panel milestone (v0.Y.13.1) - toggling a layer hidden
    // should actually change what's on screen, not just its own row icon.
    // canvasWidth matches both layers' frameCount below - see the same note
    // in rendersALayersContentInsteadOfThePlaceholder() above.
    ProjectSettings settings;
    settings.canvasWidth = 2;
    Project project = Project::createNew(settings);

    // Bottom (visible): full-scale red/no-green/mid-blue, same recipe as
    // rendersALayersContentInsteadOfThePlaceholder() above.
    StreamImage bottomContent;
    bottomContent.config.binCount = 2;
    bottomContent.frameCount = 2;
    bottomContent.leftMagnitudeDb.assign(4, 0.0f);
    bottomContent.rightMagnitudeDb.assign(4, -96.0f);
    bottomContent.sharedPhaseRadians.assign(4, 0.0f);
    Layer bottomLayer(0, "Bottom", LayerType::Normal);
    bottomLayer.setContent(bottomContent);
    project.addLayer(std::move(bottomLayer));

    // Top (hidden): the inverse recipe - no red, full-scale green - so a
    // wrong (unskipped) render is trivially distinguishable from a correct
    // (skipped-to-Bottom) one.
    StreamImage topContent;
    topContent.config.binCount = 2;
    topContent.frameCount = 2;
    topContent.leftMagnitudeDb.assign(4, -96.0f);
    topContent.rightMagnitudeDb.assign(4, 0.0f);
    topContent.sharedPhaseRadians.assign(4, 0.0f);
    Layer topLayer(0, "Top", LayerType::Normal);
    topLayer.setContent(topContent);
    topLayer.setVisible(false);
    project.addLayer(std::move(topLayer));

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(20, 20);

    const QImage rendered = widget.grab().toImage();
    const QColor centerPixel = rendered.pixelColor(rendered.width() / 2, rendered.height() / 2);

    QCOMPARE(centerPixel.red(), 255);
    QCOMPARE(centerPixel.green(), 0);
}

void CanvasWidgetTest::reflectsALayersTranslationColumns() {
    // v0.Y.21.1 (Layer Time Alignment): translationColumns() shifts a
    // layer's rendered content later in time - a wide-enough canvas with a
    // narrow, translated layer should show black at the untranslated start
    // and the layer's own color only past the shift.
    ProjectSettings settings;
    settings.canvasWidth = 4;
    Project project = Project::createNew(settings);

    StreamImage content;
    content.config.binCount = 1;
    content.frameCount = 2;
    content.leftMagnitudeDb.assign(2, 0.0f);
    content.rightMagnitudeDb.assign(2, -96.0f);
    content.sharedPhaseRadians.assign(2, 0.0f);

    Layer layer(0, "Imported", LayerType::Normal);
    layer.setContent(content);
    layer.setTranslationColumns(2);  // content now occupies canvas columns [2, 4).
    project.addLayer(std::move(layer));

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(4, 1);

    const QImage rendered = widget.grab().toImage();

    QCOMPARE(rendered.pixelColor(0, 0), QColor(0, 0, 0));
    QCOMPARE(rendered.pixelColor(3, 0).red(), 255);
}

void CanvasWidgetTest::drawsAPlayheadLineAtTheGivenFraction() {
    CanvasWidget widget;
    widget.resize(10, 10);
    widget.setPlayheadFraction(0.5);

    const QImage rendered = widget.grab().toImage();

    QCOMPARE(rendered.pixelColor(5, 5), QColor(255, 255, 255));
}

void CanvasWidgetTest::drawsNoPlayheadByDefault() {
    CanvasWidget widget;
    widget.resize(10, 10);

    const QImage rendered = widget.grab().toImage();

    // No project set -> a plain black canvas; no playhead means no white
    // line drawn anywhere over it.
    QCOMPARE(rendered.pixelColor(5, 5), QColor(0, 0, 0));
}
