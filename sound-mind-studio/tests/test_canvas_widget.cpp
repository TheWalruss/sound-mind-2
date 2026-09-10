#include "test_canvas_widget.h"

#include <optional>

#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/canvas_widget.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::Layer;
using sound_mind::core::LayerType;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::ToolConfiguration;
using sound_mind::studio::CanvasWidget;

namespace {

/// @brief A project whose canvas is exactly 100x50, with a real,
/// distinct frequency range (20-2020 Hz) - matching
/// test_paint_controller.cpp's own testSettings(), so a widget resized
/// to this exact pixel size maps widget pixels to frame/bin indices 1:1,
/// keeping expected mouse-to-domain conversions simple to state.
ProjectSettings mouseConversionTestSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;  // 100 columns * 10ms = 1 second total duration.
    return settings;
}

}  // namespace

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

void CanvasWidgetTest::toolModeDefaultsToNone() {
    const CanvasWidget widget;
    QCOMPARE(widget.toolMode(), CanvasWidget::ToolMode::None);
}

void CanvasWidgetTest::setToolModeChangesTheMode() {
    CanvasWidget widget;
    widget.setToolMode(CanvasWidget::ToolMode::Paint);
    QCOMPARE(widget.toolMode(), CanvasWidget::ToolMode::Paint);
}

void CanvasWidgetTest::mousePressDoesNothingInNoneMode() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    QSignalSpy spy(&widget, &CanvasWidget::paintStrokeStarted);

    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QCOMPARE(spy.count(), 0);
}

void CanvasWidgetTest::mousePressInPaintModeEmitsPaintStrokeStartedWithAConvertedPoint() {
    const ProjectSettings settings = mouseConversionTestSettings();
    const Project project = Project::createNew(settings);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Paint);

    std::optional<TimeFrequencyPoint> received;
    QObject::connect(&widget, &CanvasWidget::paintStrokeStarted,
                      [&](TimeFrequencyPoint point) { received = point; });

    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QVERIFY(received.has_value());
    const double expectedTime = sound_mind::core::frameIndexToTime(30.0, config);
    // Bin index rises bottom-to-top on screen (widget y=0 is the highest
    // bin) - see widgetPointToTimeFrequency()'s own docs - so a press at
    // widget y=10 out of a 50px-tall, 50-bin widget lands on bin
    // (50 - 10) = 40, not bin 10.
    const float expectedFrequency = sound_mind::core::binIndexToFrequency(40.0f, config);
    QVERIFY(qAbs(received->timeSeconds - expectedTime) < 0.01);
    QVERIFY(qAbs(received->frequencyHz - expectedFrequency) < 1.0);
}

void CanvasWidgetTest::mouseMoveWithoutAPriorPressDoesNothingInPaintMode() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Paint);
    QSignalSpy spy(&widget, &CanvasWidget::paintStrokeContinued);

    QTest::mouseMove(&widget, QPoint(40, 20));

    QCOMPARE(spy.count(), 0);
}

void CanvasWidgetTest::mouseMoveAfterPressEmitsPaintStrokeContinued() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Paint);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QSignalSpy spy(&widget, &CanvasWidget::paintStrokeContinued);

    QTest::mouseMove(&widget, QPoint(40, 20));

    QCOMPARE(spy.count(), 1);
}

void CanvasWidgetTest::mouseReleaseEmitsPaintStrokeEndedAndEndsTheStroke() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Paint);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QSignalSpy endedSpy(&widget, &CanvasWidget::paintStrokeEnded);

    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));
    QCOMPARE(endedSpy.count(), 1);

    // A further move, without a new press, shouldn't continue the
    // already-ended stroke.
    QSignalSpy continuedSpy(&widget, &CanvasWidget::paintStrokeContinued);
    QTest::mouseMove(&widget, QPoint(50, 30));
    QCOMPARE(continuedSpy.count(), 0);
}

