#include "test_grid_panel.h"

#include <optional>
#include <set>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/grid_preset.h"
#include "sound_mind/core/music_theory.h"
#include "sound_mind/studio/grid_config.h"
#include "sound_mind/studio/grid_panel.h"

using sound_mind::core::FrequencyGridPresetConfig;
using sound_mind::core::GridPresetId;
using sound_mind::core::GridTimingPresetMode;
using sound_mind::core::NamedGridPreset;
using sound_mind::core::TimingGridPresetConfig;
using sound_mind::studio::FrequencyGridConfig;
using sound_mind::studio::GridPanel;
using sound_mind::studio::HorizontalAxisLabelMode;
using sound_mind::studio::TimingGridConfig;
using sound_mind::studio::TimingGridMode;
using sound_mind::studio::VerticalAxisLabelMode;

void GridPanelTest::freshPanelHasBothAxisLabelsOff() {
    const GridPanel panel;
    QCOMPARE(panel.verticalAxisLabelMode(), VerticalAxisLabelMode::Off);
    QCOMPARE(panel.horizontalAxisLabelMode(), HorizontalAxisLabelMode::Off);
}

void GridPanelTest::changingTheVerticalAxisComboEmitsVerticalAxisLabelModeChanged() {
    GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("verticalAxisCombo"));
    QVERIFY(combo != nullptr);

    std::optional<VerticalAxisLabelMode> received;
    connect(&panel, &GridPanel::verticalAxisLabelModeChanged, [&](VerticalAxisLabelMode mode) { received = mode; });

    combo->setCurrentIndex(combo->findText(QStringLiteral("Notes")));

    QVERIFY(received.has_value());
    QCOMPARE(*received, VerticalAxisLabelMode::Notes);
    QCOMPARE(panel.verticalAxisLabelMode(), VerticalAxisLabelMode::Notes);
}

void GridPanelTest::changingTheHorizontalAxisComboEmitsHorizontalAxisLabelModeChanged() {
    GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("horizontalAxisCombo"));
    QVERIFY(combo != nullptr);

    std::optional<HorizontalAxisLabelMode> received;
    connect(&panel, &GridPanel::horizontalAxisLabelModeChanged,
            [&](HorizontalAxisLabelMode mode) { received = mode; });

    combo->setCurrentIndex(combo->findText(QStringLiteral("Milliseconds")));

    QVERIFY(received.has_value());
    QCOMPARE(*received, HorizontalAxisLabelMode::Milliseconds);
    QCOMPARE(panel.horizontalAxisLabelMode(), HorizontalAxisLabelMode::Milliseconds);
}

void GridPanelTest::freshPanelHasNoFrequencyGridSourceActiveAndTimingGridOffAndSnapToGridOff() {
    const GridPanel panel;
    QVERIFY(!panel.frequencyGridConfig().isActive());
    QVERIFY(!panel.timingGridConfig().isActive());
    QCOMPARE(panel.timingGridConfig().mode, TimingGridMode::Off);
    QVERIFY(!panel.snapToGridEnabled());
}

void GridPanelTest::checkingNoteGridEmitsFrequencyGridConfigChanged() {
    GridPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("noteGridCheckBox"));
    QVERIFY(checkBox != nullptr);
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(panel.frequencyGridConfig().noteGridEnabled);
    QVERIFY(panel.frequencyGridConfig().isActive());
}

void GridPanelTest::checkingHarmonicSeriesEnablesTheFundamentalSpinBoxAndEmits() {
    GridPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("harmonicSeriesCheckBox"));
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicFundamentalSpinBox"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(spinBox != nullptr);
    QVERIFY(!spinBox->isEnabled());
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(panel.frequencyGridConfig().harmonicSeriesEnabled);
    QVERIFY(spinBox->isEnabled());
}

void GridPanelTest::changingTheHarmonicFundamentalEmitsFrequencyGridConfigChanged() {
    GridPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicFundamentalSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    spinBox->setValue(220.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.frequencyGridConfig().harmonicFundamentalHz, 220.0);
}

