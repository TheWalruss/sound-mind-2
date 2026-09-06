#include "test_canvas_widget.h"

#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"

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
