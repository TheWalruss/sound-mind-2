#include "sound_mind/studio/canvas_widget.h"

#include <optional>

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
}

void CanvasWidget::setPaintPreviewPath(sound_mind::core::Path path) {
    paintPreviewPath_ = std::move(path);
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
    if (project_ != nullptr && !paintPreviewPath_.nodes().empty()) {
        painter.setPen(QPen(Qt::yellow, 1));
        painter.drawPath(toPainterPath(paintPreviewPath_));
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
            const auto bounds = operation->bounds();
            // .normalized() guards against bin 0 mapping to the *top* of
            // the widget (see widgetPointToTimeFrequency()'s own docs) -
            // highFrequencyHz's own corner lands at a numerically *larger*
            // y than lowFrequencyHz's, the opposite of what a plain
            // QRectF(topLeft, bottomRight) construction assumes.
            const QPointF corner1 = timeFrequencyToWidgetPoint(
                sound_mind::core::TimeFrequencyPoint{bounds.startTimeSeconds, bounds.highFrequencyHz});
            const QPointF corner2 = timeFrequencyToWidgetPoint(
                sound_mind::core::TimeFrequencyPoint{bounds.endTimeSeconds, bounds.lowFrequencyHz});
            painter.drawRect(QRectF(corner1, corner2).normalized());
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

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (toolMode_ != ToolMode::Paint || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
        paintStrokeActive_ = true;
        emit paintStrokeStarted(*point);
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    // Independent of toolMode()/paintStrokeActive_ - see cursorMoved()'s
    // own docs; every move gets a position readout, not just ones that
    // also continue an in-progress stroke.
    emit cursorMoved(event->position(), widgetPointToTimeFrequency(event->position()));

    if (toolMode_ != ToolMode::Paint || !paintStrokeActive_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
        emit paintStrokeContinued(*point);
    }
}

void CanvasWidget::leaveEvent(QEvent* /*event*/) { emit cursorLeft(); }

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (toolMode_ != ToolMode::Paint || !paintStrokeActive_ || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    paintStrokeActive_ = false;
    emit paintStrokeEnded();
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
