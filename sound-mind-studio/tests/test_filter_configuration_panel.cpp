#include "test_filter_configuration_panel.h"

#include <optional>

#include <QDoubleSpinBox>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/filter_configuration_panel.h"

using sound_mind::core::FilterConfiguration;
using sound_mind::studio::FilterConfigurationPanel;

void FilterConfigurationPanelTest::freshPanelHasAFullyTransparentDefaultConfiguration() {
    const FilterConfigurationPanel panel;
    const auto& stops = panel.filterConfiguration().frequencyGradient().stops();
    QCOMPARE(stops.size(), std::size_t{2});
    QCOMPARE(stops.front().leftOpacity, 0.0f);
    QCOMPARE(stops.back().leftOpacity, 0.0f);
}

void FilterConfigurationPanelTest::changingAStartSpinBoxUpdatesStop0AndEmitsFilterConfigurationChanged() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("startLeftIntensitySpinBox"));
    QVERIFY(spinBox != nullptr);

    std::optional<FilterConfiguration> received;
    connect(&panel, &FilterConfigurationPanel::filterConfigurationChanged,
            [&](const FilterConfiguration& config) { received = config; });

    spinBox->setValue(-20.0);

    QVERIFY(received.has_value());
    QCOMPARE(received->frequencyGradient().stops().front().leftIntensity, -20.0f);
    QCOMPARE(panel.filterConfiguration().frequencyGradient().stops().front().leftIntensity, -20.0f);
    // The other endpoint stop is untouched.
    QCOMPARE(panel.filterConfiguration().frequencyGradient().stops().back().leftIntensity, 0.0f);
}

void FilterConfigurationPanelTest::changingAnEndSpinBoxUpdatesStop1AndEmitsFilterConfigurationChanged() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("endRightOpacitySpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(0.75);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().frequencyGradient().stops().back().rightOpacity, 0.75f);
    // The other endpoint stop is untouched.
    QCOMPARE(panel.filterConfiguration().frequencyGradient().stops().front().rightOpacity, 0.0f);
}

void FilterConfigurationPanelTest::setFilterConfigurationSyncsAllEightSpinBoxesWithoutEmitting() {
    FilterConfigurationPanel panel;
    FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -10.0f, -20.0f, 0.3f, 0.4f});
    config.frequencyGradient().setStopValues(1, {1.0f, -30.0f, -40.0f, 0.5f, 0.6f});
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.setFilterConfiguration(config);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("startLeftIntensitySpinBox"))->value(), -10.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("startRightIntensitySpinBox"))->value(), -20.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("startLeftOpacitySpinBox"))->value(), 0.3);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("startRightOpacitySpinBox"))->value(), 0.4);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("endLeftIntensitySpinBox"))->value(), -30.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("endRightIntensitySpinBox"))->value(), -40.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("endLeftOpacitySpinBox"))->value(), 0.5);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("endRightOpacitySpinBox"))->value(), 0.6);
}
