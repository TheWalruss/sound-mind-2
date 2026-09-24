#include "test_selection_configuration_panel.h"

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QWidget>
#include <QtTest/QtTest>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/studio/selection_configuration_panel.h"
#include "sound_mind/studio/selection_controller.h"

using sound_mind::core::BlendMode;
using sound_mind::studio::SelectionConfigurationPanel;
using sound_mind::studio::SelectionShape;

void SelectionConfigurationPanelTest::freshPanelDefaultsToRectangle() {
    const SelectionConfigurationPanel panel;
    QCOMPARE(panel.selectionShape(), SelectionShape::Rectangle);
}

void SelectionConfigurationPanelTest::changingTheSelectionTypeComboEmitsSelectionShapeChangedAndUpdatesSelectionShape() {
    SelectionConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("selectionTypeCombo"));
    QVERIFY(combo != nullptr);

    std::optional<SelectionShape> received;
    connect(&panel, &SelectionConfigurationPanel::selectionShapeChanged,
            [&](SelectionShape shape) { received = shape; });

    combo->setCurrentIndex(combo->findText(QStringLiteral("Lasso")));

    QVERIFY(received.has_value());
    QCOMPARE(*received, SelectionShape::Lasso);
    QCOMPARE(panel.selectionShape(), SelectionShape::Lasso);
}

void SelectionConfigurationPanelTest::freshPanelHasTheWandGroupHiddenAndDefaultWandSettings() {
    const SelectionConfigurationPanel panel;
    auto* wandGroup = panel.findChild<QWidget*>(QStringLiteral("wandGroup"));
    QVERIFY(wandGroup != nullptr);
    QVERIFY(wandGroup->isHidden());
    QCOMPARE(panel.wandTolerance(), 10.0);
    QVERIFY(!panel.wandHarmonicsAware());
}

void SelectionConfigurationPanelTest::switchingToWandShowsTheWandGroupAndSwitchingAwayHidesItAgain() {
    SelectionConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("selectionTypeCombo"));
    auto* wandGroup = panel.findChild<QWidget*>(QStringLiteral("wandGroup"));
    QVERIFY(combo != nullptr);
    QVERIFY(wandGroup != nullptr);

    combo->setCurrentIndex(combo->findText(QStringLiteral("Wand")));
    QVERIFY(!wandGroup->isHidden());

    combo->setCurrentIndex(combo->findText(QStringLiteral("Rectangle")));
    QVERIFY(wandGroup->isHidden());
}

void SelectionConfigurationPanelTest::changingToleranceEmitsWandToleranceChangedAndUpdatesWandTolerance() {
    SelectionConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("wandToleranceSpinBox"));
    QVERIFY(spinBox != nullptr);

    std::optional<double> received;
    connect(&panel, &SelectionConfigurationPanel::wandToleranceChanged, [&](double tolerancePercent) {
        received = tolerancePercent;
    });

    spinBox->setValue(42.0);

    QVERIFY(received.has_value());
    QCOMPARE(*received, 42.0);
    QCOMPARE(panel.wandTolerance(), 42.0);
}

void SelectionConfigurationPanelTest::togglingHarmonicsAwareEmitsWandHarmonicsAwareChangedAndUpdatesWandHarmonicsAware() {
    SelectionConfigurationPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("wandHarmonicsAwareCheckBox"));
    QVERIFY(checkBox != nullptr);

    std::optional<bool> received;
    connect(&panel, &SelectionConfigurationPanel::wandHarmonicsAwareChanged,
            [&](bool harmonicsAware) { received = harmonicsAware; });

    checkBox->setChecked(true);

    QVERIFY(received.has_value());
    QVERIFY(*received);
    QVERIFY(panel.wandHarmonicsAware());
}

void SelectionConfigurationPanelTest::freshPanelDefaultsPasteBlendModeToOverwrite() {
    const SelectionConfigurationPanel panel;
    QCOMPARE(panel.pasteBlendMode(), BlendMode::Overwrite);
}

void SelectionConfigurationPanelTest::changingThePasteBlendModeComboUpdatesPasteBlendMode() {
    SelectionConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("pasteBlendModeCombo"));
    QVERIFY(combo != nullptr);

    combo->setCurrentIndex(combo->findText(QStringLiteral("Multiply")));

    QCOMPARE(panel.pasteBlendMode(), BlendMode::Multiply);
}

void SelectionConfigurationPanelTest::changingThePasteBlendModeComboEmitsPasteBlendModeChanged() {
    SelectionConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("pasteBlendModeCombo"));
    QVERIFY(combo != nullptr);
    std::optional<BlendMode> received;
    connect(&panel, &SelectionConfigurationPanel::pasteBlendModeChanged, [&](BlendMode mode) { received = mode; });

    combo->setCurrentIndex(combo->findText(QStringLiteral("Multiply")));

    QVERIFY(received.has_value());
    QCOMPARE(*received, BlendMode::Multiply);
}

void SelectionConfigurationPanelTest::setPasteBlendModeUpdatesTheComboWithoutEmittingPasteBlendModeChanged() {
    SelectionConfigurationPanel panel;
    QSignalSpy spy(&panel, &SelectionConfigurationPanel::pasteBlendModeChanged);

    panel.setPasteBlendMode(BlendMode::Multiply);

    QCOMPARE(panel.pasteBlendMode(), BlendMode::Multiply);
    QCOMPARE(spy.count(), 0);
}

void SelectionConfigurationPanelTest::clickingDeselectEmitsDeselectRequested() {
    SelectionConfigurationPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("deselectButton"));
    QVERIFY(button != nullptr);
    QSignalSpy spy(&panel, &SelectionConfigurationPanel::deselectRequested);

    button->click();

    QCOMPARE(spy.count(), 1);
}
