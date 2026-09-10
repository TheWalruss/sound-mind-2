#include "test_tool_configuration_panel.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
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
    // 0 dB on both channels is byte 255 on both red and green - a bright
    // yellow (no blue - painting doesn't touch phase yet) - see color()'s
    // own docs.
    QCOMPARE(panel.color(), QColor(255, 255, 0));
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

void ToolConfigurationPanelTest::setColorSetsBothGradientStopsIntensityAndEmitsChange() {
    ToolConfigurationPanel panel;
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    panel.setColor(QColor(128, 64, 255));  // blue is ignored - painting doesn't touch phase yet.

    QCOMPARE(spy.count(), 1);
    const auto& stops = panel.toolConfiguration().defaultGradient().stops();
    // byteToDbColor(128) ~= -47.8 dB, byteToDbColor(64) ~= -71.9 dB, over
    // the -96..0 dB display range dbToByteColor()/byteToDbColor() share
    // with color_mapping.cpp's own (unexported) formula.
    QVERIFY(qAbs(stops.front().leftIntensity - (-47.8f)) < 1.0f);
    QVERIFY(qAbs(stops.front().rightIntensity - (-71.9f)) < 1.0f);
    QCOMPARE(stops.back().leftIntensity, stops.front().leftIntensity);
    QCOMPARE(stops.back().rightIntensity, stops.front().rightIntensity);
}

void ToolConfigurationPanelTest::colorRoundTripsThroughSetColor() {
    ToolConfigurationPanel panel;

    panel.setColor(QColor(200, 40, 0));

    // Round-trips exactly for red/green (blue is always 0 - see color()'s
    // own docs) - dbToByteColor()/byteToDbColor() are exact inverses over
    // the 0-255 byte range.
    QCOMPARE(panel.color(), QColor(200, 40, 0));
}

void ToolConfigurationPanelTest::colorButtonExistsForOpeningTheRealDialog() {
    const ToolConfigurationPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("colorButton"));
    QVERIFY(button != nullptr);
    // Its own displayed swatch already matches color() - see
    // updateColorButtonAppearance()'s own docs - checked via the hex text
    // it sets alongside the background fill, not by parsing a stylesheet.
    QCOMPARE(button->text(), panel.color().name());
}
