#include "sound_mind/studio/canvas_widget.h"

#include <optional>

#include <QFontMetrics>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>

#include "sound_mind/core/compositor.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/qt_image_conversion.h"

namespace sound_mind::studio {

namespace {
const QSize kFallbackSize(400, 300);

/// @brief The last layer (top of the stack) with cached content, if any -
/// see CanvasWidget's docs for why "last with content" stands in for a
/// real composite for now.
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> findTopmostRender(const sound_mind::core::Project& project) {
    const auto& layers = project.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        // A layer hidden via the Layers Panel (v0.Y.13.1) is skipped here
        // too, same as MainWindow::topmostLayerWithContent() - this is a
        // second, independent "topmost layer" traversal (CanvasWidget
        // renders directly from the Project it's given, rather than going
        // through MainWindow), so it needs the same check applied
        // separately rather than inheriting it for free.
        if (!it->visible()) {
            continue;
        }
        if (auto rendered = sound_mind::core::renderLayer(*it, project.settings().canvasWidth); rendered.has_value()) {
            return rendered;
        }
    }
    return std::nullopt;
}

/// @brief The same "topmost visible layer with content" the image
/// findTopmostRender() above returns actually came from - as the Layer
/// itself, for drawOperationOverlays() to query its own id's operations
/// with. A second, independent lookup (same reasoning as findTopmostRender()'s
/// own docs) rather than having findTopmostRender() return both, to keep
/// its own return type (just the rendered image) unchanged for every
/// existing caller.
[[nodiscard]] const sound_mind::core::Layer* findTopmostLayerWithContent(const sound_mind::core::Project& project) {
    const auto& layers = project.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (it->visible() && it->content().has_value()) {
            return &*it;
        }
    }
    return nullptr;
}

}  // namespace

CanvasWidget::CanvasWidget(QWidget* parent) : QWidget(parent) {
    // cursorMoved() needs mouseMoveEvent() to fire on every move, not just
    // while a button is held (QWidget's default) - see that signal's own
    // docs.
    setMouseTracking(true);
}

void CanvasWidget::setProject(const sound_mind::core::Project* project) {
    project_ = project;
    updateGeometry();
    update();
}

void CanvasWidget::setPlayheadFraction(std::optional<double> fraction) {
    playheadFraction_ = fraction;
    update();
}

void CanvasWidget::setToolMode(ToolMode mode) {
    toolMode_ = mode;
    paintStrokeActive_ = false;
    pickStrokeActive_ = false;
    selectStrokeActive_ = false;
}

void CanvasWidget::setPaintPreviewPath(sound_mind::core::Path path) {
    paintPreviewPath_ = std::move(path);
    update();
}

void CanvasWidget::setPreviewSelectedNodeIndex(std::optional<std::size_t> index) {
    previewSelectedNodeIndex_ = index;
    update();
}

void CanvasWidget::setPickSelectionBounds(std::optional<sound_mind::core::TimeFrequencyRect> bounds) {
    pickSelectionBounds_ = bounds;
    update();
}

void CanvasWidget::setSelectionBounds(std::optional<sound_mind::core::TimeFrequencyRect> bounds) {
    selectionBounds_ = bounds;
    update();
}

void CanvasWidget::setVerticalAxisLabelMode(VerticalAxisLabelMode mode) {
    verticalAxisLabelMode_ = mode;
    update();
}

void CanvasWidget::setHorizontalAxisLabelMode(HorizontalAxisLabelMode mode) {
    horizontalAxisLabelMode_ = mode;
    update();
}

void CanvasWidget::setShowBoundingBoxes(bool shown) {
    showBoundingBoxes_ = shown;
    update();
}

void CanvasWidget::setShowPathGeometry(bool shown) {
    showPathGeometry_ = shown;
    update();
}

QSize CanvasWidget::sizeHint() const {
    if (project_ == nullptr) {
        return kFallbackSize;
    }
    return QSize(static_cast<int>(project_->settings().canvasWidth),
                 static_cast<int>(project_->settings().canvasHeight));
}

void CanvasWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (project_ != nullptr) {
        if (const auto rendered = findTopmostRender(*project_); rendered.has_value()) {
            painter.drawImage(rect(), toQImageView(*rendered));
        } else {
            // No layer has any content yet - fall back to the placeholder
            // that stood in for the whole canvas before Import existed.
            const QRect canvasRect(0, 0, static_cast<int>(project_->settings().canvasWidth),
                                    static_cast<int>(project_->settings().canvasHeight));
            painter.fillRect(canvasRect.intersected(rect()), QColor(40, 40, 40));
            painter.setPen(Qt::darkGray);
            painter.drawRect(canvasRect.adjusted(0, 0, -1, -1));
        }

        if (showBoundingBoxes_ || showPathGeometry_) {
            drawOperationOverlays(painter);
        }
    }

    // The playhead (v0.0.21.1, Playback position bar) is drawn last, over
    // whatever the canvas otherwise shows - independent of project_/layer
    // state, matching video export's own playhead line
    // (sound_mind::codec::exportVideo()) so live playback and an exported
    // video look the same.
    if (playheadFraction_.has_value()) {
        const int x = static_cast<int>(*playheadFraction_ * rect().width());
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(x, 0, x, rect().height());
    }

    // The live paint-stroke preview (v0.Y.24.1, Basic Painting) - see
    // setPaintPreviewPath()'s own docs. Always drawn, regardless of Show
    // path geometry's own setting - see that method's own docs for why.
    //
    // Drawn as a black-outlined white line, not a single fixed color - a
    // path being painted or edited over a matching painted color (yellow
    // over yellow, say) used to disappear entirely. No single fixed color
    // survives an arbitrary painted background, but white-with-a-black-
    // outline does: nothing is simultaneously black and white, so at
    // least one of the two always contrasts, on any background color or
    // pattern - the same "halo" technique drawPreviewPathNodes() below
    // uses for the nodes/handles themselves.
    if (project_ != nullptr && !paintPreviewPath_.nodes().empty()) {
        const QPainterPath previewPath = toPainterPath(paintPreviewPath_);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawPath(previewPath);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawPath(previewPath);
        drawPreviewPathNodes(painter);
    }

    // The Picked object's own selection highlight (Pick) - see
    // setPickSelectionBounds()'s own docs. Drawn when set, regardless of
    // Show bounding boxes' own setting, for the same reason the live
    // paint preview above always draws regardless of Show path geometry -
    // *except* while a live preview is itself showing (a whole-object
    // move, or a path edit session): setPickSelectionBounds() only ever
    // updates on selectionChanged(), which a move/edit-in-progress
    // doesn't emit, so this rectangle would otherwise sit frozen at the
    // picked object's own *pre*-drag position for the entire gesture,
    // visibly out of step with the live preview actually tracking the
    // drag (reported as the bounding box's own top corners "not updated
    // properly when moved" - true of the whole box, but the top edge is
    // the one most visibly wrong while dragging down and to the right).
    // The live preview itself already shows the object's current extent
    // more accurately than this rectangle ever could mid-drag.
    if (project_ != nullptr && pickSelectionBounds_.has_value() && paintPreviewPath_.nodes().empty()) {
        painter.setPen(QPen(Qt::white, 2));
        painter.drawRect(widgetRectFor(*pickSelectionBounds_));
    }

    // The current rectangular selection (Select) - see
    // setSelectionBounds()'s own docs. Independent of toolMode(), the
    // same "a selection stays visible/usable after switching tools"
    // reasoning that method's own docs describe.
    if (project_ != nullptr && selectionBounds_.has_value()) {
        painter.setPen(QPen(Qt::green, 2));
        painter.drawRect(widgetRectFor(*selectionBounds_));
    }

    // Axis Labels (see setVerticalAxisLabelMode()'s/
    // setHorizontalAxisLabelMode()'s own docs) - drawn last, over
    // everything else, the same "always legible" precedent the playhead
    // and every Pick/Selection highlight above already established.
    if (project_ != nullptr) {
        drawAxisLabels(painter);
    }
}

