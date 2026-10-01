#include "sound_mind/studio/rotary_dial_widget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QtMath>

namespace sound_mind::studio {

namespace {

/// @brief Margin (px) inset from this widget's own bounds to the dial
/// itself.
constexpr qreal kMargin = 4.0;

/// @brief The dial's own fixed sweep, in degrees, clockwise from straight
/// up - see this class's own docs on why `270`, not a full `360`.
constexpr double kStartAngleDegrees = -135.0;
constexpr double kSweepDegrees = 270.0;

/// @brief How many vertical pixels of drag sweep the dial's own entire
/// `[minimum, maximum]` range - see mouseMoveEvent()'s own docs.
constexpr qreal kPixelsPerFullSweep = 150.0;

/// @brief Stroke width (px) of the ring itself.
constexpr qreal kRingWidthPixels = 4.0;

/// @brief Angle step (degrees) between consecutive points sampled along
/// the ring - fine enough to look smooth at this widget's own small size.
constexpr double kArcStepDegrees = 4.0;

/// @brief A point on a circle of `radius` centered at `center`, at
/// `degrees` clockwise from straight up - the one place this file
/// converts the class's own "clockwise from up" angle convention into
/// Cartesian, screen-space (y-down) coordinates.
QPointF pointOnCircle(QPointF center, qreal radius, double degrees) {
    const double radians = qDegreesToRadians(degrees);
    return center + QPointF(std::sin(radians), -std::cos(radians)) * radius;
}

/// @brief Strokes the portion of the ring from `startDegrees` to
/// `endDegrees` (both in this class's own "clockwise from up" convention)
/// in `color` - the shared body `paintEvent()` calls twice, once for the
/// unswept remainder and once for the swept (minimum-to-value) portion.
void paintArc(QPainter& painter, QPointF center, qreal radius, double startDegrees, double endDegrees,
              const QColor& color) {
    if (endDegrees <= startDegrees) {
        return;
    }
    QPainterPath path;
    path.moveTo(pointOnCircle(center, radius, startDegrees));
    for (double degrees = startDegrees + kArcStepDegrees; degrees < endDegrees; degrees += kArcStepDegrees) {
        path.lineTo(pointOnCircle(center, radius, degrees));
    }
    path.lineTo(pointOnCircle(center, radius, endDegrees));

    painter.setPen(QPen(color, kRingWidthPixels, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(path);
}

}  // namespace

RotaryDialWidget::RotaryDialWidget(QWidget* parent) : QWidget(parent) {}

QSize RotaryDialWidget::sizeHint() const { return {48, 48}; }

void RotaryDialWidget::setRange(double minimum, double maximum) {
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = std::clamp(value_, minimum_, maximum_);
    update();
}

void RotaryDialWidget::setValue(double value) {
    value_ = std::clamp(value, minimum_, maximum_);
    dragging_ = false;
    update();
}

double RotaryDialWidget::angleForValue(double value) const {
    const double range = maximum_ - minimum_;
    const double fraction = range > 0.0 ? (value - minimum_) / range : 0.0;
    return kStartAngleDegrees + std::clamp(fraction, 0.0, 1.0) * kSweepDegrees;
}

void RotaryDialWidget::applyDraggedValue(double value) {
    const double clamped = std::clamp(value, minimum_, maximum_);
    if (clamped == value_) {
        return;
    }
    value_ = clamped;
    emit valueChanged(value_);
    update();
}

void RotaryDialWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    const QPointF center = r.center();
    const qreal radius = std::min(r.width(), r.height()) / 2.0 - kRingWidthPixels / 2.0;
    const double currentAngle = angleForValue(value_);

    // The unswept remainder of the ring first, full range end to end, so
    // the swept highlight below paints over only its own portion.
    paintArc(painter, center, radius, kStartAngleDegrees, kStartAngleDegrees + kSweepDegrees, palette().mid().color());
    paintArc(painter, center, radius, kStartAngleDegrees, currentAngle, palette().highlight().color());

    // The pointer line, on top of both - marks the exact current angle,
    // distinct from the ring's own filled sweep.
    painter.setPen(QPen(palette().text().color(), 2));
    painter.drawLine(pointOnCircle(center, radius * 0.4, currentAngle), pointOnCircle(center, radius * 0.95, currentAngle));
}

void RotaryDialWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = true;
    dragStartY_ = event->position().y();
    dragStartValue_ = value_;
}

void RotaryDialWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) {
        return;
    }
    const qreal deltaY = dragStartY_ - event->position().y();  // Up is positive - see this class's own docs.
    const double fraction = static_cast<double>(deltaY) / kPixelsPerFullSweep;
    applyDraggedValue(dragStartValue_ + fraction * (maximum_ - minimum_));
}

void RotaryDialWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
    }
}

}  // namespace sound_mind::studio