void CanvasWidgetTest::changingToolModeAwayFromPaintCancelsAnyActiveStroke() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Paint);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    widget.setToolMode(CanvasWidget::ToolMode::None);
    widget.setToolMode(CanvasWidget::ToolMode::Paint);  // back to Paint, but the old stroke is gone.
    QSignalSpy spy(&widget, &CanvasWidget::paintStrokeContinued);

    QTest::mouseMove(&widget, QPoint(40, 20));

    QCOMPARE(spy.count(), 0);  // a move alone, with no fresh press, still doesn't continue anything.
}

void CanvasWidgetTest::setPaintPreviewPathDrawsItOverTheCanvas() {
    const ProjectSettings settings = mouseConversionTestSettings();
    const Project project = Project::createNew(settings);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);

    // A straight horizontal line (both nodes at the same frequency, plain
    // Corner nodes) - predictable to check: it should render at a fixed
    // row, spanning the columns between its own two endpoints.
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{sound_mind::core::frameIndexToTime(10.0, sound_mind::core::streamCodecConfigFor(settings)),
                                       sound_mind::core::binIndexToFrequency(25.0f, sound_mind::core::streamCodecConfigFor(settings))};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{sound_mind::core::frameIndexToTime(90.0, sound_mind::core::streamCodecConfigFor(settings)),
                                     start.anchor.frequencyHz};
    end.type = PathNodeType::Corner;
    path.addNode(end);

    widget.setPaintPreviewPath(path);

    const QImage rendered = widget.grab().toImage();
    QCOMPARE(rendered.pixelColor(50, 25), QColor(255, 255, 0));  // Qt::yellow.
}

void CanvasWidgetTest::setPaintPreviewPathWithNoNodesDrawsNothing() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);

    widget.setPaintPreviewPath(Path{});

    const QImage rendered = widget.grab().toImage();
    // The placeholder's own dark-gray fill, not yellow anywhere.
    QCOMPARE(rendered.pixelColor(50, 25), QColor(40, 40, 40));
}

namespace {

/// @brief A project (per mouseConversionTestSettings()) with one Normal
/// layer with real (blank) content, plus one real `PaintOperation`
/// already appended to its own OperationLog - a straight diagonal Path
/// from (frame 20, bin 10) to (frame 80, bin 40), so its bounding box has
/// real, non-degenerate width and height to check for.
///
/// @param project The project to populate.
/// @return The new layer's own id.
sound_mind::core::LayerId addLayerWithARealPaintOperation(Project& project) {
    Layer layer(0, "Test", LayerType::Normal);
    const auto& settings = project.settings();
    StreamImage content;
    content.config = sound_mind::core::streamCodecConfigFor(settings);
    content.frameCount = settings.canvasWidth;
    const std::size_t pixelCount = std::size_t{content.config.binCount} * content.frameCount;
    content.leftMagnitudeDb.assign(pixelCount, 0.0f);
    content.rightMagnitudeDb.assign(pixelCount, 0.0f);
    content.sharedPhaseRadians.assign(pixelCount, 0.0f);
    layer.setContent(content);
    const auto layerId = project.addLayer(std::move(layer));

    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{sound_mind::core::frameIndexToTime(20.0, config),
                                       sound_mind::core::binIndexToFrequency(10.0f, config)};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{sound_mind::core::frameIndexToTime(80.0, config),
                                     sound_mind::core::binIndexToFrequency(40.0f, config)};
    end.type = PathNodeType::Corner;
    path.addNode(end);

    const auto opId = project.operationLog().reserveId();
    project.operationLog().append(std::make_unique<PaintOperation>(opId, layerId, path, ToolConfiguration{}));
    return layerId;
}

}  // namespace

void CanvasWidgetTest::showBoundingBoxesDrawsNothingWhenOff() {
    Project project = Project::createNew(mouseConversionTestSettings());
    addLayerWithARealPaintOperation(project);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);

    const QImage rendered = widget.grab().toImage();
    // Not the overlay's own cyan - whatever color the layer's own 0 dB
    // content renders as (full-scale, not silent - see
    // rendersALayersContentInsteadOfThePlaceholder()'s own comment for
    // why "blank" content isn't literally black here) doesn't matter, as
    // long as it isn't the bounding-box overlay's own distinctive color.
    QVERIFY(rendered.pixelColor(20, 10) != QColor(0, 255, 255));
}

