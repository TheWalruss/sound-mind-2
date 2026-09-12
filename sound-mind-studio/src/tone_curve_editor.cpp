#include "sound_mind/studio/tone_curve_editor.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include "sound_mind/core/tone_curve.h"

namespace sound_mind::studio {

namespace {

/// @brief Margin (px) inset from this widget's own bounds to its own
/// plot area - see plotRect()'s own docs.
constexpr qreal kMargin = 8.0;

/// @brief How close (px) a click needs to land to an existing point to
/// drag it, rather than insert a new one.
constexpr qreal kHitRadiusPixels = 8.0;

/// @brief Drawn radius (px) of each control point's own handle.
constexpr qreal kPointRadiusPixels = 4.0;

/// @brief How many segments to sample the curve into for drawing - dense
/// enough that individual line segments aren't visible.
constexpr int kCurveSamples = 128;

/// @brief How far apart (in normalized x) two adjacent points must stay
/// while dragging, so a point can never cross or land exactly on a
/// neighbor (which would make `evaluateToneCurve()`'s own per-segment
/// span zero).
constexpr float kMinPointSpacing = 0.001f;

}  // namespace

ToneCurveEditor::ToneCurveEditor(QWidget* parent) : QWidget(parent) { setMouseTracking(false); }

QSize ToneCurveEditor::sizeHint() const { return {240, 160}; }

void ToneCurveEditor::setPoints(std::vector<std::array<float, 2>> points) {
    points_ = std::move(points);
    draggingIndex_ = -1;
    update();
}

QRectF ToneCurveEditor::plotRect() const { return QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin); }

QPointF ToneCurveEditor::plotToWidget(std::array<float, 2> point) const {
    const QRectF r = plotRect();
    const qreal x = r.left() + static_cast<qreal>(point[0]) * r.width();
    // Plot y=0 at the bottom (a conventional curve editor), the opposite
    // of Qt's own top-down widget y.
    const qreal y = r.bottom() - static_cast<qreal>(point[1]) * r.height();
    return {x, y};
}

std::array<float, 2> ToneCurveEditor::widgetToPlot(QPointF widgetPosition) const {
    const QRectF r = plotRect();
    const float x = r.width() > 0.0 ? static_cast<float>((widgetPosition.x() - r.left()) / r.width()) : 0.0f;
    const float y = r.height() > 0.0 ? static_cast<float>((r.bottom() - widgetPosition.y()) / r.height()) : 0.0f;
    return {std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f)};
}

int ToneCurveEditor::hitTestPoint(QPointF widgetPosition) const {
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const QPointF p = plotToWidget(points_[i]);
        const qreal dx = p.x() - widgetPosition.x();
        const qreal dy = p.y() - widgetPosition.y();
        if (dx * dx + dy * dy <= kHitRadiusPixels * kHitRadiusPixels) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ToneCurveEditor::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF r = plotRect();
    painter.fillRect(rect(), palette().base());
    painter.setPen(QPen(palette().mid().color()));
    painter.drawRect(r);

    // Quarter gridlines, and the identity (no-op) reference diagonal.
    painter.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        const float t = static_cast<float>(i) / 4.0f;
        painter.drawLine(plotToWidget({t, 0.0f}), plotToWidget({t, 1.0f}));
        painter.drawLine(plotToWidget({0.0f, t}), plotToWidget({1.0f, t}));
    }
    painter.setPen(QPen(palette().mid().color(), 1, Qt::DashLine));
    painter.drawLine(plotToWidget({0.0f, 0.0f}), plotToWidget({1.0f, 1.0f}));

    // The curve itself, sampled via the exact same math applyFilter()'s
    // own ToneCurve case uses - see this class's own docs.
    if (points_.size() >= 2) {
        const auto tangents = sound_mind::core::monotoneCubicTangents(points_);
        QPainterPath path;
        for (int i = 0; i <= kCurveSamples; ++i) {
            const float x = static_cast<float>(i) / static_cast<float>(kCurveSamples);
            const float y = std::clamp(sound_mind::core::evaluateToneCurve(points_, tangents, x), 0.0f, 1.0f);
            const QPointF widgetPoint = plotToWidget({x, y});
            if (i == 0) {
                path.moveTo(widgetPoint);
            } else {
                path.lineTo(widgetPoint);
            }
        }
        painter.setPen(QPen(palette().highlight().color(), 2));
        painter.drawPath(path);
    }

    // Control point handles.
    painter.setPen(QPen(palette().text().color(), 1));
    painter.setBrush(palette().highlight());
    for (const auto& point : points_) {
        painter.drawEllipse(plotToWidget(point), kPointRadiusPixels, kPointRadiusPixels);
    }
}

void ToneCurveEditor::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QPointF pos = event->position();
    const int hit = hitTestPoint(pos);
    if (hit >= 0) {
        draggingIndex_ = hit;
        return;
    }

    const auto plotPoint = widgetToPlot(pos);
    const auto insertAt = std::upper_bound(points_.begin(), points_.end(), plotPoint,
                                            [](const auto& a, const auto& b) { return a[0] < b[0]; });
    draggingIndex_ = static_cast<int>(std::distance(points_.begin(), insertAt));
    points_.insert(insertAt, plotPoint);
    emit pointsChanged(points_);
    update();
}

void ToneCurveEditor::mouseMoveEvent(QMouseEvent* event) {
    if (draggingIndex_ < 0) {
        return;
    }
    auto plotPoint = widgetToPlot(event->position());
    const auto index = static_cast<std::size_t>(draggingIndex_);

    if (index == 0) {
        plotPoint[0] = 0.0f;  // The first point's own x stays pinned - see this class's own docs.
    } else if (index + 1 == points_.size()) {
        plotPoint[0] = 1.0f;  // The last point's own x stays pinned.
    } else {
        const float minX = points_[index - 1][0] + kMinPointSpacing;
        const float maxX = points_[index + 1][0] - kMinPointSpacing;
        plotPoint[0] = std::clamp(plotPoint[0], std::min(minX, maxX), std::max(minX, maxX));
    }

    points_[index] = plotPoint;
    emit pointsChanged(points_);
    update();
}

void ToneCurveEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        draggingIndex_ = -1;
    }
}

void ToneCurveEditor::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int hit = hitTestPoint(event->position());
    // Endpoints can't be removed - see this class's own docs.
    if (hit <= 0 || static_cast<std::size_t>(hit) + 1 >= points_.size()) {
        return;
    }
    points_.erase(points_.begin() + hit);
    draggingIndex_ = -1;
    emit pointsChanged(points_);
    update();
}

}  // namespace sound_mind::studio
