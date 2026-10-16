#include "sound_mind/studio/numeric_field.h"
#include "sound_mind/studio/device_configuration_widget.h"

#include <algorithm>

#include <QComboBox>
#include <QCheckBox>
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

    // --- Output processing (v0.Y.80.1) ---------------------------------------
    root->addWidget(new QLabel(tr("Output processing"), this));
    auto* processingForm = new QFormLayout();
    limiterCheckBox_ = new QCheckBox(tr("Master limiter"), this);
    limiterCheckBox_->setObjectName(QStringLiteral("masterLimiterCheckBox"));
    limiterCheckBox_->setChecked(true);
    limiterCheckBox_->setToolTip(tr("Turn the mix down smoothly wherever it would go over the ceiling, instead of "
                                    "letting it distort - in playback and in exports"));
    limiterCeilingField_ = new NumericField(this);
    limiterCeilingField_->setObjectName(QStringLiteral("masterLimiterCeilingField"));
    limiterCeilingField_->setRange(-24.0, 0.0);
    limiterCeilingField_->setDecimals(1);
    limiterCeilingField_->setValue(-1.0);
    limiterCeilingField_->setToolTip(tr("The highest level the output may reach, dBFS"));
    processingForm->addRow(limiterCheckBox_, limiterCeilingField_);
    normalizationCheckBox_ = new QCheckBox(tr("Normalize loudness"), this);
    normalizationCheckBox_->setObjectName(QStringLiteral("loudnessNormalizationCheckBox"));
    normalizationCheckBox_->setToolTip(tr("Play every mix at the same loudness, turning the output up or down toward "
                                          "the target - playback only, exports keep their own level"));
    normalizationTargetField_ = new NumericField(this);
    normalizationTargetField_->setObjectName(QStringLiteral("loudnessTargetField"));
    normalizationTargetField_->setRange(-40.0, -5.0);
    normalizationTargetField_->setDecimals(1);
    normalizationTargetField_->setValue(-14.0);
    normalizationTargetField_->setToolTip(tr("The target loudness, LUFS - many streaming services use -14"));
    processingForm->addRow(normalizationCheckBox_, normalizationTargetField_);
    root->addLayout(processingForm);
    const auto emitLimiter = [this]() {
        emit masterLimiterChanged(limiterCheckBox_->isChecked(), limiterCeilingField_->value());
    };
    connect(limiterCheckBox_, &QCheckBox::toggled, this, emitLimiter);
    connect(limiterCeilingField_, &NumericField::valueChanged, this, emitLimiter);
    const auto emitNormalization = [this]() {
        emit loudnessNormalizationChanged(normalizationCheckBox_->isChecked(), normalizationTargetField_->value());
    };
    connect(normalizationCheckBox_, &QCheckBox::toggled, this, emitNormalization);
    connect(normalizationTargetField_, &NumericField::valueChanged, this, emitNormalization);

    root->addStretch();

    populateDeviceCombo(inputDeviceCombo_, {});
    populateDeviceCombo(outputDeviceCombo_, {});
}

void DeviceConfigurationWidget::setMasterLimiter(bool enabled, double ceilingDb) {
    const QSignalBlocker checkBlocker(limiterCheckBox_);
    const QSignalBlocker fieldBlocker(limiterCeilingField_);
    limiterCheckBox_->setChecked(enabled);
    limiterCeilingField_->setValue(ceilingDb);
}

void DeviceConfigurationWidget::setLoudnessNormalization(bool enabled, double targetLufs) {
    const QSignalBlocker checkBlocker(normalizationCheckBox_);
    const QSignalBlocker fieldBlocker(normalizationTargetField_);
    normalizationCheckBox_->setChecked(enabled);
    normalizationTargetField_->setValue(targetLufs);
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
