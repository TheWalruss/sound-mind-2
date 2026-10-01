#include "sound_mind/studio/equalizer_curve_widget.h"

#include <algorithm>
#include <cstddef>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/axis_labels.h"

namespace sound_mind::studio {

namespace {

/// @brief Margin (px) inset from this widget's own bounds to its own plot
/// area - see plotRect()'s own docs. Wider on the left than
/// `ToneCurveEditor`'s own uniform margin, to leave room for frequency
/// tick labels.
constexpr qreal kMarginTop = 8.0;
constexpr qreal kMarginRight = 8.0;
constexpr qreal kMarginBottom = 8.0;
constexpr qreal kMarginLeft = 48.0;

/// @brief How close (px) a click needs to land to an existing handle to
/// drag it, rather than insert a new one.
constexpr qreal kHitRadiusPixels = 8.0;

/// @brief Drawn radius (px) of each stop's own handle.
constexpr qreal kHandleRadiusPixels = 4.0;

/// @brief How far apart (in normalized t) two adjacent stops must stay
/// while dragging, matching `GradientBarWidget`'s own `kMinStopSpacing`
/// precedent for the same "never cross or land exactly on a neighbor"
/// reason.
constexpr float kMinStopSpacing = 0.001f;

}  // namespace

EqualizerCurveWidget::EqualizerCurveWidget(QWidget* parent) : QWidget(parent) {}

QSize EqualizerCurveWidget::sizeHint() const { return {160, 240}; }

void EqualizerCurveWidget::setGradient(sound_mind::core::Gradient gradient) {
    gradient_ = std::move(gradient);
    selectedIndex_ = 0;
    draggingIndex_ = -1;
    update();
}

void EqualizerCurveWidget::setProjectSettings(std::optional<sound_mind::core::ProjectSettings> settings) {
    settings_ = std::move(settings);
    update();
}

float EqualizerCurveWidget::cutAmount(const sound_mind::core::GradientStop& stop) noexcept {
    return std::clamp(std::max(stop.leftOpacity, stop.rightOpacity), 0.0f, 1.0f);
}

QRectF EqualizerCurveWidget::plotRect() const {
    return QRectF(rect()).adjusted(kMarginLeft, kMarginTop, -kMarginRight, -kMarginBottom);
}

qreal EqualizerCurveWidget::tToY(float t) const {
    const QRectF r = plotRect();
    // t=1 (highest frequency) at the top, t=0 (lowest) at the bottom - see
    // this class's own docs on why this is the opposite of
    // ToneCurveEditor's/GradientBarWidget's own plain top-down/left-right
    // mapping.
    return r.bottom() - static_cast<qreal>(t) * r.height();
}

float EqualizerCurveWidget::yToT(qreal y) const {
    const QRectF r = plotRect();
    const float t = r.height() > 0.0 ? static_cast<float>((r.bottom() - y) / r.height()) : 0.0f;
    return std::clamp(t, 0.0f, 1.0f);
}

qreal EqualizerCurveWidget::cutToX(float cut) const {
    const QRectF r = plotRect();
    return r.left() + static_cast<qreal>(cut) * r.width();
}

int EqualizerCurveWidget::hitTestStop(QPointF widgetPosition) const {
    const auto& stops = gradient_.stops();
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const QPointF handle(cutToX(cutAmount(stops[i])), tToY(stops[i].t));
        const qreal dx = handle.x() - widgetPosition.x();
        const qreal dy = handle.y() - widgetPosition.y();
        if (dx * dx + dy * dy <= kHitRadiusPixels * kHitRadiusPixels) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void EqualizerCurveWidget::selectStop(std::size_t index) {
    if (selectedIndex_ == index) {
        return;
    }
    selectedIndex_ = index;
    emit selectionChanged(selectedIndex_);
    update();
}

void EqualizerCurveWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF r = plotRect();
    painter.fillRect(rect(), palette().base());
    painter.setPen(QPen(palette().mid().color()));
    painter.drawRect(r);

    // Quarter Cut-amount gridlines (vertical) - the same "no extra text,
    // just a visual reference" minimalism ToneCurveEditor's own quarter
    // gridlines use for its value axis.
    painter.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        const qreal x = cutToX(static_cast<float>(i) / 4.0f);
        painter.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
    }

