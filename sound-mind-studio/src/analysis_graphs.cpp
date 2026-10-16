#include "sound_mind/studio/analysis_graphs.h"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>

namespace sound_mind::studio {

namespace {

constexpr double kGraphFloorLufs = -60.0;

/// @brief `value` LUFS as a y coordinate in a `height`-tall graph (0 LUFS at the top).
double yFor(double value, double height) {
    const double clamped = std::clamp(value, kGraphFloorLufs, 0.0);
    return height * (clamped / kGraphFloorLufs);
}

QPainterPath seriesPath(const std::vector<float>& series, double xScale, double height) {
    QPainterPath path;
    for (std::size_t i = 0; i < series.size(); ++i) {
        const QPointF point(static_cast<double>(i) * xScale, yFor(series[i], height));
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    return path;
}

}  // namespace

LoudnessGraphWidget::LoudnessGraphWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(80);
    setToolTip(tr("Momentary (thin) and short-term (thick) loudness over time, -60 to 0 LUFS"));
}

void LoudnessGraphWidget::setSeries(std::vector<float> momentary, std::vector<float> shortTerm, double stepSeconds) {
    momentary_ = std::move(momentary);
    shortTerm_ = std::move(shortTerm);
    stepSeconds_ = stepSeconds > 0.0 ? stepSeconds : 0.1;
    update();
}

void LoudnessGraphWidget::setPlayhead(std::optional<double> seconds) {
    playhead_ = seconds;
    update();
}

void LoudnessGraphWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(20, 20, 28));
    const double height = static_cast<double>(rect().height());
    painter.setPen(QPen(QColor(60, 60, 75), 1));
    for (int lufs = -10; lufs > kGraphFloorLufs; lufs -= 10) {
        const double y = yFor(lufs, height);
        painter.drawLine(QPointF(0.0, y), QPointF(rect().width(), y));
    }
    if (momentary_.empty()) {
        return;
    }
    const double xScale = momentary_.size() > 1 ? static_cast<double>(rect().width()) / (momentary_.size() - 1) : 0.0;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(90, 170, 255), 1));
    painter.drawPath(seriesPath(momentary_, xScale, height));
    painter.setPen(QPen(QColor(255, 210, 80), 2));
    painter.drawPath(seriesPath(shortTerm_, xScale, height));
    if (playhead_.has_value()) {
        const double x = *playhead_ / stepSeconds_ * xScale;
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(QPointF(x, 0.0), QPointF(x, height));
    }
}

BandLevelsWidget::BandLevelsWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(60);
    setToolTip(tr("Level per frequency band over time - lowest band at the bottom"));
}

void BandLevelsWidget::setLevels(std::vector<float> levels, int bandCount, int frameCount, double secondsPerFrame) {
    levels_ = std::move(levels);
    bandCount_ = bandCount;
    frameCount_ = frameCount;
    secondsPerFrame_ = secondsPerFrame;
    update();
}

void BandLevelsWidget::setPlayhead(std::optional<double> seconds) {
    playhead_ = seconds;
    update();
}

QColor BandLevelsWidget::colorFor(float levelDb) {
    const double t = std::clamp((static_cast<double>(levelDb) + 70.0) / 70.0, 0.0, 1.0);
    // Dark blue -> magenta -> orange -> yellow.
    return QColor::fromHsvF(std::fmod(0.66 + t * 0.5, 1.0), 0.9, 0.15 + 0.85 * t);
}

void BandLevelsWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (levels_.empty() || bandCount_ <= 0 || frameCount_ <= 0) {
        return;
    }
    const double rowHeight = static_cast<double>(rect().height()) / bandCount_;
    const int columns = std::min(rect().width(), frameCount_);
    for (int column = 0; column < columns; ++column) {
        const int frame = static_cast<int>(static_cast<double>(column) / columns * frameCount_);
        const double x = static_cast<double>(column) / columns * rect().width();
        const double width = static_cast<double>(rect().width()) / columns + 1.0;
        for (int band = 0; band < bandCount_; ++band) {
            const float level = levels_[static_cast<std::size_t>(band) * frameCount_ + frame];
            const double y = rect().height() - (band + 1) * rowHeight;
            painter.fillRect(QRectF(x, y, width, rowHeight), colorFor(level));
        }
    }
    if (playhead_.has_value() && secondsPerFrame_ > 0.0) {
        const double x = *playhead_ / (secondsPerFrame_ * frameCount_) * rect().width();
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(QPointF(x, 0.0), QPointF(x, rect().height()));
    }
}

}  // namespace sound_mind::studio
