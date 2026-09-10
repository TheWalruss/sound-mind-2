#include "sound_mind/studio/playback_panel.h"

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

const QString kSystemDefaultLabel = QObject::tr("(System Default)");

/// @brief Formats `seconds` as "M:SS" - the position bar's own time label,
/// both halves of "elapsed / total". Negative or non-finite input is
/// treated as `0`, so a not-yet-loaded/zero-duration state ("0:00 / 0:00")
/// never shows garbage.
QString formatMinutesSeconds(double seconds) {
    const double safeSeconds = (std::isfinite(seconds) && seconds > 0.0) ? seconds : 0.0;
    const auto totalWholeSeconds = static_cast<int>(safeSeconds);
    const int minutes = totalWholeSeconds / 60;
    const int remainingSeconds = totalWholeSeconds % 60;
    return QStringLiteral("%1:%2").arg(minutes).arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}

/// @brief Replaces `combo`'s items with a "(System Default)" entry
/// followed by `deviceNames` - see `LoopPanel`'s identical helper for why
/// device name (not display index) is what's preserved across a refresh.
void populateDeviceCombo(QComboBox* combo, const QStringList& deviceNames) {
    const QString previousSelection = combo->currentData().toString();

    combo->blockSignals(true);
    combo->clear();
    combo->addItem(kSystemDefaultLabel, QString());
    for (const QString& name : deviceNames) {
        combo->addItem(name, name);
    }
    const int previousIndex = combo->findData(previousSelection);
    combo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
    combo->blockSignals(false);
}

}  // namespace

PlaybackPanel::PlaybackPanel(QWidget* parent) : QDockWidget(tr("Playback"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* transportRow = new QHBoxLayout();
    auto* playButton = new QPushButton(tr("Play"), container);
    playButton->setObjectName(QStringLiteral("playButton"));
    connect(playButton, &QPushButton::clicked, this, &PlaybackPanel::playRequested);
    transportRow->addWidget(playButton);

    auto* pauseButton = new QPushButton(tr("Pause"), container);
    pauseButton->setObjectName(QStringLiteral("pauseButton"));
    connect(pauseButton, &QPushButton::clicked, this, &PlaybackPanel::pauseRequested);
    transportRow->addWidget(pauseButton);

    auto* stopButton = new QPushButton(tr("Stop"), container);
    stopButton->setObjectName(QStringLiteral("stopButton"));
    connect(stopButton, &QPushButton::clicked, this, &PlaybackPanel::stopRequested);
    transportRow->addWidget(stopButton);
    root->addLayout(transportRow);

    positionSlider_ = new QSlider(Qt::Horizontal, container);
    positionSlider_->setObjectName(QStringLiteral("positionSlider"));
    positionSlider_->setRange(0, kPositionSliderSteps);
    connect(positionSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (totalSeconds_ <= 0.0) {
            return;  // nothing loaded yet - dragging does nothing, see setDuration()'s own docs.
        }
        emit seekRequested((static_cast<double>(value) / kPositionSliderSteps) * totalSeconds_);
    });
    root->addWidget(positionSlider_);

    positionLabel_ = new QLabel(container);
    positionLabel_->setObjectName(QStringLiteral("positionLabel"));
    positionLabel_->setAlignment(Qt::AlignHCenter);
    root->addWidget(positionLabel_);
    updatePositionLabel();

    auto* outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(tr("Output:"), container));
    outputDeviceCombo_ = new QComboBox(container);
    outputDeviceCombo_->setObjectName(QStringLiteral("outputDeviceCombo"));
    connect(outputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit outputDeviceChanged(outputDeviceCombo_->itemData(index).toString());
    });
    outputRow->addWidget(outputDeviceCombo_, 1);
    root->addLayout(outputRow);

    auto* volumeRow = new QHBoxLayout();
    volumeRow->addWidget(new QLabel(tr("Volume:"), container));
    volumeSlider_ = new QSlider(Qt::Horizontal, container);
    volumeSlider_->setObjectName(QStringLiteral("volumeSlider"));
    volumeSlider_->setRange(0, kMaxVolumePercent);
    volumeSlider_->setValue(100);
    connect(volumeSlider_, &QSlider::valueChanged, this, &PlaybackPanel::volumePercentChanged);
    volumeRow->addWidget(volumeSlider_, 1);
    root->addLayout(volumeRow);

    root->addStretch();

    populateDeviceCombo(outputDeviceCombo_, {});

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void PlaybackPanel::setOutputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(outputDeviceCombo_, deviceNames);
}

void PlaybackPanel::setVolumePercent(int percent) {
    const QSignalBlocker blocker(volumeSlider_);
    volumeSlider_->setValue(std::clamp(percent, 0, kMaxVolumePercent));
}

void PlaybackPanel::setDuration(double totalSeconds) {
    totalSeconds_ = std::max(0.0, totalSeconds);
    positionSeconds_ = 0.0;
    {
        const QSignalBlocker blocker(positionSlider_);
        positionSlider_->setValue(0);
    }
    updatePositionLabel();
}

void PlaybackPanel::setPositionSeconds(double positionSeconds) {
    positionSeconds_ = std::clamp(positionSeconds, 0.0, totalSeconds_);
    const int value =
        totalSeconds_ > 0.0 ? static_cast<int>(std::lround((positionSeconds_ / totalSeconds_) * kPositionSliderSteps)) : 0;
    {
        const QSignalBlocker blocker(positionSlider_);
        positionSlider_->setValue(value);
    }
    updatePositionLabel();
}

void PlaybackPanel::updatePositionLabel() {
    positionLabel_->setText(formatMinutesSeconds(positionSeconds_) + QStringLiteral(" / ") +
                             formatMinutesSeconds(totalSeconds_));
}

}  // namespace sound_mind::studio
