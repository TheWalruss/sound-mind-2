#include "sound_mind/studio/playback_panel.h"

#include <algorithm>

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

}  // namespace sound_mind::studio