void GridPanelTest::checkingCustomFrequenciesEnablesTheLineEditAndEmits() {
    GridPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("customFrequenciesCheckBox"));
    auto* lineEdit = panel.findChild<QLineEdit*>(QStringLiteral("customFrequenciesLineEdit"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(lineEdit != nullptr);
    QVERIFY(!lineEdit->isEnabled());
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(panel.frequencyGridConfig().customFrequenciesEnabled);
    QVERIFY(lineEdit->isEnabled());
}

void GridPanelTest::typingCustomFrequenciesParsesACommaSeparatedListIntoTheConfig() {
    GridPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("customFrequenciesCheckBox"));
    auto* lineEdit = panel.findChild<QLineEdit*>(QStringLiteral("customFrequenciesLineEdit"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(lineEdit != nullptr);
    checkBox->setChecked(true);

    lineEdit->setText(QStringLiteral("220, 440  880"));

    const auto& frequencies = panel.frequencyGridConfig().customFrequenciesHz;
    QCOMPARE(frequencies.size(), std::size_t{3});
    QCOMPARE(frequencies[0], 220.0);
    QCOMPARE(frequencies[1], 440.0);
    QCOMPARE(frequencies[2], 880.0);
}

void GridPanelTest::changingTheFrequencyGridWidthEmitsFrequencyGridConfigChanged() {
    GridPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("frequencyGridWidthSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    spinBox->setValue(3.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.frequencyGridConfig().lineWidthPixels, 3.0);
}

void GridPanelTest::changingTheFrequencyGridDashStyleEmitsFrequencyGridConfigChanged() {
    GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("frequencyGridDashStyleCombo"));
    QVERIFY(combo != nullptr);
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    combo->setCurrentIndex(combo->findText(QStringLiteral("Dash")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.frequencyGridConfig().lineStyle, Qt::DashLine);
}

void GridPanelTest::freshPanelDefaultsToEqual12ChromaticWithKeyAndScaleEnabled() {
    const GridPanel panel;
    QCOMPARE(panel.frequencyGridConfig().noteGridTemperament, sound_mind::core::Temperament::Equal12);
    QCOMPARE(panel.frequencyGridConfig().noteGridScale, sound_mind::core::ScaleType::Chromatic);
    QVERIFY(panel.frequencyGridConfig().noteGridExcludedSteps.empty());
    QVERIFY(panel.frequencyGridConfig().noteGridExcludedOctaves.empty());
    auto* keyCombo = panel.findChild<QComboBox*>(QStringLiteral("keyCombo"));
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY(keyCombo != nullptr);
    QVERIFY(scaleCombo != nullptr);
    QVERIFY(keyCombo->isEnabled());
    QVERIFY(scaleCombo->isEnabled());
}

void GridPanelTest::changingTemperamentToANonKeyScaleOneDisablesKeyAndScaleCombosAndEmits() {
    GridPanel panel;
    auto* temperamentCombo = panel.findChild<QComboBox*>(QStringLiteral("temperamentCombo"));
    auto* keyCombo = panel.findChild<QComboBox*>(QStringLiteral("keyCombo"));
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY(temperamentCombo != nullptr);
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    temperamentCombo->setCurrentIndex(temperamentCombo->findText(QStringLiteral("19-TET")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.frequencyGridConfig().noteGridTemperament, sound_mind::core::Temperament::Equal19);
    QVERIFY(!keyCombo->isEnabled());
    QVERIFY(!scaleCombo->isEnabled());
    // 19-TET has no Key/Scale filtering at all - nothing should be
    // excluded just from switching to it.
    QVERIFY(panel.frequencyGridConfig().noteGridExcludedSteps.empty());
}

void GridPanelTest::changingTemperamentBackToAKeyScaleOneReEnablesTheCombos() {
    GridPanel panel;
    auto* temperamentCombo = panel.findChild<QComboBox*>(QStringLiteral("temperamentCombo"));
    auto* keyCombo = panel.findChild<QComboBox*>(QStringLiteral("keyCombo"));
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    temperamentCombo->setCurrentIndex(temperamentCombo->findText(QStringLiteral("19-TET")));
    QVERIFY(!keyCombo->isEnabled());

    temperamentCombo->setCurrentIndex(temperamentCombo->findText(QStringLiteral("24-TET (Quarter Tone)")));

    QVERIFY(keyCombo->isEnabled());
    QVERIFY(scaleCombo->isEnabled());
    QCOMPARE(panel.frequencyGridConfig().noteGridTemperament, sound_mind::core::Temperament::Equal24);
}

void GridPanelTest::changingScaleToMajorExcludesEveryNonScaleStepAndEmits() {
    GridPanel panel;
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY(scaleCombo != nullptr);
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    scaleCombo->setCurrentIndex(scaleCombo->findText(QStringLiteral("Major (Ionian)")));

    QCOMPARE(spy.count(), 1);
    // Key defaults to C - C Major excludes the 5 non-scale steps (the
    // "black keys"), leaving 7 included, under Equal12's own `step == 0
    // is A` convention: C Major = {C, D, E, F, G, A, B} = steps {3, 5, 7,
    // 8, 10, 0, 2} - the excluded ones are the other five.
    const auto& excluded = panel.frequencyGridConfig().noteGridExcludedSteps;
    QCOMPARE(excluded.size(), std::size_t{5});
    for (const int includedStep : {3, 5, 7, 8, 10, 0, 2}) {
        QVERIFY(excluded.count(includedStep) == 0);
    }
}

void GridPanelTest::changingKeyWithAMajorScaleRecomputesTheExcludedStepsForTheNewRoot() {
    GridPanel panel;
    auto* keyCombo = panel.findChild<QComboBox*>(QStringLiteral("keyCombo"));
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    scaleCombo->setCurrentIndex(scaleCombo->findText(QStringLiteral("Major (Ionian)")));
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    keyCombo->setCurrentIndex(keyCombo->findText(QStringLiteral("G")));

    QCOMPARE(spy.count(), 1);
    // G Major = {G, A, B, C, D, E, F#}, step-mapped (Equal12's own
    // `step == 0 is A` convention, so F natural = step 8, F# = step 9):
    // F natural is now excluded (G Major has F#, not F natural) where it
    // wasn't for C Major above, and F# is now included where it was
    // excluded for C Major.
    QVERIFY(panel.frequencyGridConfig().noteGridExcludedSteps.count(8) > 0);
    QVERIFY(panel.frequencyGridConfig().noteGridExcludedSteps.count(9) == 0);
}

void GridPanelTest::changingScaleToChromaticClearsEveryExclusion() {
    GridPanel panel;
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    scaleCombo->setCurrentIndex(scaleCombo->findText(QStringLiteral("Major (Ionian)")));
    QVERIFY(!panel.frequencyGridConfig().noteGridExcludedSteps.empty());

    scaleCombo->setCurrentIndex(scaleCombo->findText(QStringLiteral("Chromatic")));

    QVERIFY(panel.frequencyGridConfig().noteGridExcludedSteps.empty());
}

void GridPanelTest::changingTemperamentResetsExcludedStepsButLeavesExcludedOctavesAlone() {
    GridPanel panel;
    auto* temperamentCombo = panel.findChild<QComboBox*>(QStringLiteral("temperamentCombo"));
    auto* scaleCombo = panel.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    scaleCombo->setCurrentIndex(scaleCombo->findText(QStringLiteral("Major (Ionian)")));
    QVERIFY(!panel.frequencyGridConfig().noteGridExcludedSteps.empty());
    panel.applyOctaveSelection({false, true});  // Excludes octave -1 only.
    QVERIFY(!panel.frequencyGridConfig().noteGridExcludedOctaves.empty());

    // Switching to a non-Key/Scale temperament and back should reset the
    // step exclusions (a Major-scale selection re-applied fresh) without
    // touching the unrelated octave exclusion at all.
    temperamentCombo->setCurrentIndex(temperamentCombo->findText(QStringLiteral("19-TET")));
    QVERIFY(panel.frequencyGridConfig().noteGridExcludedSteps.empty());
    QVERIFY(!panel.frequencyGridConfig().noteGridExcludedOctaves.empty());

    temperamentCombo->setCurrentIndex(temperamentCombo->findText(QStringLiteral("12-TET")));
    QVERIFY(!panel.frequencyGridConfig().noteGridExcludedSteps.empty());  // Major re-applied.
    QVERIFY(!panel.frequencyGridConfig().noteGridExcludedOctaves.empty());  // Untouched throughout.
}

void GridPanelTest::applyNoteSelectionExcludesUncheckedStepsAndEmits() {
    GridPanel panel;
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    std::vector<bool> checked(12, true);
    checked[0] = false;  // Excludes step 0 (A).
    panel.applyNoteSelection(checked);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.frequencyGridConfig().noteGridExcludedSteps, std::set<int>{0});
}

void GridPanelTest::applyOctaveSelectionExcludesUncheckedOctavesAndEmits() {
    GridPanel panel;
    QSignalSpy spy(&panel, &GridPanel::frequencyGridConfigChanged);

    // This panel's own fixed Octaves range starts at -1 (see
    // grid_panel.cpp's own kMinOctave) - index 1 is octave 0.
    std::vector<bool> checked(12, true);
    checked[1] = false;
    panel.applyOctaveSelection(checked);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.frequencyGridConfig().noteGridExcludedOctaves, std::set<int>{0});
}

void GridPanelTest::changingTheTimingGridModeToIntervalEnablesTheIntervalSpinBoxAndEmits() {
    GridPanel panel;
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("timingGridModeCombo"));
    auto* intervalSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("timingGridIntervalSpinBox"));
    QVERIFY(modeCombo != nullptr);
    QVERIFY(intervalSpinBox != nullptr);
    QVERIFY(!intervalSpinBox->isEnabled());
    QSignalSpy spy(&panel, &GridPanel::timingGridConfigChanged);

    modeCombo->setCurrentIndex(modeCombo->findText(QStringLiteral("Fixed Interval")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.timingGridConfig().mode, TimingGridMode::Interval);
    QVERIFY(panel.timingGridConfig().isActive());
    QVERIFY(intervalSpinBox->isEnabled());
}

void GridPanelTest::changingTheTimingGridModeToTempoEnablesTheSubdivisionComboAndEmits() {
    GridPanel panel;
    auto* modeCombo = panel.findChild<QComboBox*>(QStringLiteral("timingGridModeCombo"));
    auto* subdivisionCombo = panel.findChild<QComboBox*>(QStringLiteral("timingGridSubdivisionCombo"));
    QVERIFY(modeCombo != nullptr);
    QVERIFY(subdivisionCombo != nullptr);
    QVERIFY(!subdivisionCombo->isEnabled());
    QSignalSpy spy(&panel, &GridPanel::timingGridConfigChanged);

    modeCombo->setCurrentIndex(modeCombo->findText(QStringLiteral("Tempo")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.timingGridConfig().mode, TimingGridMode::Tempo);
    QVERIFY(subdivisionCombo->isEnabled());
}

void GridPanelTest::changingTheTimingGridIntervalEmitsTimingGridConfigChanged() {
    GridPanel panel;
    auto* intervalSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("timingGridIntervalSpinBox"));
    QVERIFY(intervalSpinBox != nullptr);
    QSignalSpy spy(&panel, &GridPanel::timingGridConfigChanged);

    intervalSpinBox->setValue(2.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.timingGridConfig().intervalSeconds, 2.0);
}

void GridPanelTest::changingTheTimingGridSubdivisionEmitsTimingGridConfigChanged() {
    GridPanel panel;
    auto* subdivisionCombo = panel.findChild<QComboBox*>(QStringLiteral("timingGridSubdivisionCombo"));
    QVERIFY(subdivisionCombo != nullptr);
    QSignalSpy spy(&panel, &GridPanel::timingGridConfigChanged);

    subdivisionCombo->setCurrentIndex(subdivisionCombo->findText(QStringLiteral("Eighth")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.timingGridConfig().tempoBeatFraction, 0.5);
}

void GridPanelTest::togglingSnapToGridEmitsSnapToGridChanged() {
    GridPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("snapToGridCheckBox"));
    QVERIFY(checkBox != nullptr);
    QSignalSpy spy(&panel, &GridPanel::snapToGridChanged);

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QVERIFY(panel.snapToGridEnabled());
}

namespace {

NamedGridPreset makeGridPreset(GridPresetId id, const QString& name) {
    NamedGridPreset preset;
    preset.id = id;
    preset.name = name.toStdString();
    preset.frequencyGrid.harmonicSeriesEnabled = true;
    preset.frequencyGrid.harmonicFundamentalHz = 55.0;
    preset.timingGrid.mode = GridTimingPresetMode::Tempo;
    preset.timingGrid.tempoBeatFraction = 0.25;
    preset.snapToGridEnabled = true;
    return preset;
}

}  // namespace

void GridPanelTest::freshPanelShowsNoneSavedYetInThePresetCombo() {
    const GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("gridPresetCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 1);
    QCOMPARE(combo->currentText(), QStringLiteral("(none saved yet)"));
}

void GridPanelTest::clickingSaveButtonEmitsSavePresetRequested() {
    GridPanel panel;
    auto* saveButton = panel.findChild<QPushButton*>(QStringLiteral("savePresetButton"));
    QVERIFY(saveButton != nullptr);
    QSignalSpy spy(&panel, &GridPanel::savePresetRequested);

    saveButton->click();

    QCOMPARE(spy.count(), 1);
}

void GridPanelTest::setAvailableGridPresetsPopulatesTheComboByName() {
    GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("gridPresetCombo"));

    panel.setAvailableGridPresets({makeGridPreset(1, QStringLiteral("Grid A")), makeGridPreset(2, QStringLiteral("Grid B"))});

    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->itemText(0), QStringLiteral("Grid A"));
    QCOMPARE(combo->itemText(1), QStringLiteral("Grid B"));
}

