#include "test_selection_configuration_panel.h"

#include <optional>

#include <QComboBox>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/selection_configuration_panel.h"
#include "sound_mind/studio/selection_controller.h"

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