void CanvasWidget::drawAxisLabels(QPainter& painter) const {
    // How far a tick mark extends from the canvas's own edge, and how
    // much breathing room its own label text gets around it - small
    // enough not to eat into the canvas itself, big enough to read.
    constexpr int kTickLength = 4;
    constexpr int kTextPadding = 2;
    // A translucent backing behind each label - like the halo technique
    // drawPreviewPathNodes() uses for node dots, but simpler for text:
    // a dark backing plate reads clearly over any painted color or
    // pattern, without needing a second color to cover the opposite
    // case.
    const QColor kLabelBackground(0, 0, 0, 170);

    const auto& settings = project_->settings();
    const QFontMetrics metrics(painter.font());

    if (verticalAxisLabelMode_ != VerticalAxisLabelMode::Off) {
        const auto ticks = verticalAxisTicks(verticalAxisLabelMode_, settings, rect().height());
        for (const auto& tick : ticks) {
            const double y =
                timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{0.0, tick.domainValue}).y();
            painter.setPen(QPen(Qt::lightGray, 1));
            painter.drawLine(QPointF(0.0, y), QPointF(kTickLength, y));

            const QRect textBounds = metrics.boundingRect(tick.label);
            const QRectF backing(kTickLength + kTextPadding, y - textBounds.height() / 2.0 - kTextPadding,
                                   textBounds.width() + 2 * kTextPadding, textBounds.height() + 2 * kTextPadding);
            painter.fillRect(backing, kLabelBackground);
            painter.setPen(Qt::lightGray);
            painter.drawText(QPointF(backing.left() + kTextPadding, y + textBounds.height() / 2.0 - metrics.descent()),
                              tick.label);
        }
    }

    if (horizontalAxisLabelMode_ != HorizontalAxisLabelMode::Off) {
        const auto ticks = horizontalAxisTicks(horizontalAxisLabelMode_, settings, rect().width());
        const double bottom = rect().height();
        for (const auto& tick : ticks) {
            const double x =
                timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{tick.domainValue, 0.0}).x();
            painter.setPen(QPen(Qt::lightGray, 1));
            painter.drawLine(QPointF(x, bottom), QPointF(x, bottom - kTickLength));

            const QRect textBounds = metrics.boundingRect(tick.label);
            const QRectF backing(x + kTextPadding, bottom - kTickLength - kTextPadding - textBounds.height(),
                                   textBounds.width() + 2 * kTextPadding, textBounds.height() + 2 * kTextPadding);
            painter.fillRect(backing, kLabelBackground);
            painter.setPen(Qt::lightGray);
            painter.drawText(QPointF(backing.left() + kTextPadding, backing.bottom() - kTextPadding - metrics.descent()),
                              tick.label);
        }
    }
}

void CanvasWidget::drawOperationOverlays(QPainter& painter) const {
    const sound_mind::core::Layer* layer = findTopmostLayerWithContent(*project_);
    if (layer == nullptr) {
        return;
    }
    const auto operations = project_->operationLog().activeOperationsTargeting(layer->id());

    if (showBoundingBoxes_) {
        painter.setPen(QPen(Qt::cyan, 1));
        for (const sound_mind::core::Operation* operation : operations) {
            painter.drawRect(widgetRectFor(operation->bounds()));
        }
    }

    if (showPathGeometry_) {
        painter.setPen(QPen(Qt::magenta, 1));
        for (const sound_mind::core::Operation* operation : operations) {
            if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(operation)) {
                painter.drawPath(toPainterPath(paint->path()));
            }
        }
    }
}