void GridPanelTest::selectingAPresetAppliesItsConfigurationAndEmitsChangeSignals() {
    GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("gridPresetCombo"));
    // Two presets, not one - setAvailableGridPresets() itself already
    // (silently, signal-blocked) lands the combo on index 0 when nothing
    // was selected before, so re-selecting index 0 wouldn't be a real
    // change for QComboBox::currentIndexChanged to fire on; moving to
    // index 1 is.
    panel.setAvailableGridPresets(
        {makeGridPreset(1, QStringLiteral("Grid A")), makeGridPreset(7, QStringLiteral("Grid B"))});
    QSignalSpy frequencySpy(&panel, &GridPanel::frequencyGridConfigChanged);
    QSignalSpy timingSpy(&panel, &GridPanel::timingGridConfigChanged);
    QSignalSpy snapSpy(&panel, &GridPanel::snapToGridChanged);

    combo->setCurrentIndex(1);

    QCOMPARE(frequencySpy.count(), 1);
    QCOMPARE(timingSpy.count(), 1);
    QCOMPARE(snapSpy.count(), 1);
    QVERIFY(panel.frequencyGridConfig().harmonicSeriesEnabled);
    QCOMPARE(panel.frequencyGridConfig().harmonicFundamentalHz, 55.0);
    QCOMPARE(panel.timingGridConfig().mode, TimingGridMode::Tempo);
    QCOMPARE(panel.timingGridConfig().tempoBeatFraction, 0.25);
    QVERIFY(panel.snapToGridEnabled());
}

void GridPanelTest::clickingDeleteButtonEmitsDeletePresetRequestedForTheSelectedPreset() {
    GridPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("gridPresetCombo"));
    auto* deleteButton = panel.findChild<QPushButton*>(QStringLiteral("deletePresetButton"));
    panel.setAvailableGridPresets({makeGridPreset(42, QStringLiteral("Grid A"))});
    combo->setCurrentIndex(0);
    QSignalSpy spy(&panel, &GridPanel::deletePresetRequested);

    deleteButton->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toULongLong(), 42ULL);
}

void GridPanelTest::deleteButtonWithThePlaceholderSelectedEmitsNothing() {
    GridPanel panel;
    auto* deleteButton = panel.findChild<QPushButton*>(QStringLiteral("deletePresetButton"));
    QSignalSpy spy(&panel, &GridPanel::deletePresetRequested);

    deleteButton->click();

    QCOMPARE(spy.count(), 0);
}
