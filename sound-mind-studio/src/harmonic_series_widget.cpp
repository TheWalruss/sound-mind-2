#include "sound_mind/studio/harmonic_series_widget.h"

#include <algorithm>
#include <cstddef>

#include <QApplication>
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>

namespace sound_mind::studio {

namespace {

/// @brief Margin (px) inset from this widget's own bounds to its own plot
/// area - see plotRect()'s own docs. Uniform on every side, unlike
/// `EqualizerCurveWidget`'s own wider left margin - no axis labels are
/// drawn here (the existing per-harmonic spin boxes already show exact
/// numbers beside this widget).
constexpr qreal kMargin = 8.0;

/// @brief This widget's own vertical display ceiling - see this class's
/// own docs on why `2.0`, not `InstrumentConfiguration`'s full `[0, 10]`
/// spin-box range.
constexpr double kDisplayMaxStrength = 2.0;

/// @brief Horizontal gap (px) between adjacent bars.
constexpr qreal kBarGapPixels = 2.0;

}  // namespace

HarmonicSeriesWidget::HarmonicSeriesWidget(QWidget* parent) : QWidget(parent) {}

QSize HarmonicSeriesWidget::sizeHint() const { return {240, 100}; }

void HarmonicSeriesWidget::setHarmonicStrengths(std::vector<double> strengths) {
    strengths_ = std::move(strengths);
    draggingIndex_ = -1;
    update();
}

QRectF HarmonicSeriesWidget::plotRect() const { return QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin); }

qreal HarmonicSeriesWidget::valueToY(const QRectF& rect, double value) {
    const double clamped = std::clamp(value, 0.0, kDisplayMaxStrength);
    return rect.bottom() - (clamped / kDisplayMaxStrength) * rect.height();
}

double HarmonicSeriesWidget::yToValue(const QRectF& rect, qreal y) {
    const double fraction = rect.height() > 0.0 ? (rect.bottom() - y) / rect.height() : 0.0;
    return std::clamp(fraction, 0.0, 1.0) * kDisplayMaxStrength;
}

int HarmonicSeriesWidget::columnAt(const QRectF& rect, qreal x, std::size_t count) {
    if (count == 0) {
        return -1;
    }
    const qreal columnWidth = rect.width() / static_cast<qreal>(count);
    const int index = static_cast<int>((x - rect.left()) / columnWidth);
    return std::clamp(index, 0, static_cast<int>(count) - 1);
}

void HarmonicSeriesWidget::paintBars(QPainter& painter, const QRectF& rect, const std::vector<double>& strengths,
                                      const QColor& fillColor) {
    if (strengths.empty()) {
        return;
    }
    const qreal columnWidth = rect.width() / static_cast<qreal>(strengths.size());
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    for (std::size_t i = 0; i < strengths.size(); ++i) {
        const qreal left = rect.left() + static_cast<qreal>(i) * columnWidth;
        const qreal top = valueToY(rect, strengths[i]);
        const QRectF barRect(left + kBarGapPixels / 2.0, top, columnWidth - kBarGapPixels, rect.bottom() - top);
        painter.drawRect(barRect);
    }
}

void HarmonicSeriesWidget::setBarValueFromY(std::size_t index, qreal y) {
    if (index >= strengths_.size()) {
        return;
    }
    const double value = yToValue(plotRect(), y);
    if (strengths_[index] == value) {
        return;
    }
    strengths_[index] = value;
    emit harmonicStrengthsChanged(strengths_);
    update();
}

void HarmonicSeriesWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF r = plotRect();
    painter.fillRect(rect(), palette().base());
    painter.setPen(QPen(palette().mid().color()));
    painter.drawRect(r);

    // Quarter value gridlines (horizontal) - the same minimalism
    // ToneCurveEditor's/EqualizerCurveWidget's own quarter gridlines use.
    painter.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        const qreal y = valueToY(r, kDisplayMaxStrength * static_cast<double>(i) / 4.0);
        painter.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }

    paintBars(painter, r, strengths_, palette().highlight().color());
}

QImage HarmonicSeriesWidget::renderThumbnail(const std::vector<double>& strengths, const QSize& size) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (strengths.empty() || size.isEmpty()) {
        return image;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r(0, 0, size.width(), size.height());
    paintBars(painter, r, strengths, QApplication::palette().highlight().color());
    return image;
}

void HarmonicSeriesWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QPointF pos = event->position();
    const int column = columnAt(plotRect(), pos.x(), strengths_.size());
    if (column < 0) {
        return;
    }
    draggingIndex_ = column;
    setBarValueFromY(static_cast<std::size_t>(column), pos.y());
}

void HarmonicSeriesWidget::mouseMoveEvent(QMouseEvent* event) {
    if (draggingIndex_ < 0) {
        return;
    }
    setBarValueFromY(static_cast<std::size_t>(draggingIndex_), event->position().y());
}

void HarmonicSeriesWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        draggingIndex_ = -1;
    }
}

}  // namespace sound_mind::studio
