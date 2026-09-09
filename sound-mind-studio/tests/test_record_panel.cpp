#include "test_record_panel.h"

#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/record_panel.h"

using sound_mind::studio::RecordPanel;

void RecordPanelTest::toggleButtonEmitsToggleRequested() {
    RecordPanel panel;
    QSignalSpy spy(&panel, &RecordPanel::toggleRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void RecordPanelTest::setRecordingUpdatesButtonAndDisablesTheDevicePicker() {
    RecordPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(button != nullptr);
    QVERIFY(combo != nullptr);

    panel.setRecording(true);
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Recording"));
    QVERIFY(!combo->isEnabled());

    panel.setRecording(false);
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Recording"));
    QVERIFY(combo->isEnabled());
}

void RecordPanelTest::setInputDevicesListsSystemDefaultFirst() {
    RecordPanel panel;
    panel.setInputDevices({QStringLiteral("Microphone A")});

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemData(0).toString(), QString());
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Microphone A"));
}

void RecordPanelTest::changingTheInputDeviceEmitsInputDeviceChanged() {
    RecordPanel panel;
    panel.setInputDevices({QStringLiteral("Microphone A")});
    QSignalSpy spy(&panel, &RecordPanel::inputDeviceChanged);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("inputDeviceCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Microphone A"));
}
