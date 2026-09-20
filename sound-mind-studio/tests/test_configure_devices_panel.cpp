#include "test_configure_devices_panel.h"

#include <QComboBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest/QtTest>

#include "sound_mind/studio/configure_devices_panel.h"

using sound_mind::studio::ConfigureDevicesPanel;

void ConfigureDevicesPanelTest::refreshButtonEmitsRefreshRequested() {
    ConfigureDevicesPanel panel;
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::refreshRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("refreshDevicesButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void ConfigureDevicesPanelTest::setInputDevicesListsSystemDefaultFirst() {
    ConfigureDevicesPanel panel;
    panel.setInputDevices({QStringLiteral("Mic A"), QStringLiteral("Mic B")});

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemData(0).toString(), QString());
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Mic A"));
    QCOMPARE(combo->itemData(2).toString(), QStringLiteral("Mic B"));
}

void ConfigureDevicesPanelTest::setOutputDevicesListsSystemDefaultFirst() {
    ConfigureDevicesPanel panel;
    panel.setOutputDevices({QStringLiteral("Speakers A"), QStringLiteral("Speakers B")});

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemData(0).toString(), QString());
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Speakers A"));
}

void ConfigureDevicesPanelTest::changingTheInputDeviceEmitsInputDeviceChanged() {
    ConfigureDevicesPanel panel;
    panel.setInputDevices({QStringLiteral("Mic A")});
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::inputDeviceChanged);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Mic A"));
}

void ConfigureDevicesPanelTest::changingTheOutputDeviceEmitsOutputDeviceChanged() {
    ConfigureDevicesPanel panel;
    panel.setOutputDevices({QStringLiteral("Speakers A")});
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::outputDeviceChanged);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Speakers A"));
}

void ConfigureDevicesPanelTest::gainSlidersStartAtUnityAndAllowAboveIt() {
    ConfigureDevicesPanel panel;
    auto* inputSlider = panel.findChild<QSlider*>(QStringLiteral("inputGainSlider"));
    auto* outputSlider = panel.findChild<QSlider*>(QStringLiteral("outputGainSlider"));
    QVERIFY(inputSlider != nullptr);
    QVERIFY(outputSlider != nullptr);
    QCOMPARE(inputSlider->value(), 100);
    QCOMPARE(outputSlider->value(), 100);
    QCOMPARE(inputSlider->maximum(), ConfigureDevicesPanel::kMaxGainPercent);
    QVERIFY(ConfigureDevicesPanel::kMaxGainPercent > 100);
}

void ConfigureDevicesPanelTest::movingTheInputGainSliderEmitsInputGainPercentChanged() {
    ConfigureDevicesPanel panel;
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::inputGainPercentChanged);

    auto* slider = panel.findChild<QSlider*>(QStringLiteral("inputGainSlider"));
    QVERIFY(slider != nullptr);
    slider->setValue(150);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 150);
}

void ConfigureDevicesPanelTest::movingTheOutputGainSliderEmitsOutputGainPercentChanged() {
    ConfigureDevicesPanel panel;
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::outputGainPercentChanged);

    auto* slider = panel.findChild<QSlider*>(QStringLiteral("outputGainSlider"));
    QVERIFY(slider != nullptr);
    slider->setValue(50);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 50);
}

void ConfigureDevicesPanelTest::setInputGainPercentDoesNotEmitInputGainPercentChanged() {
    ConfigureDevicesPanel panel;
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::inputGainPercentChanged);

    panel.setInputGainPercent(150);

    QCOMPARE(spy.count(), 0);
    auto* slider = panel.findChild<QSlider*>(QStringLiteral("inputGainSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 150);
}

void ConfigureDevicesPanelTest::setOutputGainPercentDoesNotEmitOutputGainPercentChanged() {
    ConfigureDevicesPanel panel;
    QSignalSpy spy(&panel, &ConfigureDevicesPanel::outputGainPercentChanged);

    panel.setOutputGainPercent(50);

    QCOMPARE(spy.count(), 0);
    auto* slider = panel.findChild<QSlider*>(QStringLiteral("outputGainSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 50);
}

void ConfigureDevicesPanelTest::testButtonsEmitTestToggledWithTheirCheckedState() {
    ConfigureDevicesPanel panel;
    QSignalSpy inputSpy(&panel, &ConfigureDevicesPanel::testInputToggled);
    QSignalSpy outputSpy(&panel, &ConfigureDevicesPanel::testOutputToggled);

    auto* inputButton = panel.findChild<QPushButton*>(QStringLiteral("testInputButton"));
    auto* outputButton = panel.findChild<QPushButton*>(QStringLiteral("testOutputButton"));
    QVERIFY(inputButton != nullptr);
    QVERIFY(outputButton != nullptr);
    QVERIFY(inputButton->isCheckable());
    QVERIFY(outputButton->isCheckable());

    inputButton->click();
    outputButton->click();

    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(inputSpy.at(0).at(0).toBool(), true);
    QCOMPARE(outputSpy.count(), 1);
    QCOMPARE(outputSpy.at(0).at(0).toBool(), true);

    inputButton->click();
    QCOMPARE(inputSpy.count(), 2);
    QCOMPARE(inputSpy.at(1).at(0).toBool(), false);
}

void ConfigureDevicesPanelTest::setInputLevelUpdatesTheLevelBarClamped() {
    ConfigureDevicesPanel panel;
    auto* bar = panel.findChild<QProgressBar*>(QStringLiteral("inputLevelBar"));
    QVERIFY(bar != nullptr);
    QCOMPARE(bar->value(), 0);

    panel.setInputLevel(0.5f);
    QCOMPARE(bar->value(), 50);

    panel.setInputLevel(2.0f);  // above the documented [0, 1] range - clamped.
    QCOMPARE(bar->value(), 100);

    panel.setInputLevel(-1.0f);
    QCOMPARE(bar->value(), 0);
}

void ConfigureDevicesPanelTest::setTestingInputAndOutputChangeCheckedStateWithoutEmittingSignals() {
    ConfigureDevicesPanel panel;
    QSignalSpy inputSpy(&panel, &ConfigureDevicesPanel::testInputToggled);
    QSignalSpy outputSpy(&panel, &ConfigureDevicesPanel::testOutputToggled);
    auto* inputButton = panel.findChild<QPushButton*>(QStringLiteral("testInputButton"));
    auto* outputButton = panel.findChild<QPushButton*>(QStringLiteral("testOutputButton"));
    QVERIFY(inputButton != nullptr);
    QVERIFY(outputButton != nullptr);

    panel.setTestingInput(true);
    panel.setTestingOutput(true);

    QCOMPARE(inputSpy.count(), 0);
    QCOMPARE(outputSpy.count(), 0);
    QVERIFY(inputButton->isChecked());
    QVERIFY(outputButton->isChecked());

    panel.setTestingInput(false);
    QCOMPARE(inputSpy.count(), 0);
    QVERIFY(!inputButton->isChecked());
}
