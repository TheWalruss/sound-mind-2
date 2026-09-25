#include "sound_mind/studio/gradient_bar_widget.h"

#include <algorithm>
#include <cstddef>

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include "sound_mind/studio/color_conversion.h"

namespace sound_mind::studio {

namespace {

/// @brief Margin (px) inset from this widget's own bounds to its own bar
/// area - see barRect()'s own docs.
constexpr qreal kMargin = 8.0;

/// @brief How close (px) a click needs to land to an existing stop's own
/// marker to select/drag it, rather than insert a new one.
constexpr qreal kHitRadiusPixels = 8.0;

/// @brief The checkerboard's own square size (px).
constexpr int kCheckerSize = 6;

/// @brief How many samples to build the gradient preview's own
/// `QLinearGradient` from - dense enough that interior stops' own sharp
/// transitions still read clearly.
constexpr int kPreviewSamples = 64;

/// @brief How far apart (in normalized t) two adjacent stops must stay
/// while dragging, matching `ToneCurveEditor`'s own `kMinPointSpacing`
/// precedent for the same "never cross or land exactly on a neighbor"
/// reason.
constexpr float kMinStopSpacing = 0.001f;

/// @brief `stop`'s own display color, per `dbToDisplayByte()`'s
/// `Red = leftIntensity, Green = rightIntensity` mapping
/// `ToolConfigurationPanel::color()`'s own pre-finding-#17 single-color
/// button already established - alpha from the average of both channels'
/// own opacity.
QColor stopToColor(const sound_mind::core::GradientStop& stop) {
    QColor color(dbToDisplayByte(stop.leftIntensity), dbToDisplayByte(stop.rightIntensity), 0);
    color.setAlphaF(std::clamp((stop.leftOpacity + stop.rightOpacity) / 2.0f, 0.0f, 1.0f));
    return color;
}

}  // namespace

GradientBarWidget::GradientBarWidget(QWidget* parent) : QWidget(parent) {}

QSize GradientBarWidget::sizeHint() const { return {240, 48}; }

void GradientBarWidget::setGradient(sound_mind::core::Gradient gradient) {
    gradient_ = std::move(gradient);
    selectedIndex_ = 0;
    draggingIndex_ = -1;
    update();
}

void GradientBarWidget::setSelectedStopValues(const sound_mind::core::GradientStop& values) {
    gradient_.setStopValues(selectedIndex_, values);
    emit gradientChanged(gradient_);
    update();
}

void GradientBarWidget::setLinkChannels(bool linked) {
    gradient_.setLinkChannels(linked);
    emit gradientChanged(gradient_);
}

void GradientBarWidget::removeSelectedStop() {
    if (!gradient_.removeStop(selectedIndex_)) {
        return;
    }
    if (selectedIndex_ >= gradient_.stops().size()) {
        selectedIndex_ = gradient_.stops().size() - 1;
    }
    draggingIndex_ = -1;
    emit gradientChanged(gradient_);
    emit selectionChanged(selectedIndex_);
    update();
}

QRectF GradientBarWidget::barRect() const { return QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin); }

qreal GradientBarWidget::tToX(float t) const {
    const QRectF r = barRect();
    return r.left() + static_cast<qreal>(t) * r.width();
}

float GradientBarWidget::xToT(qreal x) const {
    const QRectF r = barRect();
    const float t = r.width() > 0.0 ? static_cast<float>((x - r.left()) / r.width()) : 0.0f;
    return std::clamp(t, 0.0f, 1.0f);
}

