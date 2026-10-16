#include "test_device_configuration_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest/QtTest>

#include "sound_mind/studio/device_configuration_widget.h"
#include "sound_mind/studio/numeric_field.h"

using sound_mind::studio::DeviceConfigurationWidget;

// v0.Y.62.1 Installment H - DeviceConfigurationWidget is the actual
// implementation ConfigureDevicesPanel now just forwards to (see its own
// class docs); most of these mirror test_configure_devices_panel.cpp's own
// cases, confirming the extracted widget works standalone, not just
// through that wrapper. setSelectedInputDevice()/setSelectedOutputDevice()
// are net new - the sync mechanism MainWindow needs to keep this widget's
// own instance and the dock's own instance showing the same selection.

void DeviceConfigurationWidgetTest::refreshButtonEmitsRefreshRequested() {
    DeviceConfigurationWidget widget;
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::refreshRequested);

    auto* button = widget.findChild<QPushButton*>(QStringLiteral("refreshDevicesButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void DeviceConfigurationWidgetTest::setInputDevicesListsSystemDefaultFirst() {
    DeviceConfigurationWidget widget;
    widget.setInputDevices({QStringLiteral("Mic A"), QStringLiteral("Mic B")});

    auto* combo = widget.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemData(0).toString(), QString());
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Mic A"));
    QCOMPARE(combo->itemData(2).toString(), QStringLiteral("Mic B"));
}

void DeviceConfigurationWidgetTest::setOutputDevicesListsSystemDefaultFirst() {
    DeviceConfigurationWidget widget;
    widget.setOutputDevices({QStringLiteral("Speakers A")});

    auto* combo = widget.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemData(0).toString(), QString());
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Speakers A"));
}

void DeviceConfigurationWidgetTest::changingTheInputDeviceEmitsInputDeviceChanged() {
    DeviceConfigurationWidget widget;
    widget.setInputDevices({QStringLiteral("Mic A")});
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::inputDeviceChanged);

    auto* combo = widget.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Mic A"));
}

void DeviceConfigurationWidgetTest::changingTheOutputDeviceEmitsOutputDeviceChanged() {
    DeviceConfigurationWidget widget;
    widget.setOutputDevices({QStringLiteral("Speakers A")});
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::outputDeviceChanged);

    auto* combo = widget.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Speakers A"));
}

void DeviceConfigurationWidgetTest::gainSlidersStartAtUnityAndAllowAboveIt() {
    DeviceConfigurationWidget widget;
    auto* inputSlider = widget.findChild<QSlider*>(QStringLiteral("inputGainSlider"));
    auto* outputSlider = widget.findChild<QSlider*>(QStringLiteral("outputGainSlider"));
    QVERIFY(inputSlider != nullptr);
    QVERIFY(outputSlider != nullptr);
    QCOMPARE(inputSlider->value(), 100);
    QCOMPARE(outputSlider->value(), 100);
    QCOMPARE(inputSlider->maximum(), DeviceConfigurationWidget::kMaxGainPercent);
}

void DeviceConfigurationWidgetTest::movingTheInputGainSliderEmitsInputGainPercentChanged() {
    DeviceConfigurationWidget widget;
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::inputGainPercentChanged);

    auto* slider = widget.findChild<QSlider*>(QStringLiteral("inputGainSlider"));
    slider->setValue(150);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 150);
}

void DeviceConfigurationWidgetTest::setInputGainPercentDoesNotEmitInputGainPercentChanged() {
    DeviceConfigurationWidget widget;
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::inputGainPercentChanged);

    widget.setInputGainPercent(150);

    QCOMPARE(spy.count(), 0);
    auto* slider = widget.findChild<QSlider*>(QStringLiteral("inputGainSlider"));
    QCOMPARE(slider->value(), 150);
}

void DeviceConfigurationWidgetTest::testButtonsEmitTestToggledWithTheirCheckedState() {
    DeviceConfigurationWidget widget;
    QSignalSpy inputSpy(&widget, &DeviceConfigurationWidget::testInputToggled);
    QSignalSpy outputSpy(&widget, &DeviceConfigurationWidget::testOutputToggled);

    auto* inputButton = widget.findChild<QPushButton*>(QStringLiteral("testInputButton"));
    auto* outputButton = widget.findChild<QPushButton*>(QStringLiteral("testOutputButton"));
    inputButton->click();
    outputButton->click();

    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(inputSpy.at(0).at(0).toBool(), true);
    QCOMPARE(outputSpy.count(), 1);
    QCOMPARE(outputSpy.at(0).at(0).toBool(), true);
}

void DeviceConfigurationWidgetTest::setInputLevelUpdatesTheLevelBarClamped() {
    DeviceConfigurationWidget widget;
    auto* bar = widget.findChild<QProgressBar*>(QStringLiteral("inputLevelBar"));
    QCOMPARE(bar->value(), 0);

    widget.setInputLevel(0.5f);
    QCOMPARE(bar->value(), 50);

    widget.setInputLevel(2.0f);  // above the documented [0, 1] range - clamped.
    QCOMPARE(bar->value(), 100);
}

