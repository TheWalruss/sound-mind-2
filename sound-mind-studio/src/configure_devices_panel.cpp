#include "sound_mind/studio/configure_devices_panel.h"

#include <algorithm>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
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
/// followed by `deviceNames`, preserving the previously selected device
/// name across the refresh if it's still present - the same helper
/// `PlaybackPanel`/`LoopPanel` each already have their own copy of.
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

ConfigureDevicesPanel::ConfigureDevicesPanel(QWidget* parent) : QDockWidget(tr("Configure Devices"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* refreshButton = new QPushButton(tr("Refresh Devices"), container);
    refreshButton->setObjectName(QStringLiteral("refreshDevicesButton"));
    connect(refreshButton, &QPushButton::clicked, this, &ConfigureDevicesPanel::refreshRequested);
    root->addWidget(refreshButton);

    // --- Input -------------------------------------------------------------
    root->addWidget(new QLabel(tr("Input"), container));

    auto* inputForm = new QFormLayout();
    inputDeviceCombo_ = new QComboBox(container);
    inputDeviceCombo_->setObjectName(QStringLiteral("inputDeviceCombo"));
    connect(inputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit inputDeviceChanged(inputDeviceCombo_->itemData(index).toString());
    });
    inputForm->addRow(tr("Device:"), inputDeviceCombo_);

    inputGainSlider_ = new QSlider(Qt::Horizontal, container);
    inputGainSlider_->setObjectName(QStringLiteral("inputGainSlider"));
    inputGainSlider_->setRange(0, kMaxGainPercent);
    inputGainSlider_->setValue(100);
    connect(inputGainSlider_, &QSlider::valueChanged, this, &ConfigureDevicesPanel::inputGainPercentChanged);
    inputForm->addRow(tr("Gain:"), inputGainSlider_);
    root->addLayout(inputForm);

    auto* inputTestRow = new QHBoxLayout();
    testInputButton_ = new QPushButton(tr("Test"), container);
    testInputButton_->setObjectName(QStringLiteral("testInputButton"));
    testInputButton_->setCheckable(true);
    connect(testInputButton_, &QPushButton::toggled, this, &ConfigureDevicesPanel::testInputToggled);
    inputTestRow->addWidget(testInputButton_);

    inputLevelBar_ = new QProgressBar(container);
    inputLevelBar_->setObjectName(QStringLiteral("inputLevelBar"));
    inputLevelBar_->setRange(0, 100);
    inputLevelBar_->setValue(0);
    inputLevelBar_->setTextVisible(false);
    inputTestRow->addWidget(inputLevelBar_, 1);
    root->addLayout(inputTestRow);

    // --- Output --------------------------------------------------------------
    root->addWidget(new QLabel(tr("Output"), container));

    auto* outputForm = new QFormLayout();
    outputDeviceCombo_ = new QComboBox(container);
    outputDeviceCombo_->setObjectName(QStringLiteral("outputDeviceCombo"));
    connect(outputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit outputDeviceChanged(outputDeviceCombo_->itemData(index).toString());
    });
    outputForm->addRow(tr("Device:"), outputDeviceCombo_);

    outputGainSlider_ = new QSlider(Qt::Horizontal, container);
    outputGainSlider_->setObjectName(QStringLiteral("outputGainSlider"));
    outputGainSlider_->setRange(0, kMaxGainPercent);
    outputGainSlider_->setValue(100);
    connect(outputGainSlider_, &QSlider::valueChanged, this, &ConfigureDevicesPanel::outputGainPercentChanged);
    outputForm->addRow(tr("Gain:"), outputGainSlider_);
    root->addLayout(outputForm);

    testOutputButton_ = new QPushButton(tr("Test"), container);
    testOutputButton_->setObjectName(QStringLiteral("testOutputButton"));
    testOutputButton_->setCheckable(true);
    connect(testOutputButton_, &QPushButton::toggled, this, &ConfigureDevicesPanel::testOutputToggled);
    root->addWidget(testOutputButton_);

    root->addStretch();

    populateDeviceCombo(inputDeviceCombo_, {});
    populateDeviceCombo(outputDeviceCombo_, {});

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void ConfigureDevicesPanel::setInputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(inputDeviceCombo_, deviceNames);
}

void ConfigureDevicesPanel::setOutputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(outputDeviceCombo_, deviceNames);
}

void ConfigureDevicesPanel::setInputGainPercent(int percent) {
    const QSignalBlocker blocker(inputGainSlider_);
    inputGainSlider_->setValue(std::clamp(percent, 0, kMaxGainPercent));
}

void ConfigureDevicesPanel::setOutputGainPercent(int percent) {
    const QSignalBlocker blocker(outputGainSlider_);
    outputGainSlider_->setValue(std::clamp(percent, 0, kMaxGainPercent));
}

void ConfigureDevicesPanel::setInputLevel(float level) {
    const int percent = static_cast<int>(std::clamp(level, 0.0f, 1.0f) * 100.0f);
    inputLevelBar_->setValue(percent);
}

void ConfigureDevicesPanel::setTestingInput(bool testing) {
    const QSignalBlocker blocker(testInputButton_);
    testInputButton_->setChecked(testing);
}

void ConfigureDevicesPanel::setTestingOutput(bool testing) {
    const QSignalBlocker blocker(testOutputButton_);
    testOutputButton_->setChecked(testing);
}

}  // namespace sound_mind::studio
