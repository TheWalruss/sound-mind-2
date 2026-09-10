#include "test_tool_configuration_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/tool_configuration_panel.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::ToolConfiguration;
using sound_mind::studio::ToolConfigurationPanel;

void ToolConfigurationPanelTest::freshPanelIsAnOpaqueCircularBrush() {
    const ToolConfigurationPanel panel;
    const ToolConfiguration& config = panel.toolConfiguration();
    QCOMPARE(config.tipShape(), BrushTipShape::Circle);
    QCOMPARE(config.defaultGradient().stops().front().leftOpacity, 1.0f);
    QCOMPARE(config.defaultGradient().stops().front().rightOpacity, 1.0f);
}

void ToolConfigurationPanelTest::freshPanelHasBothOverlayCheckboxesOff() {
    const ToolConfigurationPanel panel;
    auto* boundingBoxes = panel.findChild<QCheckBox*>(QStringLiteral("showBoundingBoxesCheckBox"));
    auto* pathGeometry = panel.findChild<QCheckBox*>(QStringLiteral("showPathGeometryCheckBox"));
    QVERIFY(boundingBoxes != nullptr);
    QVERIFY(pathGeometry != nullptr);
    QVERIFY(!boundingBoxes->isChecked());
    QVERIFY(!pathGeometry->isChecked());
}

void ToolConfigurationPanelTest::changingTheTipShapeEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("tipShapeCombo"));
    QVERIFY(combo != nullptr);

    std::optional<ToolConfiguration> received;
    connect(&panel, &ToolConfigurationPanel::toolConfigurationChanged,
            [&](const ToolConfiguration& config) { received = config; });

    combo->setCurrentIndex(combo->findText(QStringLiteral("Diamond")));

    QVERIFY(received.has_value());
    QCOMPARE(received->tipShape(), BrushTipShape::Diamond);
    QCOMPARE(panel.toolConfiguration().tipShape(), BrushTipShape::Diamond);
}

void ToolConfigurationPanelTest::changingFalloffEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    spinBox->setValue(0.75);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().falloff(), 0.75f);
}

void ToolConfigurationPanelTest::changingSizeEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    spinBox->setValue(2.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().size(), 2.5);
}

void ToolConfigurationPanelTest::changingIntensitySetsBothGradientStopsIntensity() {
    ToolConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("intensitySpinBox"));
    QVERIFY(spinBox != nullptr);

    spinBox->setValue(-20.0);

    const auto& stops = panel.toolConfiguration().defaultGradient().stops();
    QCOMPARE(stops.front().leftIntensity, -20.0f);
    QCOMPARE(stops.front().rightIntensity, -20.0f);
    QCOMPARE(stops.back().leftIntensity, -20.0f);
    QCOMPARE(stops.back().rightIntensity, -20.0f);
}

void ToolConfigurationPanelTest::changingOpacitySetsBothGradientStopsOpacity() {
    ToolConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("opacitySpinBox"));
    QVERIFY(spinBox != nullptr);

    spinBox->setValue(50.0);

    const auto& stops = panel.toolConfiguration().defaultGradient().stops();
    QCOMPARE(stops.front().leftOpacity, 0.5f);
    QCOMPARE(stops.front().rightOpacity, 0.5f);
}

void ToolConfigurationPanelTest::togglingShowBoundingBoxesEmitsItsOwnSignal() {
    ToolConfigurationPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("showBoundingBoxesCheckBox"));
    QVERIFY(checkBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::showBoundingBoxesChanged);

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}

void ToolConfigurationPanelTest::togglingShowPathGeometryEmitsItsOwnSignal() {
    ToolConfigurationPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("showPathGeometryCheckBox"));
    QVERIFY(checkBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::showPathGeometryChanged);

    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}