void CanvasWidgetTest::showBoundingBoxesDrawsAnActiveOperationsBoundingBox() {
    Project project = Project::createNew(mouseConversionTestSettings());
    addLayerWithARealPaintOperation(project);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setShowBoundingBoxes(true);

    const QImage rendered = widget.grab().toImage();
    QCOMPARE(rendered.pixelColor(20, 10), QColor(0, 255, 255));  // Qt::cyan - the box's own top-left corner.
}

void CanvasWidgetTest::showPathGeometryDrawsAnActiveOperationsPath() {
    Project project = Project::createNew(mouseConversionTestSettings());
    addLayerWithARealPaintOperation(project);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setShowPathGeometry(true);

    const QImage rendered = widget.grab().toImage();
    // The path's own start node, at (frame 20, bin 10) - bin 10 is a *low*
    // bin (near the low-frequency end), which renders near the *bottom* of
    // the widget (widget y = (binCount - bin) * height / binCount = 40),
    // not near the top - see widgetPointToTimeFrequency()'s own docs for
    // why screen y and bin index move in opposite directions. Checked as
    // "somewhere in the 3x3 neighborhood", not the exact pixel: unlike the
    // bounding box's axis-aligned edges above (which land on an exact
    // pixel reliably), a diagonal line's own endpoint is a sub-pixel
    // rasterization/antialiasing rounding call - confirmed empirically to
    // land 1px off (at (20, 39), not (20, 40)) for this exact geometry.
    bool foundNearby = false;
    for (int dy = -1; dy <= 1 && !foundNearby; ++dy) {
        for (int dx = -1; dx <= 1 && !foundNearby; ++dx) {
            if (rendered.pixelColor(20 + dx, 40 + dy) == QColor(255, 0, 255)) {
                foundNearby = true;
            }
        }
    }
    QVERIFY(foundNearby);
}

void CanvasWidgetTest::mouseMoveEmitsCursorMovedRegardlessOfToolMode() {
    const ProjectSettings settings = mouseConversionTestSettings();
    const Project project = Project::createNew(settings);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    // toolMode() stays None (the default) - cursorMoved() must still fire,
    // unlike paintStrokeContinued(), which requires Paint mode and an
    // active stroke - see cursorMoved()'s own docs.
    QCOMPARE(widget.toolMode(), CanvasWidget::ToolMode::None);

    std::optional<QPointF> receivedPixel;
    std::optional<TimeFrequencyPoint> receivedDomain;
    QObject::connect(&widget, &CanvasWidget::cursorMoved, [&](QPointF pixel, std::optional<TimeFrequencyPoint> domain) {
        receivedPixel = pixel;
        receivedDomain = domain;
    });

    // A hand-built QMouseEvent, sent directly to the widget, rather than
    // QTest::mouseMove(): with no button held, QTest::mouseMove() routes
    // through real window-under-cursor resolution, which has nothing to
    // resolve to in this headless suite's own established convention of
    // never calling show() - a mousePress-then-move works instead only
    // because the press implicitly grabs the mouse for that widget.
    // sendEvent() bypasses that routing entirely.
    QMouseEvent moveEvent(QEvent::MouseMove, QPointF(30, 10), widget.mapToGlobal(QPoint(30, 10)), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &moveEvent);

    QVERIFY(receivedPixel.has_value());
    QCOMPARE(receivedPixel->toPoint(), QPoint(30, 10));
    QVERIFY(receivedDomain.has_value());
    const double expectedTime = sound_mind::core::frameIndexToTime(30.0, config);
    // Bin index rises bottom-to-top on screen - see
    // widgetPointToTimeFrequency()'s own docs - so widget y=10 (out of a
    // 50px-tall, 50-bin widget) is bin (50 - 10) = 40, not bin 10.
    const float expectedFrequency = sound_mind::core::binIndexToFrequency(40.0f, config);
    QVERIFY(qAbs(receivedDomain->timeSeconds - expectedTime) < 0.01);
    QVERIFY(qAbs(receivedDomain->frequencyHz - expectedFrequency) < 1.0);
}