void CanvasWidget::drawPreviewPathNodes(QPainter& painter) const {
    constexpr double kNodeRadius = 3.0;
    constexpr double kSelectedNodeRadius = 5.0;
    constexpr double kHandleRadius = 3.0;
    // How much wider/larger the black halo is drawn than the colored
    // shape it sits behind - see this method's own docs, and
    // paintEvent()'s matching treatment of the path line itself.
    constexpr double kHaloExtra = 1.5;

    // Draws `radius`-sized dot at `point` in `color`, behind a slightly
    // larger solid black one - so it stays visible even directly over a
    // painted region the same color as the dot itself.
    const auto drawHaloDot = [&painter](QPointF point, double radius, const QColor& color) {
        painter.setPen(QPen(Qt::black, 1));
        painter.setBrush(Qt::black);
        painter.drawEllipse(point, radius + kHaloExtra, radius + kHaloExtra);
        painter.setPen(QPen(color, 1));
        painter.setBrush(color);
        painter.drawEllipse(point, radius, radius);
        painter.setBrush(Qt::NoBrush);
    };
    // Draws a line from `from` to `to` in `color`, behind a wider solid
    // black one, for the same reason.
    const auto drawHaloLine = [&painter](QPointF from, QPointF to, const QColor& color) {
        painter.setPen(QPen(Qt::black, 3));
        painter.drawLine(from, to);
        painter.setPen(QPen(color, 1));
        painter.drawLine(from, to);
    };

    const auto& nodes = paintPreviewPath_.nodes();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const bool selected = previewSelectedNodeIndex_.has_value() && *previewSelectedNodeIndex_ == i;
        const QPointF anchorPoint = timeFrequencyToWidgetPoint(nodes[i].anchor);

        // The node's own anchor - drawn *before* its own handles below,
        // not after: a freshly-smoothed node's handles start collapsed
        // exactly onto its own anchor (see PathNode's own docs) and stay
        // there until dragged out, so if the anchor drew on top it would
        // fully hide them - the only visible sign Toggle Node Type did
        // anything would be a Corner node's already-absent handles simply
        // staying absent, indistinguishable from nothing having happened.
        drawHaloDot(anchorPoint, selected ? kSelectedNodeRadius : kNodeRadius, selected ? Qt::white : Qt::yellow);

        // Handles - only for the selected node, and only if it's Smooth -
        // see setPreviewSelectedNodeIndex()'s own docs for why.
        if (selected && nodes[i].type == sound_mind::core::PathNodeType::Smooth) {
            if (nodes[i].handleOut.has_value()) {
                const QPointF handlePoint = timeFrequencyToWidgetPoint(*nodes[i].handleOut);
                drawHaloLine(anchorPoint, handlePoint, Qt::cyan);
                drawHaloDot(handlePoint, kHandleRadius, Qt::cyan);
            }
            if (nodes[i].handleIn.has_value()) {
                const QPointF handlePoint = timeFrequencyToWidgetPoint(*nodes[i].handleIn);
                drawHaloLine(anchorPoint, handlePoint, Qt::cyan);
                drawHaloDot(handlePoint, kHandleRadius, Qt::cyan);
            }
        }
    }
}

QPainterPath CanvasWidget::toPainterPath(const sound_mind::core::Path& path) const {
    QPainterPath qPath;
    const auto& nodes = path.nodes();
    if (nodes.empty()) {
        return qPath;
    }
    qPath.moveTo(timeFrequencyToWidgetPoint(nodes.front().anchor));
    for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
        const auto& start = nodes[i];
        const auto& end = nodes[i + 1];
        const QPointF p1 = timeFrequencyToWidgetPoint(start.handleOut.value_or(start.anchor));
        const QPointF p2 = timeFrequencyToWidgetPoint(end.handleIn.value_or(end.anchor));
        qPath.cubicTo(p1, p2, timeFrequencyToWidgetPoint(end.anchor));
    }
    return qPath;
}

