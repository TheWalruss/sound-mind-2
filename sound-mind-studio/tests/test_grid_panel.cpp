#include "test_grid_panel.h"

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/grid_config.h"
#include "sound_mind/studio/grid_panel.h"

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
