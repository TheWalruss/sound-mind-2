#include "sound_mind/studio/loop_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include "sound_mind/studio/device_combo_helpers.h"

namespace sound_mind::studio {

LoopPanel::LoopPanel(QWidget* parent) : QDockWidget(tr("Loop"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    toggleButton_ = new QPushButton(tr("Start Loop"), container);
    toggleButton_->setObjectName(QStringLiteral("loopToggleButton"));
    toggleButton_->setCheckable(true);
    connect(toggleButton_, &QPushButton::clicked, this, &LoopPanel::toggleRequested);
    root->addWidget(toggleButton_);

    // Labeled "Freeze Loop" (not "Keep Looping" - confirmed with the user
    // as not descriptive of what it actually does): the standard
    // loop-pedal term for freezing whichever take is currently playing,
    // rather than recording over it every cycle. The internal name and API
    // (keepLoopingCheckBox, setKeepLooping()/keepLoopingChanged()) are
    // unchanged - only the visible label.
    keepLoopingCheckBox_ = new QCheckBox(tr("Freeze Loop"), container);
    keepLoopingCheckBox_->setObjectName(QStringLiteral("keepLoopingCheckBox"));
    connect(keepLoopingCheckBox_, &QCheckBox::toggled, this, &LoopPanel::keepLoopingChanged);
    root->addWidget(keepLoopingCheckBox_);

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(new QLabel(tr("Input:"), container));
    inputDeviceCombo_ = new QComboBox(container);
    inputDeviceCombo_->setObjectName(QStringLiteral("inputDeviceCombo"));
    connect(inputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit inputDeviceChanged(inputDeviceCombo_->itemData(index).toString());
    });
    inputRow->addWidget(inputDeviceCombo_, 1);
    root->addLayout(inputRow);

    auto* outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(tr("Output:"), container));
    outputDeviceCombo_ = new QComboBox(container);
    outputDeviceCombo_->setObjectName(QStringLiteral("outputDeviceCombo"));
    connect(outputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit outputDeviceChanged(outputDeviceCombo_->itemData(index).toString());
    });
    outputRow->addWidget(outputDeviceCombo_, 1);
    root->addLayout(outputRow);

    root->addStretch();

    populateDeviceCombo(inputDeviceCombo_, {});
    populateDeviceCombo(outputDeviceCombo_, {});

    // Wrapped in a QScrollArea - per the confirmed scope for this
    // milestone - so this panel's content is never clipped, and never
    // forces the dock wider than the window, if it doesn't fit the
    // available height.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void LoopPanel::setRunning(bool running) {
    toggleButton_->setChecked(running);
    toggleButton_->setText(running ? tr("Stop Loop") : tr("Start Loop"));
    inputDeviceCombo_->setEnabled(!running);
    outputDeviceCombo_->setEnabled(!running);
}

void LoopPanel::setInputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(inputDeviceCombo_, deviceNames);
}

void LoopPanel::setOutputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(outputDeviceCombo_, deviceNames);
}

void LoopPanel::setSelectedInputDevice(const QString& deviceName) {
    setSelectedDeviceInCombo(inputDeviceCombo_, deviceName);
}

void LoopPanel::setSelectedOutputDevice(const QString& deviceName) {
    setSelectedDeviceInCombo(outputDeviceCombo_, deviceName);
}

void LoopPanel::setKeepLoopingChecked(bool checked) {
    const QSignalBlocker blocker(keepLoopingCheckBox_);
    keepLoopingCheckBox_->setChecked(checked);
}

}  // namespace sound_mind::studio