void CanvasWidgetTest::leavingTheCanvasEmitsCursorLeft() {
    CanvasWidget widget;
    QSignalSpy spy(&widget, &CanvasWidget::cursorLeft);

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(&widget, &leaveEvent);

    QCOMPARE(spy.count(), 1);
}

void CanvasWidgetTest::mousePressInPickModeEmitsPickStrokeStartedWithAConvertedPoint() {
    const ProjectSettings settings = mouseConversionTestSettings();
    const Project project = Project::createNew(settings);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Pick);

    std::optional<TimeFrequencyPoint> received;
    QObject::connect(&widget, &CanvasWidget::pickStrokeStarted, [&](TimeFrequencyPoint point) { received = point; });

    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    QVERIFY(received.has_value());
    const double expectedTime = sound_mind::core::frameIndexToTime(30.0, config);
    const float expectedFrequency = sound_mind::core::binIndexToFrequency(40.0f, config);  // see the Paint-mode test's own comment.
    QVERIFY(qAbs(received->timeSeconds - expectedTime) < 0.01);
    QVERIFY(qAbs(received->frequencyHz - expectedFrequency) < 1.0);
}

void CanvasWidgetTest::mouseMoveAfterPressInPickModeEmitsPickStrokeContinued() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Pick);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QSignalSpy spy(&widget, &CanvasWidget::pickStrokeContinued);

    QTest::mouseMove(&widget, QPoint(40, 20));

    QCOMPARE(spy.count(), 1);
}

void CanvasWidgetTest::mouseReleaseInPickModeEmitsPickStrokeEndedAndEndsTheGesture() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Pick);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));
    QSignalSpy endedSpy(&widget, &CanvasWidget::pickStrokeEnded);

    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(40, 20));
    QCOMPARE(endedSpy.count(), 1);

    // A further move, without a new press, shouldn't continue the
    // already-ended gesture.
    QSignalSpy continuedSpy(&widget, &CanvasWidget::pickStrokeContinued);
    QTest::mouseMove(&widget, QPoint(50, 30));
    QCOMPARE(continuedSpy.count(), 0);
}

void CanvasWidgetTest::changingToolModeAwayFromPickCancelsAnyActiveGesture() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);
    widget.setToolMode(CanvasWidget::ToolMode::Pick);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(30, 10));

    widget.setToolMode(CanvasWidget::ToolMode::None);
    widget.setToolMode(CanvasWidget::ToolMode::Pick);  // back to Pick, but the old gesture is gone.
    QSignalSpy spy(&widget, &CanvasWidget::pickStrokeContinued);

    QTest::mouseMove(&widget, QPoint(40, 20));

    QCOMPARE(spy.count(), 0);  // a move alone, with no fresh press, still doesn't continue anything.
}

void CanvasWidgetTest::setPickSelectionBoundsDrawsAHighlight() {
    const ProjectSettings settings = mouseConversionTestSettings();
    const Project project = Project::createNew(settings);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);

    sound_mind::core::TimeFrequencyRect bounds;
    bounds.startTimeSeconds = sound_mind::core::frameIndexToTime(20.0, config);
    bounds.endTimeSeconds = sound_mind::core::frameIndexToTime(80.0, config);
    bounds.lowFrequencyHz = sound_mind::core::binIndexToFrequency(10.0f, config);
    bounds.highFrequencyHz = sound_mind::core::binIndexToFrequency(40.0f, config);
    widget.setPickSelectionBounds(bounds);

    const QImage rendered = widget.grab().toImage();
    // The highlight's own top-left corner - highFrequencyHz (bin 40) is
    // the *smaller* y (near the top) - see widgetPointToTimeFrequency()'s
    // own docs.
    QCOMPARE(rendered.pixelColor(20, 10), QColor(255, 255, 255));  // Qt::white.
}

void CanvasWidgetTest::setPickSelectionBoundsWithNoValueDrawsNothing() {
    const Project project = Project::createNew(mouseConversionTestSettings());
    CanvasWidget widget;
    widget.setProject(&project);
    widget.resize(100, 50);

    widget.setPickSelectionBounds(std::nullopt);

    const QImage rendered = widget.grab().toImage();
    QVERIFY(rendered.pixelColor(20, 10) != QColor(255, 255, 255));
}
