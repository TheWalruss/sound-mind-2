#include "sound_mind/studio/waveform_preview_widget.h"

#include <algorithm>
#include <cstddef>

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace sound_mind::studio {

namespace {

/// @brief Margin (px) inset from this widget's own bounds to its own plot
/// area - see plotRect()'s own docs.
constexpr qreal kMargin = 4.0;

}  // namespace

WaveformPreviewWidget::WaveformPreviewWidget(QWidget* parent) : QWidget(parent) {}

QSize WaveformPreviewWidget::sizeHint() const { return {240, 56}; }

void WaveformPreviewWidget::setSamples(std::vector<float> samples) {
    samples_ = std::move(samples);
    update();
}

void WaveformPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    painter.fillRect(rect(), palette().base());
    painter.setPen(QPen(palette().mid().color()));
    painter.drawRect(r);

    if (samples_.size() < 2) {
        return;
    }
    QPainterPath path;
    const qreal step = r.width() / static_cast<qreal>(samples_.size() - 1);
    for (std::size_t i = 0; i < samples_.size(); ++i) {
        const qreal x = r.left() + static_cast<qreal>(i) * step;
        const float clamped = std::clamp(samples_[i], 0.0f, 1.0f);
        const qreal y = r.bottom() - static_cast<qreal>(clamped) * r.height();
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }
    painter.setPen(QPen(palette().highlight().color(), 2));
    painter.drawPath(path);
}

}  // namespace sound_mind::studio