void DeviceConfigurationWidgetTest::setTestingInputAndOutputChangeCheckedStateWithoutEmittingSignals() {
    DeviceConfigurationWidget widget;
    QSignalSpy inputSpy(&widget, &DeviceConfigurationWidget::testInputToggled);
    auto* inputButton = widget.findChild<QPushButton*>(QStringLiteral("testInputButton"));

    widget.setTestingInput(true);

    QCOMPARE(inputSpy.count(), 0);
    QVERIFY(inputButton->isChecked());
}

void DeviceConfigurationWidgetTest::setInputDeviceSelectionEnabledTogglesTheInputComboOnly() {
    DeviceConfigurationWidget widget;
    auto* inputCombo = widget.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    auto* outputCombo = widget.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));

    widget.setInputDeviceSelectionEnabled(false);

    QVERIFY(!inputCombo->isEnabled());
    QVERIFY(outputCombo->isEnabled());
}

void DeviceConfigurationWidgetTest::setSelectedInputDeviceSelectsItWithoutEmittingInputDeviceChanged() {
    DeviceConfigurationWidget widget;
    widget.setInputDevices({QStringLiteral("Mic A"), QStringLiteral("Mic B")});
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::inputDeviceChanged);

    widget.setSelectedInputDevice(QStringLiteral("Mic B"));

    QCOMPARE(spy.count(), 0);
    auto* combo = widget.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QCOMPARE(combo->currentText(), QStringLiteral("Mic B"));
}

void DeviceConfigurationWidgetTest::setSelectedOutputDeviceSelectsItWithoutEmittingOutputDeviceChanged() {
    DeviceConfigurationWidget widget;
    widget.setOutputDevices({QStringLiteral("Speakers A"), QStringLiteral("Speakers B")});
    QSignalSpy spy(&widget, &DeviceConfigurationWidget::outputDeviceChanged);

    widget.setSelectedOutputDevice(QStringLiteral("Speakers B"));

    QCOMPARE(spy.count(), 0);
    auto* combo = widget.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QCOMPARE(combo->currentText(), QStringLiteral("Speakers B"));
}

// v0.Y.80.1 Installment D - the "Output processing" group.
void DeviceConfigurationWidgetTest::outputProcessingDefaultsToLimiterOnAndNormalizationOff() {
    DeviceConfigurationWidget widget;
    QVERIFY(widget.findChild<QCheckBox*>(QStringLiteral("masterLimiterCheckBox"))->isChecked());
    QCOMPARE(widget.findChild<sound_mind::studio::NumericField*>(QStringLiteral("masterLimiterCeilingField"))->value(),
             -1.0);
    QVERIFY(!widget.findChild<QCheckBox*>(QStringLiteral("loudnessNormalizationCheckBox"))->isChecked());
    QCOMPARE(widget.findChild<sound_mind::studio::NumericField*>(QStringLiteral("loudnessTargetField"))->value(), -14.0);
}

void DeviceConfigurationWidgetTest::outputProcessingControlsEmitTheirSettings() {
    DeviceConfigurationWidget widget;
    QSignalSpy limiterSpy(&widget, &DeviceConfigurationWidget::masterLimiterChanged);
    QSignalSpy normalizationSpy(&widget, &DeviceConfigurationWidget::loudnessNormalizationChanged);

    widget.findChild<QCheckBox*>(QStringLiteral("masterLimiterCheckBox"))->setChecked(false);
    QCOMPARE(limiterSpy.count(), 1);
    QCOMPARE(limiterSpy.last().at(0).toBool(), false);
    QCOMPARE(limiterSpy.last().at(1).toDouble(), -1.0);
    widget.findChild<sound_mind::studio::NumericField*>(QStringLiteral("masterLimiterCeilingField"))->setValue(-3.0);
    QCOMPARE(limiterSpy.last().at(1).toDouble(), -3.0);

    widget.findChild<QCheckBox*>(QStringLiteral("loudnessNormalizationCheckBox"))->setChecked(true);
    QCOMPARE(normalizationSpy.count(), 1);
    QCOMPARE(normalizationSpy.last().at(0).toBool(), true);
    QCOMPARE(normalizationSpy.last().at(1).toDouble(), -14.0);
    widget.findChild<sound_mind::studio::NumericField*>(QStringLiteral("loudnessTargetField"))->setValue(-23.0);
    QCOMPARE(normalizationSpy.last().at(1).toDouble(), -23.0);
}

void DeviceConfigurationWidgetTest::outputProcessingSettersDoNotEmit() {
    DeviceConfigurationWidget widget;
    QSignalSpy limiterSpy(&widget, &DeviceConfigurationWidget::masterLimiterChanged);
    QSignalSpy normalizationSpy(&widget, &DeviceConfigurationWidget::loudnessNormalizationChanged);

    widget.setMasterLimiter(false, -6.0);
    widget.setLoudnessNormalization(true, -16.0);

    QCOMPARE(limiterSpy.count(), 0);
    QCOMPARE(normalizationSpy.count(), 0);
    QVERIFY(!widget.findChild<QCheckBox*>(QStringLiteral("masterLimiterCheckBox"))->isChecked());
    QCOMPARE(widget.findChild<sound_mind::studio::NumericField*>(QStringLiteral("masterLimiterCeilingField"))->value(),
             -6.0);
    QVERIFY(widget.findChild<QCheckBox*>(QStringLiteral("loudnessNormalizationCheckBox"))->isChecked());
    QCOMPARE(widget.findChild<sound_mind::studio::NumericField*>(QStringLiteral("loudnessTargetField"))->value(), -16.0);
}