int GradientBarWidget::hitTestStop(qreal x) const {
    const auto& stops = gradient_.stops();
    for (std::size_t i = 0; i < stops.size(); ++i) {
        if (std::abs(tToX(stops[i].t) - x) <= kHitRadiusPixels) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void GradientBarWidget::selectStop(std::size_t index) {
    if (selectedIndex_ == index) {
        return;
    }
    selectedIndex_ = index;
    emit selectionChanged(selectedIndex_);
    update();
}

void GradientBarWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const QRectF r = barRect();

    // Checkerboard - shows through wherever the gradient preview below is
    // less than fully opaque.
    for (int y = 0; y < static_cast<int>(r.height()); y += kCheckerSize) {
        for (int x = 0; x < static_cast<int>(r.width()); x += kCheckerSize) {
            const bool dark = ((x / kCheckerSize) + (y / kCheckerSize)) % 2 == 0;
            painter.fillRect(QRectF(r.left() + x, r.top() + y, kCheckerSize, kCheckerSize),
                              dark ? QColor(160, 160, 160) : QColor(200, 200, 200));
        }
    }

    // The gradient preview itself, sampled across the bar's own width -
    // QLinearGradient interpolates smoothly between however many stops
    // are given it, matching Gradient::evaluate()'s own linear
    // interpolation.
    QLinearGradient preview(r.left(), 0.0, r.right(), 0.0);
    for (int i = 0; i <= kPreviewSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kPreviewSamples);
        preview.setColorAt(static_cast<qreal>(t), stopToColor(gradient_.evaluate(t)));
    }
    painter.fillRect(r, preview);

    painter.setPen(QPen(palette().mid().color()));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(r);

    // Stop markers, along the bar's own bottom edge - a small triangle,
    // larger and in the highlight color for the selected stop.
    painter.setRenderHint(QPainter::Antialiasing);
    const auto& stops = gradient_.stops();
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const bool selected = i == selectedIndex_;
        const qreal x = tToX(stops[i].t);
        const qreal size = selected ? 7.0 : 5.0;
        QPolygonF marker;
        marker << QPointF(x - size, r.bottom() + size * 2) << QPointF(x + size, r.bottom() + size * 2)
               << QPointF(x, r.bottom() + 2);
        painter.setPen(QPen(palette().text().color(), 1));
        painter.setBrush(selected ? palette().highlight() : palette().mid());
        painter.drawPolygon(marker);
    }
}

void GradientBarWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const qreal x = event->position().x();
    const int hit = hitTestStop(x);
    if (hit >= 0) {
        draggingIndex_ = hit;
        selectStop(static_cast<std::size_t>(hit));
        return;
    }

    const std::size_t insertedIndex = gradient_.insertStop(xToT(x));
    draggingIndex_ = static_cast<int>(insertedIndex);
    emit gradientChanged(gradient_);
    selectStop(insertedIndex);
    update();
}

void GradientBarWidget::mouseMoveEvent(QMouseEvent* event) {
    if (draggingIndex_ < 0) {
        return;
    }
    const auto index = static_cast<std::size_t>(draggingIndex_);
    const auto& stops = gradient_.stops();
    if (index == 0 || index + 1 == stops.size()) {
        return;  // Endpoints stay pinned at t=0/t=1 - only interior stops can move.
    }

    float t = xToT(event->position().x());
    const float minT = stops[index - 1].t + kMinStopSpacing;
    const float maxT = stops[index + 1].t - kMinStopSpacing;
    t = std::clamp(t, std::min(minT, maxT), std::max(minT, maxT));

    auto values = stops[index];
    values.t = t;
    // setStopValues() ignores the t field it's given (see its own docs) -
    // moving a stop is remove-then-reinsert-at-the-new-position, the same
    // "position only changes via insertStop()/removeStop()" contract
    // Gradient itself documents.
    gradient_.removeStop(index);
    const std::size_t newIndex = gradient_.insertStop(t);
    gradient_.setStopValues(newIndex, values);
    draggingIndex_ = static_cast<int>(newIndex);
    selectedIndex_ = newIndex;
    emit gradientChanged(gradient_);
    update();
}

void GradientBarWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        draggingIndex_ = -1;
    }
}

void GradientBarWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int hit = hitTestStop(event->position().x());
    if (hit < 0) {
        return;
    }
    selectStop(static_cast<std::size_t>(hit));
    removeSelectedStop();
}

}  // namespace sound_mind::studio