QRectF CanvasWidget::widgetRectFor(const sound_mind::core::TimeFrequencyRect& bounds) const {
    // .normalized() guards against bin 0 mapping to the *top* of the
    // widget (see widgetPointToTimeFrequency()'s own docs) -
    // highFrequencyHz's own corner lands at a numerically *larger* y than
    // lowFrequencyHz's, the opposite of what a plain QRectF(topLeft,
    // bottomRight) construction assumes.
    const QPointF corner1 = timeFrequencyToWidgetPoint(
        sound_mind::core::TimeFrequencyPoint{bounds.startTimeSeconds, bounds.highFrequencyHz});
    const QPointF corner2 = timeFrequencyToWidgetPoint(
        sound_mind::core::TimeFrequencyPoint{bounds.endTimeSeconds, bounds.lowFrequencyHz});
    return QRectF(corner1, corner2).normalized();
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton ||
        (toolMode_ != ToolMode::Paint && toolMode_ != ToolMode::Pick && toolMode_ != ToolMode::Select &&
         toolMode_ != ToolMode::Path)) {
        QWidget::mousePressEvent(event);
        return;
    }
    const auto point = widgetPointToTimeFrequency(event->position());
    if (!point.has_value()) {
        return;
    }
    if (toolMode_ == ToolMode::Paint) {
        paintStrokeActive_ = true;
        emit paintStrokeStarted(*point);
    } else if (toolMode_ == ToolMode::Pick) {
        pickStrokeActive_ = true;
        emit pickStrokeStarted(*point);
    } else if (toolMode_ == ToolMode::Select) {
        selectStrokeActive_ = true;
        emit selectStrokeStarted(*point);
    } else {
        // Path mode: no "active" bookkeeping - see pathNodePlaced()'s own
        // docs for why a single press is the whole gesture.
        emit pathNodePlaced(*point);
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    // Independent of toolMode()/paintStrokeActive_/pickStrokeActive_/
    // selectStrokeActive_ - see cursorMoved()'s own docs; every move gets
    // a position readout, not just ones that also continue an in-progress
    // stroke/drag.
    emit cursorMoved(event->position(), widgetPointToTimeFrequency(event->position()));

    if (toolMode_ == ToolMode::Paint && paintStrokeActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit paintStrokeContinued(*point);
        }
        return;
    }
    if (toolMode_ == ToolMode::Pick && pickStrokeActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit pickStrokeContinued(*point);
        }
        return;
    }
    if (toolMode_ == ToolMode::Select && selectStrokeActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit selectStrokeContinued(*point);
        }
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void CanvasWidget::leaveEvent(QEvent* /*event*/) { emit cursorLeft(); }

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (toolMode_ == ToolMode::Paint && paintStrokeActive_) {
        paintStrokeActive_ = false;
        emit paintStrokeEnded();
        return;
    }
    if (toolMode_ == ToolMode::Pick && pickStrokeActive_) {
        pickStrokeActive_ = false;
        emit pickStrokeEnded();
        return;
    }
    if (toolMode_ == ToolMode::Select && selectStrokeActive_) {
        selectStrokeActive_ = false;
        emit selectStrokeEnded();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

std::optional<sound_mind::core::TimeFrequencyPoint> CanvasWidget::widgetPointToTimeFrequency(QPointF point) const {
    if (project_ == nullptr || rect().width() <= 0 || rect().height() <= 0) {
        return std::nullopt;
    }
    const auto& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    // Bin index rises bottom-to-top on screen, not top-to-bottom - see
    // this method's own docs (and color_mapping.cpp's toRgbImage(), the
    // actual rendered image's "row 0 = highest frequency"/highest bin
    // convention this has to match). Flipping here, rather than changing
    // frequencyToBinIndex()/binIndexToFrequency() themselves, keeps those
    // shared with paint_application.cpp's own frequency-domain math,
    // which has no notion of screen pixels at all.
    const double frameIndex = point.x() * static_cast<double>(settings.canvasWidth) / rect().width();
    const double binIndex =
        static_cast<double>(settings.binCount) - (point.y() * static_cast<double>(settings.binCount) / rect().height());

    sound_mind::core::TimeFrequencyPoint result;
    result.timeSeconds = sound_mind::core::frameIndexToTime(frameIndex, config);
    result.frequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(binIndex), config);
    return result;
}

QPointF CanvasWidget::timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint point) const {
    if (project_ == nullptr) {
        return QPointF(0.0, 0.0);
    }
    const auto& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    const double frameIndex = sound_mind::core::timeToFrameIndex(point.timeSeconds, config);
    const double binIndex = sound_mind::core::frequencyToBinIndex(static_cast<float>(point.frequencyHz), config);

    // The exact inverse of widgetPointToTimeFrequency()'s own flip - see
    // its comment above.
    const double x = frameIndex * rect().width() / static_cast<double>(settings.canvasWidth);
    const double y =
        (static_cast<double>(settings.binCount) - binIndex) * rect().height() / static_cast<double>(settings.binCount);
    return QPointF(x, y);
}

}  // namespace sound_mind::studio
