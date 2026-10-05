#include "sound_mind/studio/device_configuration_widget.h"

#include <algorithm>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "sound_mind/studio/device_combo_helpers.h"

namespace sound_mind::studio {

DeviceConfigurationWidget::DeviceConfigurationWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* refreshButton = new QPushButton(tr("Refresh Devices"), this);
    refreshButton->setObjectName(QStringLiteral("refreshDevicesButton"));
    connect(refreshButton, &QPushButton::clicked, this, &DeviceConfigurationWidget::refreshRequested);
    root->addWidget(refreshButton);

    // --- Input -------------------------------------------------------------
    root->addWidget(new QLabel(tr("Input"), this));

    auto* inputForm = new QFormLayout();
    inputDeviceCombo_ = new QComboBox(this);
    inputDeviceCombo_->setObjectName(QStringLiteral("inputDeviceCombo"));
    connect(inputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit inputDeviceChanged(inputDeviceCombo_->itemData(index).toString());
    });
    inputForm->addRow(tr("Device:"), inputDeviceCombo_);

    inputGainSlider_ = new QSlider(Qt::Horizontal, this);
    inputGainSlider_->setObjectName(QStringLiteral("inputGainSlider"));
    inputGainSlider_->setRange(0, kMaxGainPercent);
    inputGainSlider_->setValue(100);
    connect(inputGainSlider_, &QSlider::valueChanged, this, &DeviceConfigurationWidget::inputGainPercentChanged);
    inputForm->addRow(tr("Gain:"), inputGainSlider_);
    root->addLayout(inputForm);

    auto* inputTestRow = new QHBoxLayout();
    testInputButton_ = new QPushButton(tr("Test"), this);
    testInputButton_->setObjectName(QStringLiteral("testInputButton"));
    testInputButton_->setCheckable(true);
    connect(testInputButton_, &QPushButton::toggled, this, &DeviceConfigurationWidget::testInputToggled);
    inputTestRow->addWidget(testInputButton_);

    inputLevelBar_ = new QProgressBar(this);
    inputLevelBar_->setObjectName(QStringLiteral("inputLevelBar"));
    inputLevelBar_->setRange(0, 100);
    inputLevelBar_->setValue(0);
    inputLevelBar_->setTextVisible(false);
    inputTestRow->addWidget(inputLevelBar_, 1);
    root->addLayout(inputTestRow);

    // --- Output --------------------------------------------------------------
    root->addWidget(new QLabel(tr("Output"), this));

    auto* outputForm = new QFormLayout();
    outputDeviceCombo_ = new QComboBox(this);
    outputDeviceCombo_->setObjectName(QStringLiteral("outputDeviceCombo"));
    connect(outputDeviceCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit outputDeviceChanged(outputDeviceCombo_->itemData(index).toString());
    });
    outputForm->addRow(tr("Device:"), outputDeviceCombo_);

    outputGainSlider_ = new QSlider(Qt::Horizontal, this);
    outputGainSlider_->setObjectName(QStringLiteral("outputGainSlider"));
    outputGainSlider_->setRange(0, kMaxGainPercent);
    outputGainSlider_->setValue(100);
    connect(outputGainSlider_, &QSlider::valueChanged, this, &DeviceConfigurationWidget::outputGainPercentChanged);
    outputForm->addRow(tr("Gain:"), outputGainSlider_);
    root->addLayout(outputForm);

    testOutputButton_ = new QPushButton(tr("Test"), this);
    testOutputButton_->setObjectName(QStringLiteral("testOutputButton"));
    testOutputButton_->setCheckable(true);
    connect(testOutputButton_, &QPushButton::toggled, this, &DeviceConfigurationWidget::testOutputToggled);
    root->addWidget(testOutputButton_);

    root->addStretch();

    populateDeviceCombo(inputDeviceCombo_, {});
    populateDeviceCombo(outputDeviceCombo_, {});
}

void DeviceConfigurationWidget::setInputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(inputDeviceCombo_, deviceNames);
}

void DeviceConfigurationWidget::setOutputDevices(const QStringList& deviceNames) {
    populateDeviceCombo(outputDeviceCombo_, deviceNames);
}

void DeviceConfigurationWidget::setSelectedInputDevice(const QString& deviceName) {
    setSelectedDeviceInCombo(inputDeviceCombo_, deviceName);
}

void DeviceConfigurationWidget::setSelectedOutputDevice(const QString& deviceName) {
    setSelectedDeviceInCombo(outputDeviceCombo_, deviceName);
}

void DeviceConfigurationWidget::setInputGainPercent(int percent) {
    const QSignalBlocker blocker(inputGainSlider_);
    inputGainSlider_->setValue(std::clamp(percent, 0, kMaxGainPercent));
}

void DeviceConfigurationWidget::setOutputGainPercent(int percent) {
    const QSignalBlocker blocker(outputGainSlider_);
    outputGainSlider_->setValue(std::clamp(percent, 0, kMaxGainPercent));
}

void DeviceConfigurationWidget::setInputLevel(float level) {
    const int percent = static_cast<int>(std::clamp(level, 0.0f, 1.0f) * 100.0f);
    inputLevelBar_->setValue(percent);
}

void DeviceConfigurationWidget::setTestingInput(bool testing) {
    const QSignalBlocker blocker(testInputButton_);
    testInputButton_->setChecked(testing);
}

void DeviceConfigurationWidget::setTestingOutput(bool testing) {
    const QSignalBlocker blocker(testOutputButton_);
    testOutputButton_->setChecked(testing);
}

void DeviceConfigurationWidget::setInputDeviceSelectionEnabled(bool enabled) {
    inputDeviceCombo_->setEnabled(enabled);
}

void DeviceConfigurationWidget::setOutputDeviceSelectionEnabled(bool enabled) {
    outputDeviceCombo_->setEnabled(enabled);
}

}  // namespace sound_mind::studio
