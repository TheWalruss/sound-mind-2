#include "test_grid_panel.h"

#include <optional>

#include <QComboBox>
#include <QtTest/QtTest>

#include "sound_mind/studio/grid_panel.h"

using sound_mind::studio::GridPanel;
using sound_mind::studio::HorizontalAxisLabelMode;
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
