#include "test_preset_indicator_label.h"

#include <QtTest/QtTest>

#include "sound_mind/studio/preset_indicator_label.h"

using sound_mind::studio::PresetIndicatorLabel;

void PresetIndicatorLabelTest::startsWithNoActivePreset() {
    const PresetIndicatorLabel label;
    QVERIFY(!label.hasActivePreset());
    QVERIFY(!label.isModified());
    QCOMPARE(label.text(), QStringLiteral("No preset active"));
}

void PresetIndicatorLabelTest::setActivePresetShowsTheNameAndClearsModified() {
    PresetIndicatorLabel label;
    label.setActivePreset(QStringLiteral("Warm"));
    QVERIFY(label.hasActivePreset());
    QVERIFY(!label.isModified());
    QCOMPARE(label.activePresetName(), QStringLiteral("Warm"));
    QCOMPARE(label.text(), QStringLiteral("Preset: Warm"));
}

void PresetIndicatorLabelTest::markModifiedAddsAnAsteriskAndIsIgnoredWithoutAnActivePreset() {
    PresetIndicatorLabel label;
    label.markModified();
    QVERIFY(!label.isModified());
    QCOMPARE(label.text(), QStringLiteral("No preset active"));

    label.setActivePreset(QStringLiteral("Warm"));
    label.markModified();
    QVERIFY(label.isModified());
    QCOMPARE(label.text(), QStringLiteral("Preset: Warm * (modified)"));
    QVERIFY(label.styleSheet().contains(QStringLiteral("#e08a1e")));
}

void PresetIndicatorLabelTest::setActivePresetAgainResetsModified() {
    PresetIndicatorLabel label;
    label.setActivePreset(QStringLiteral("Warm"));
    label.markModified();
    label.setActivePreset(QStringLiteral("Warm"));
    QVERIFY(!label.isModified());
    QCOMPARE(label.text(), QStringLiteral("Preset: Warm"));
}

void PresetIndicatorLabelTest::clearActivePresetReturnsToTheInactiveState() {
    PresetIndicatorLabel label;
    label.setActivePreset(QStringLiteral("Warm"));
    label.markModified();
    label.clearActivePreset();
    QVERIFY(!label.hasActivePreset());
    QVERIFY(!label.isModified());
    QCOMPARE(label.text(), QStringLiteral("No preset active"));
}
