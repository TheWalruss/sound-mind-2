#include "test_loop_panel.h"

#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/loop_panel.h"

using sound_mind::studio::LoopPanel;

void LoopPanelTest::toggleButtonEmitsToggleRequested() {
    LoopPanel panel;
    QSignalSpy spy(&panel, &LoopPanel::toggleRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void LoopPanelTest::setRunningUpdatesButtonAndDisablesDevicePickers() {
    LoopPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    auto* inputCombo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    auto* outputCombo = panel.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(button != nullptr);
    QVERIFY(inputCombo != nullptr);
    QVERIFY(outputCombo != nullptr);

    panel.setRunning(true);
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Loop"));
    QVERIFY(!inputCombo->isEnabled());
    QVERIFY(!outputCombo->isEnabled());

    panel.setRunning(false);
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Loop"));
    QVERIFY(inputCombo->isEnabled());
    QVERIFY(outputCombo->isEnabled());
}

void LoopPanelTest::keepLoopingCheckBoxEmitsKeepLoopingChanged() {
    LoopPanel panel;
    QSignalSpy spy(&panel, &LoopPanel::keepLoopingChanged);

    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);
    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}

void LoopPanelTest::setKeepLoopingCheckedDoesNotEmitKeepLoopingChanged() {
    LoopPanel panel;
    QSignalSpy spy(&panel, &LoopPanel::keepLoopingChanged);

    panel.setKeepLoopingChecked(true);

    QCOMPARE(spy.count(), 0);
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(checkBox->isChecked());
}

void LoopPanelTest::setInputDevicesListsSystemDefaultFirst() {
    LoopPanel panel;
    panel.setInputDevices({QStringLiteral("Microphone A"), QStringLiteral("Microphone B")});

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemData(0).toString(), QString());  // "(System Default)".
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Microphone A"));
    QCOMPARE(combo->itemData(2).toString(), QStringLiteral("Microphone B"));
}

void LoopPanelTest::changingTheInputDeviceEmitsInputDeviceChanged() {
    LoopPanel panel;
    panel.setInputDevices({QStringLiteral("Microphone A")});
    QSignalSpy spy(&panel, &LoopPanel::inputDeviceChanged);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Microphone A"));
}

void LoopPanelTest::changingTheOutputDeviceEmitsOutputDeviceChanged() {
    LoopPanel panel;
    panel.setOutputDevices({QStringLiteral("Speakers A")});
    QSignalSpy spy(&panel, &LoopPanel::outputDeviceChanged);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Speakers A"));
}