    // Frequency-axis guides - this roadmap item's own "frequency-domain
    // guides (axis labels/gridlines)" ask, reusing the exact tick-choosing
    // logic the canvas's own frequency axis labels already use, rather
    // than a bespoke fixed band list (the legacy Python Studio's own
    // approach) - see this class's own docs.
    if (settings_.has_value()) {
        const auto ticks = verticalAxisTicks(VerticalAxisLabelMode::Hertz, *settings_, r.height());
        const auto codecConfig = sound_mind::core::streamCodecConfigFor(*settings_);
        painter.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
        const QFontMetrics metrics = painter.fontMetrics();
        for (const auto& tick : ticks) {
            // tick.domainValue is a frequency in Hz; convert to t via the
            // same normalized-position-across-the-encoded-range logic
            // filter_application.h's own docs describe (bin index fraction
            // *is* t - no separate Hz step needed since the ticks
            // themselves were already chosen in Hz).
            const float binIndex =
                sound_mind::core::frequencyToBinIndex(static_cast<float>(tick.domainValue), codecConfig);
            const float t = codecConfig.binCount > 1
                                 ? binIndex / static_cast<float>(codecConfig.binCount - 1)
                                 : 0.0f;
            const qreal y = tToY(std::clamp(t, 0.0f, 1.0f));
            painter.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
            painter.setPen(QPen(palette().text().color()));
            painter.drawText(QRectF(0, y - metrics.height() / 2.0, kMarginLeft - 4, metrics.height()),
                              Qt::AlignRight | Qt::AlignVCenter, tick.label);
            painter.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
        }
    }

    // The curve itself - straight segments between consecutive stops,
    // matching Gradient::evaluate()'s own piecewise-linear interpolation
    // exactly (unlike ToneCurveEditor's smooth spline) - no dense sampling
    // needed.
    const auto& stops = gradient_.stops();
    if (stops.size() >= 2) {
        QPainterPath path;
        for (std::size_t i = 0; i < stops.size(); ++i) {
            const QPointF point(cutToX(cutAmount(stops[i])), tToY(stops[i].t));
            if (i == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
        painter.setPen(QPen(palette().highlight().color(), 2));
        painter.drawPath(path);
    }

    // Stop handles, the selected one drawn larger - the same
    // GradientBarWidget::selectedIndex()-highlighting precedent.
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const QPointF point(cutToX(cutAmount(stops[i])), tToY(stops[i].t));
        const qreal radius = (i == selectedIndex_) ? kHandleRadiusPixels * 1.5 : kHandleRadiusPixels;
        painter.setPen(QPen(palette().text().color(), 1));
        painter.setBrush(palette().highlight());
        painter.drawEllipse(point, radius, radius);
    }
}

void EqualizerCurveWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QPointF pos = event->position();
    const int hit = hitTestStop(pos);
    if (hit >= 0) {
        draggingIndex_ = hit;
        selectStop(static_cast<std::size_t>(hit));
        return;
    }

    const std::size_t insertedIndex = gradient_.insertStop(yToT(pos.y()));
    draggingIndex_ = static_cast<int>(insertedIndex);
    emit gradientChanged(gradient_);
    selectStop(insertedIndex);
    update();
}

void EqualizerCurveWidget::mouseMoveEvent(QMouseEvent* event) {
    if (draggingIndex_ < 0) {
        return;
    }
    const auto index = static_cast<std::size_t>(draggingIndex_);
    const auto& stops = gradient_.stops();
    if (index == 0 || index + 1 == stops.size()) {
        return;  // Endpoints stay pinned at t=0/t=1 - only interior stops can move.
    }

    float t = yToT(event->position().y());
    const float minT = stops[index - 1].t + kMinStopSpacing;
    const float maxT = stops[index + 1].t - kMinStopSpacing;
    t = std::clamp(t, std::min(minT, maxT), std::max(minT, maxT));

    // Moving a stop is remove-then-reinsert-at-the-new-position, the same
    // workaround GradientBarWidget::mouseMoveEvent() already establishes -
    // see its own docs for why (Gradient's own "position only changes via
    // insertStop()/removeStop()" contract). Cut amount (the values below)
    // is deliberately preserved exactly, not re-seeded from evaluate(t) at
    // the new position - this widget only ever repositions frequency.
    const auto values = stops[index];
    gradient_.removeStop(index);
    const std::size_t newIndex = gradient_.insertStop(t);
    gradient_.setStopValues(newIndex, values);
    draggingIndex_ = static_cast<int>(newIndex);
    selectedIndex_ = newIndex;
    emit gradientChanged(gradient_);
    update();
}

void EqualizerCurveWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        draggingIndex_ = -1;
    }
}

void EqualizerCurveWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int hit = hitTestStop(event->position());
    if (hit <= 0 || static_cast<std::size_t>(hit) + 1 >= gradient_.stops().size()) {
        return;  // Endpoints can't be removed.
    }
    gradient_.removeStop(static_cast<std::size_t>(hit));
    draggingIndex_ = -1;
    if (selectedIndex_ >= gradient_.stops().size()) {
        selectedIndex_ = gradient_.stops().size() - 1;
    }
    emit gradientChanged(gradient_);
    emit selectionChanged(selectedIndex_);
    update();
}

}  // namespace sound_mind::studio
