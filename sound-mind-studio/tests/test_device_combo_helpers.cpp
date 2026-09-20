#include "test_device_combo_helpers.h"

#include <QComboBox>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/device_combo_helpers.h"

using sound_mind::studio::populateDeviceCombo;
using sound_mind::studio::setSelectedDeviceInCombo;

void DeviceComboHelpersTest::populateDeviceComboListsSystemDefaultFirst() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B")});

    QCOMPARE(combo.count(), 3);
    QCOMPARE(combo.itemData(0).toString(), QString());
    QCOMPARE(combo.itemData(1).toString(), QStringLiteral("Mic A"));
    QCOMPARE(combo.itemData(2).toString(), QStringLiteral("Mic B"));
    QCOMPARE(combo.currentIndex(), 0);
}

void DeviceComboHelpersTest::populateDeviceComboWithNoDevicesOnlyHasTheDefaultEntry() {
    QComboBox combo;
    populateDeviceCombo(&combo, {});

    QCOMPARE(combo.count(), 1);
    QCOMPARE(combo.itemData(0).toString(), QString());
}

void DeviceComboHelpersTest::populateDeviceComboPreservesSelectionWhenStillPresent() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B")});
    combo.setCurrentIndex(2);  // "Mic B"

    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B"), QStringLiteral("Mic C")});

    QCOMPARE(combo.currentData().toString(), QStringLiteral("Mic B"));
}

void DeviceComboHelpersTest::populateDeviceComboFallsBackToDefaultWhenSelectionDisappears() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B")});
    combo.setCurrentIndex(2);  // "Mic B"

    populateDeviceCombo(&combo, {QStringLiteral("Mic A")});  // "Mic B" no longer present.

    QCOMPARE(combo.currentIndex(), 0);
    QCOMPARE(combo.currentData().toString(), QString());
}

void DeviceComboHelpersTest::populateDeviceComboDoesNotEmitCurrentIndexChanged() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A")});
    combo.setCurrentIndex(1);
    QSignalSpy spy(&combo, &QComboBox::currentIndexChanged);

    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B")});

    QCOMPARE(spy.count(), 0);
}

void DeviceComboHelpersTest::setSelectedDeviceInComboSelectsAMatchingDevice() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B")});

    setSelectedDeviceInCombo(&combo, QStringLiteral("Mic B"));

    QCOMPARE(combo.currentIndex(), 2);
}

void DeviceComboHelpersTest::setSelectedDeviceInComboFallsBackToDefaultForAnUnknownName() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A")});
    combo.setCurrentIndex(1);

    setSelectedDeviceInCombo(&combo, QStringLiteral("Nonexistent Mic"));

    QCOMPARE(combo.currentIndex(), 0);
}

void DeviceComboHelpersTest::setSelectedDeviceInComboFallsBackToDefaultForAnEmptyName() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A")});
    combo.setCurrentIndex(1);

    setSelectedDeviceInCombo(&combo, QString());

    QCOMPARE(combo.currentIndex(), 0);
}

void DeviceComboHelpersTest::setSelectedDeviceInComboDoesNotEmitCurrentIndexChanged() {
    QComboBox combo;
    populateDeviceCombo(&combo, {QStringLiteral("Mic A"), QStringLiteral("Mic B")});
    QSignalSpy spy(&combo, &QComboBox::currentIndexChanged);

    setSelectedDeviceInCombo(&combo, QStringLiteral("Mic B"));

    QCOMPARE(spy.count(), 0);
}
