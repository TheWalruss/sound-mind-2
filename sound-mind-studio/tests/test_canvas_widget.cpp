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
    Project project = Project::createNew(ProjectSettings{});

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
