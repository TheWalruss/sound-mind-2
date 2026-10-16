#include "sound_mind/studio/analysis_graphs.h"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>

#include "sound_mind/core/loudness_meter.h"

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

StereoGraphWidget::StereoGraphWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(70);
    setToolTip(tr("Correlation (blue, -1 to +1) and width (gold, 0 to 1) over time"));
}

void StereoGraphWidget::setSeries(std::vector<float> correlation, std::vector<float> width, double stepSeconds) {
    correlation_ = std::move(correlation);
    width_ = std::move(width);
    stepSeconds_ = stepSeconds > 0.0 ? stepSeconds : 0.1;
    update();
}

void StereoGraphWidget::setPlayhead(std::optional<double> seconds) {
    playhead_ = seconds;
    update();
}

void StereoGraphWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(20, 20, 28));
    const double height = rect().height();
    painter.setPen(QPen(QColor(70, 70, 85), 1, Qt::DashLine));
    painter.drawLine(QPointF(0.0, height / 2.0), QPointF(rect().width(), height / 2.0));
    if (correlation_.empty()) {
        return;
    }
    const double xScale =
        correlation_.size() > 1 ? static_cast<double>(rect().width()) / (correlation_.size() - 1) : 0.0;
    QPainterPath correlationPath;
    QPainterPath widthPath;
    for (std::size_t i = 0; i < correlation_.size(); ++i) {
        const double x = static_cast<double>(i) * xScale;
        const QPointF c(x, height * (1.0 - (std::clamp(correlation_[i], -1.0f, 1.0f) + 1.0) / 2.0));
        const QPointF w(x, height * (1.0 - std::clamp(i < width_.size() ? width_[i] : 0.0f, 0.0f, 1.0f)));
        if (i == 0) {
            correlationPath.moveTo(c);
            widthPath.moveTo(w);
        } else {
            correlationPath.lineTo(c);
            widthPath.lineTo(w);
        }
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(90, 170, 255), 1.5));
    painter.drawPath(correlationPath);
    painter.setPen(QPen(QColor(255, 210, 80), 1.5));
    painter.drawPath(widthPath);
    if (playhead_.has_value()) {
        const double x = *playhead_ / stepSeconds_ * xScale;
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(QPointF(x, 0.0), QPointF(x, height));
    }
}

GoniometerWidget::GoniometerWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(120, 120);
    setToolTip(tr("Goniometer: mono is a vertical line, wide stereo a cloud, out-of-phase leans sideways"));
}

void GoniometerWidget::setAudio(std::shared_ptr<const sound_mind::codec::AudioBuffer> audio) {
    audio_ = std::move(audio);
    update();
}

void GoniometerWidget::setPosition(std::optional<double> seconds) {
    position_ = seconds;
    update();
}

std::vector<QPointF> GoniometerWidget::points() const {
    std::vector<QPointF> result;
    if (audio_ == nullptr || audio_->frameCount() == 0 || audio_->right.size() != audio_->frameCount()) {
        return result;
    }
    const std::size_t count = audio_->frameCount();
    std::size_t first = 0;
    std::size_t stride = 1;
    std::size_t taken = std::min<std::size_t>(count, kWindowSamples);
    if (position_.has_value()) {
        const auto centre = static_cast<std::size_t>(std::max(0.0, *position_) * audio_->sampleRateHz);
        const std::size_t half = kWindowSamples / 2;
        first = std::min(centre > half ? centre - half : 0, count - taken);
    } else {
        stride = std::max<std::size_t>(1, count / kWindowSamples);
        taken = std::min(count / stride, static_cast<std::size_t>(kWindowSamples));
    }
    result.reserve(taken);
    constexpr double kRootHalf = 0.70710678118654752;
    for (std::size_t i = 0; i < taken; ++i) {
        const std::size_t index = first + i * stride;
        const double left = audio_->left[index];
        const double right = audio_->right[index];
        result.emplace_back((left - right) * kRootHalf, (left + right) * kRootHalf);
    }
    return result;
}

void GoniometerWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(12, 12, 18));
    const double size = std::min(rect().width(), rect().height());
    const QPointF centre = QRectF(rect()).center();
    const double scale = size / 2.0 * 0.95;
    painter.setPen(QPen(QColor(60, 60, 75), 1));
    painter.drawLine(QPointF(centre.x(), centre.y() - scale), QPointF(centre.x(), centre.y() + scale));
    painter.drawLine(QPointF(centre.x() - scale, centre.y()), QPointF(centre.x() + scale, centre.y()));
    painter.setPen(QPen(QColor(120, 230, 160, 160), 1));
    for (const QPointF& point : points()) {
        painter.drawPoint(QPointF(centre.x() + std::clamp(point.x(), -1.0, 1.0) * scale,
                                  centre.y() - std::clamp(point.y(), -1.0, 1.0) * scale));
    }
}

BandWidthBarsWidget::BandWidthBarsWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(80);
    setToolTip(tr("Stereo width per frequency band: 0 is mono, 1 fully out of phase"));
}

void BandWidthBarsWidget::setWidths(std::vector<float> widths) {
    widths_ = std::move(widths);
    update();
}

void BandWidthBarsWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 20, 28));
    const auto& bands = sound_mind::core::loudnessBands();
    if (widths_.empty()) {
        return;
    }
    const double rowHeight = static_cast<double>(rect().height()) / static_cast<double>(widths_.size());
    const double labelWidth = rect().width() * 0.45;
    for (std::size_t band = 0; band < widths_.size(); ++band) {
        const double y = rect().height() - (band + 1) * rowHeight;
        const QRectF bar(labelWidth, y + 2.0, (rect().width() - labelWidth - 4.0) * std::clamp(widths_[band], 0.0f, 1.0f),
                         rowHeight - 4.0);
        painter.fillRect(bar, QColor(255, 210, 80));
        painter.setPen(QColor(200, 200, 210));
        if (band < bands.size()) {
            painter.drawText(QRectF(2.0, y, labelWidth - 4.0, rowHeight), Qt::AlignVCenter | Qt::AlignLeft,
                             QString::fromStdString(bands[band].label));
        }
    }
}

}  // namespace sound_mind::studio
