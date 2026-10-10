#include "test_tool_configuration_panel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_shot.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/gradient_editor_widget.h"
#include "sound_mind/studio/harmonic_series_widget.h"
#include "sound_mind/studio/tool_configuration_panel.h"

using sound_mind::core::BlendMode;
using sound_mind::core::BrushTipShape;
using sound_mind::core::Clip;
using sound_mind::core::HealConfiguration;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::MindGrainId;
using sound_mind::core::MindShotConfiguration;
using sound_mind::core::MindShotId;
using sound_mind::core::MindWaveBindingFrame;
using sound_mind::core::MindWaveId;
using sound_mind::core::OrderChaosConfiguration;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::ResonanceConfiguration;
using sound_mind::core::ResonanceProfileId;
using sound_mind::core::SmudgeConfiguration;
using sound_mind::core::SoftenConfiguration;
using sound_mind::core::StampMode;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::ToolPresetId;
using sound_mind::core::ToolType;
using sound_mind::studio::GradientEditorWidget;
using sound_mind::studio::HarmonicSeriesWidget;
using sound_mind::studio::ToolConfigurationPanel;

namespace {

Clip makeTestClip() {
    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.sharedPhaseRadians = {0.0f, 0.0f, 0.0f, 0.0f};
    return clip;
}

/// @brief See test_harmonic_series_widget.cpp's own identical `yPosFor()` -
/// kept as its own local copy, matching test_filter_configuration_panel.cpp's
/// own `xPosFor()` comment on why. Assumes the widget's own default
/// `sizeHint()` (`240x100`).
int harmonicYPosFor(double value) {
    constexpr double margin = 8.0;
    constexpr double height = 100.0 - 2 * margin;
    constexpr double bottom = 100.0 - margin;
    constexpr double displayMax = 2.0;
    return static_cast<int>(bottom - (value / displayMax) * height);
}

/// @brief See test_harmonic_series_widget.cpp's own identical
/// `xPosForColumn()` - kept as its own local copy, for the same reason.
int harmonicXPosForColumn(int column, std::size_t count) {
    constexpr double margin = 8.0;
    const double width = 240.0 - 2 * margin;
    const double columnWidth = width / static_cast<double>(count);
    return static_cast<int>(margin + (static_cast<double>(column) + 0.5) * columnWidth);
}

}  // namespace

void ToolConfigurationPanelTest::freshPanelIsAnOpaqueCircularBrush() {
    const ToolConfigurationPanel panel;
    const ToolConfiguration& config = panel.toolConfiguration();
    QCOMPARE(config.type(), ToolType::Procedural);
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(config).tipShape(), BrushTipShape::Circle);
    QCOMPARE(config.defaultGradient().stops().front().leftOpacity, 1.0f);
    QCOMPARE(config.defaultGradient().stops().front().rightOpacity, 1.0f);
    // 0 dB on both channels - the loudest a stop can be.
    QCOMPARE(config.defaultGradient().stops().front().leftIntensity, 0.0f);
    QCOMPARE(config.defaultGradient().stops().front().rightIntensity, 0.0f);
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

    std::unique_ptr<ToolConfiguration> received;
    connect(&panel, &ToolConfigurationPanel::toolConfigurationChanged,
            [&](const ToolConfiguration& config) { received = config.clone(); });

    combo->setCurrentIndex(combo->findText(QStringLiteral("Diamond")));

    QVERIFY(received != nullptr);
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(*received).tipShape(), BrushTipShape::Diamond);
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(panel.toolConfiguration()).tipShape(),
             BrushTipShape::Diamond);
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

void ToolConfigurationPanelTest::editingTheGradientEditorUpdatesDefaultGradientAndEmits() {
    ToolConfigurationPanel panel;
    auto* opacitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"));
    QVERIFY(opacitySpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    opacitySpinBox->setValue(0.5);

    QCOMPARE(spy.count(), 1);
    // gradientEditor_ starts with stop 0 selected - see
    // GradientBarWidget::setGradient()'s own "always resets to stop 0"
    // contract.
    QCOMPARE(panel.toolConfiguration().defaultGradient().stops().front().leftOpacity, 0.5f);
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

void ToolConfigurationPanelTest::freshPanelHasStampModeStrokeAndTheIntervalSpinBoxDisabled() {
    const ToolConfigurationPanel panel;
    QCOMPARE(panel.toolConfiguration().stampMode(), StampMode::Stroke);
    auto* intervalSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"));
    QVERIFY(intervalSpinBox != nullptr);
    // Meaningless while Stroke - see ToolConfiguration::stampInterval()'s
    // own docs - so disabled rather than editable-but-ignored.
    QVERIFY(!intervalSpinBox->isEnabled());
}

void ToolConfigurationPanelTest::changingTheStampModeEmitsToolConfigurationChangedAndEnablesTheIntervalSpinBox() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    QVERIFY(combo != nullptr);
    auto* intervalSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"));
    QVERIFY(intervalSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    combo->setCurrentIndex(combo->findText(QStringLiteral("Along Curve")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().stampMode(), StampMode::AlongCurve);
    QVERIFY(intervalSpinBox->isEnabled());
}

void ToolConfigurationPanelTest::changingTheStampIntervalEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* intervalSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"));
    QVERIFY(intervalSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    intervalSpinBox->setValue(0.25);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().stampInterval(), 0.25);
}

void ToolConfigurationPanelTest::loadingAConfigurationSyncsTheStampModeAndIntervalControls() {
    ToolConfigurationPanel panel;
    ProceduralConfiguration config;
    config.setStampMode(StampMode::FrequencyAxis);
    config.setStampInterval(150.0);

    panel.setToolConfiguration(config);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    auto* intervalSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"));
    QCOMPARE(combo->currentText(), QStringLiteral("Frequency Axis"));
    QCOMPARE(intervalSpinBox->value(), 150.0);
    QVERIFY(intervalSpinBox->isEnabled());
}

void ToolConfigurationPanelTest::stampPatternFieldIsHiddenUnlessStampModeIsAlongCurve() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    auto* patternLineEdit = panel.findChild<QLineEdit*>(QStringLiteral("stampIntervalPatternLineEdit"));
    QVERIFY(combo != nullptr);
    QVERIFY(patternLineEdit != nullptr);

    // Stroke (the fresh default) - meaningless, hidden.
    QVERIFY(patternLineEdit->isHidden());

    combo->setCurrentIndex(combo->findText(QStringLiteral("Along Curve")));
    QVERIFY(!patternLineEdit->isHidden());

    // TimeAxis/FrequencyAxis - a cyclic pattern has no well-defined meaning
    // for axis-crossing placement (see stampIntervalPatternText()'s own
    // docs), so this stays hidden for both, same as Stroke.
    combo->setCurrentIndex(combo->findText(QStringLiteral("Time Axis")));
    QVERIFY(patternLineEdit->isHidden());
    combo->setCurrentIndex(combo->findText(QStringLiteral("Frequency Axis")));
    QVERIFY(patternLineEdit->isHidden());
}

void ToolConfigurationPanelTest::enteringAValidStampPatternEmitsToolConfigurationChangedAndClearsTheErrorLabel() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    combo->setCurrentIndex(combo->findText(QStringLiteral("Along Curve")));
    auto* patternLineEdit = panel.findChild<QLineEdit*>(QStringLiteral("stampIntervalPatternLineEdit"));
    auto* errorLabel = panel.findChild<QLabel*>(QStringLiteral("stampIntervalPatternErrorLabel"));
    QVERIFY(patternLineEdit != nullptr);
    QVERIFY(errorLabel != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    patternLineEdit->setText(QStringLiteral("100ms 200ms"));

    QVERIFY(spy.count() >= 1);
    QCOMPARE(panel.toolConfiguration().stampIntervalPatternText(), std::string("100ms 200ms"));
    QVERIFY(errorLabel->text().isEmpty());
}

void ToolConfigurationPanelTest::enteringAnInvalidStampPatternShowsAnErrorButStillStoresTheRawText() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    combo->setCurrentIndex(combo->findText(QStringLiteral("Along Curve")));
    auto* patternLineEdit = panel.findChild<QLineEdit*>(QStringLiteral("stampIntervalPatternLineEdit"));
    auto* errorLabel = panel.findChild<QLabel*>(QStringLiteral("stampIntervalPatternErrorLabel"));

    patternLineEdit->setText(QStringLiteral("100 not valid"));

    // Stored regardless of validity - painting itself falls back to the
    // fixed stampInterval() for an unparseable pattern (see
    // stampIntervalPatternText()'s own docs), so an in-progress edit is
    // never silently reverted here.
    QCOMPARE(panel.toolConfiguration().stampIntervalPatternText(), std::string("100 not valid"));
    QVERIFY(!errorLabel->text().isEmpty());
    QVERIFY(!errorLabel->isHidden());
}

void ToolConfigurationPanelTest::loadingAConfigurationSyncsTheStampPatternFieldAndClearsAnyStaleError() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    combo->setCurrentIndex(combo->findText(QStringLiteral("Along Curve")));
    auto* patternLineEdit = panel.findChild<QLineEdit*>(QStringLiteral("stampIntervalPatternLineEdit"));
    auto* errorLabel = panel.findChild<QLabel*>(QStringLiteral("stampIntervalPatternErrorLabel"));
    patternLineEdit->setText(QStringLiteral("invalid"));
    QVERIFY(!errorLabel->text().isEmpty());  // A stale error from the panel's own prior state.

    ProceduralConfiguration config;
    config.setStampMode(StampMode::AlongCurve);
    config.setStampIntervalPatternText("1b 2b");
    panel.setToolConfiguration(config);

    QCOMPARE(patternLineEdit->text(), QStringLiteral("1b 2b"));
    QVERIFY(errorLabel->text().isEmpty());
}

// --- Sound Mind Instruments (v0.Y.32.1) -------------------------------------

void ToolConfigurationPanelTest::freshPanelDefaultsToProceduralWithTheProceduralGroupVisible() {
    const ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    QVERIFY(toolTypeCombo != nullptr);
    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Procedural"));

    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));
    QVERIFY(proceduralGroup != nullptr);
    QVERIFY(instrumentGroup != nullptr);
    QVERIFY(!proceduralGroup->isHidden());
    QVERIFY(instrumentGroup->isHidden());
}

void ToolConfigurationPanelTest::switchingToolTypeToInstrumentShowsItsOwnGroupAndHidesProcedural() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().type(), ToolType::Instrument);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(!instrumentGroup->isHidden());
}

void ToolConfigurationPanelTest::switchingToolTypeToInstrumentPreservesSharedFields() {
    ToolConfigurationPanel panel;
    auto* falloffSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"));
    auto* sizeSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    falloffSpinBox->setValue(0.6);
    sizeSpinBox->setValue(1.5);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));

    QCOMPARE(panel.toolConfiguration().falloff(), 0.6f);
    QCOMPARE(panel.toolConfiguration().size(), 1.5);
}

void ToolConfigurationPanelTest::switchingToolTypeBackToProceduralRestoresTheProceduralGroup() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Procedural")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::Procedural);
    QVERIFY(!proceduralGroup->isHidden());
    QVERIFY(instrumentGroup->isHidden());
}

void ToolConfigurationPanelTest::changingHarmonicCountResizesTheStrengthRows() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* harmonicCountSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("harmonicCountSpinBox"));

    const int initialCount =
        static_cast<int>(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration())
                              .harmonicStrengths()
                              .size());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox%1").arg(initialCount)) !=
            nullptr);

    harmonicCountSpinBox->setValue(initialCount + 2);

    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).harmonicStrengths().size(),
              std::size_t{static_cast<std::size_t>(initialCount) + 2});
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox%1").arg(initialCount + 2)) !=
            nullptr);

    harmonicCountSpinBox->setValue(1);

    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).harmonicStrengths().size(),
              std::size_t{1});
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox2")) == nullptr);
}

void ToolConfigurationPanelTest::changingAHarmonicStrengthEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* firstHarmonicSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox1"));
    QVERIFY(firstHarmonicSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    firstHarmonicSpinBox->setValue(0.42);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).harmonicStrengths().front(),
              0.42);
}

void ToolConfigurationPanelTest::changingInharmonicityEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* inharmonicitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("inharmonicitySpinBox"));
    QVERIFY(inharmonicitySpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    inharmonicitySpinBox->setValue(0.02);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).inharmonicity(), 0.02);
}

void ToolConfigurationPanelTest::loadingAnInstrumentConfigurationSyncsToolTypeAndHarmonicControls() {
    ToolConfigurationPanel panel;
    InstrumentConfiguration config;
    config.setHarmonicStrengths({1.0, 0.7, 0.4});
    config.setInharmonicity(0.03);

    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* harmonicCountSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("harmonicCountSpinBox"));
    auto* inharmonicitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("inharmonicitySpinBox"));
    auto* secondHarmonicSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox2"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));

    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Instrument"));
    QCOMPARE(harmonicCountSpinBox->value(), 3);
    QCOMPARE(inharmonicitySpinBox->value(), 0.03);
    QVERIFY(secondHarmonicSpinBox != nullptr);
    QCOMPARE(secondHarmonicSpinBox->value(), 0.7);
    QVERIFY(!instrumentGroup->isHidden());
}

void ToolConfigurationPanelTest::setAvailableMindWavesPopulatesBothVibratoAndTremoloCombos() {
    ToolConfigurationPanel panel;

    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")},
                                  {MindWaveId{6}, QStringLiteral("Fast Pulse")}});

    for (const auto& comboName : {QStringLiteral("vibratoMindWaveCombo"), QStringLiteral("tremoloMindWaveCombo")}) {
        auto* combo = panel.findChild<QComboBox*>(comboName);
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 4);  // None + two MindWaves + "Create New MindWave...".
        QCOMPARE(combo->itemText(1), QStringLiteral("Slow Pulse"));
        QCOMPARE(combo->itemText(2), QStringLiteral("Fast Pulse"));
        QCOMPARE(combo->itemText(3), QStringLiteral("Create New MindWave..."));
    }
}

void ToolConfigurationPanelTest::selectingCreateNewMindWaveCallsTheCallbackAndRewritesTheItemInPlace() {
    // v0.Y.62.1 Installment G.
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    int callCount = 0;
    panel.setCreateMindWaveCallback([&callCount]() -> std::pair<MindWaveId, QString> {
        ++callCount;
        return {MindWaveId{42}, QStringLiteral("Fresh Wave")};
    });
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("vibratoMindWaveCombo"));
    QCOMPARE(combo->itemText(combo->count() - 1), QStringLiteral("Create New MindWave..."));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    combo->setCurrentIndex(combo->count() - 1);  // "Create New MindWave...".

    QCOMPARE(callCount, 1);
    QCOMPARE(combo->currentText(), QStringLiteral("Fresh Wave"));
    QCOMPARE(combo->itemData(combo->currentIndex()).toULongLong(), static_cast<qulonglong>(42));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).vibratoMindWave(),
              std::optional<MindWaveId>(MindWaveId{42}));
}

void ToolConfigurationPanelTest::selectingCreateNewMindWaveWithNoCallbackSetIsASilentNoOp() {
    // No setCreateMindWaveCallback() call at all - must not crash, and
    // must leave the sentinel item exactly as it was (the rewrite this
    // callback would otherwise do simply never happens).
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("vibratoMindWaveCombo"));

    combo->setCurrentIndex(combo->count() - 1);  // "Create New MindWave...".

    QCOMPARE(combo->currentText(), QStringLiteral("Create New MindWave..."));
}

void ToolConfigurationPanelTest::changingVibratoDepthEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* vibratoDepthSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("vibratoDepthSpinBox"));
    QVERIFY(vibratoDepthSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    vibratoDepthSpinBox->setValue(2.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).vibratoDepthSemitones(), 2.5);
}

void ToolConfigurationPanelTest::changingTheVibratoComboEmitsToolConfigurationChangedWithTheNewBinding() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    panel.findChild<QComboBox*>(QStringLiteral("vibratoMindWaveCombo"))->setCurrentIndex(1);  // "Slow Pulse".

    QCOMPARE(spy.count(), 1);
    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).vibratoMindWave(),
              std::optional<MindWaveId>(MindWaveId{5}));
    // Tremolo is untouched.
    QVERIFY(!dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).tremoloMindWave().has_value());
}

void ToolConfigurationPanelTest::selectingNoneOnTheTremoloComboUnbindsAndEmits() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("tremoloMindWaveCombo"));
    combo->setCurrentIndex(1);  // Bind first.
    QVERIFY(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).tremoloMindWave().has_value());
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    combo->setCurrentIndex(0);  // "None".

    QCOMPARE(spy.count(), 1);
    QVERIFY(!dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).tremoloMindWave().has_value());
}

void ToolConfigurationPanelTest::loadingAnInstrumentConfigurationSyncsVibratoAndTremoloControls() {
    ToolConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    InstrumentConfiguration config;
    config.setVibratoMindWave(MindWaveId{5});
    config.setVibratoDepthSemitones(1.5);
    config.setTremoloDepth(0.4);

    panel.setToolConfiguration(config);

    auto* vibratoCombo = panel.findChild<QComboBox*>(QStringLiteral("vibratoMindWaveCombo"));
    auto* vibratoDepthSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("vibratoDepthSpinBox"));
    auto* tremoloCombo = panel.findChild<QComboBox*>(QStringLiteral("tremoloMindWaveCombo"));
    auto* tremoloDepthSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("tremoloDepthSpinBox"));

    QCOMPARE(vibratoCombo->currentText(), QStringLiteral("Slow Pulse"));
    QCOMPARE(vibratoDepthSpinBox->value(), 1.5);
    QCOMPARE(tremoloCombo->currentText(), QStringLiteral("None"));
    QCOMPARE(tremoloDepthSpinBox->value(), 0.4);
}

void ToolConfigurationPanelTest::switchingAwayFromAndBackToInstrumentPreservesVibratoAndTremoloBindings() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    panel.findChild<QComboBox*>(QStringLiteral("vibratoMindWaveCombo"))->setCurrentIndex(1);
    panel.findChild<QDoubleSpinBox*>(QStringLiteral("vibratoDepthSpinBox"))->setValue(3.0);

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Procedural")));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));

    const auto& instrument = dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration());
    QCOMPARE(instrument.vibratoMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
    QCOMPARE(instrument.vibratoDepthSemitones(), 3.0);
}

// --- Paint Tool Enhancements: canvas-space Opacity/Size/Color bindings (v0.Y.54.1 Installment B) ----------------

void ToolConfigurationPanelTest::setAvailableMindWavesPopulatesTheOpacitySizeAndColorCombosToo() {
    ToolConfigurationPanel panel;

    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")},
                                  {MindWaveId{6}, QStringLiteral("Fast Pulse")}});

    for (const auto& comboName :
         {QStringLiteral("opacityMindWaveCombo"), QStringLiteral("sizeMindWaveCombo"), QStringLiteral("colorMindWaveCombo")}) {
        auto* combo = panel.findChild<QComboBox*>(comboName);
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 4);  // None + two MindWaves + "Create New MindWave...".
        QCOMPARE(combo->itemText(1), QStringLiteral("Slow Pulse"));
        QCOMPARE(combo->itemText(2), QStringLiteral("Fast Pulse"));
        QCOMPARE(combo->itemText(3), QStringLiteral("Create New MindWave..."));
    }
}

void ToolConfigurationPanelTest::changingTheOpacityMindWaveComboEmitsToolConfigurationChangedWithTheNewBinding() {
    ToolConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    panel.findChild<QComboBox*>(QStringLiteral("opacityMindWaveCombo"))->setCurrentIndex(1);  // "Slow Pulse".

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().opacityMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
    // Size/Color are untouched.
    QVERIFY(!panel.toolConfiguration().sizeMindWave().has_value());
    QVERIFY(!panel.toolConfiguration().colorMindWave().has_value());
}

void ToolConfigurationPanelTest::selectingNoneOnTheSizeMindWaveComboUnbindsAndEmits() {
    ToolConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("sizeMindWaveCombo"));
    combo->setCurrentIndex(1);  // Bind first.
    QVERIFY(panel.toolConfiguration().sizeMindWave().has_value());
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    combo->setCurrentIndex(0);  // "None".

    QCOMPARE(spy.count(), 1);
    QVERIFY(!panel.toolConfiguration().sizeMindWave().has_value());
}

void ToolConfigurationPanelTest::loadingAConfigurationSyncsTheOpacitySizeAndColorCombos() {
    ToolConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    ProceduralConfiguration config;
    config.setOpacityMindWave(MindWaveId{5});

    panel.setToolConfiguration(config);

    auto* opacityCombo = panel.findChild<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    auto* sizeCombo = panel.findChild<QComboBox*>(QStringLiteral("sizeMindWaveCombo"));
    QCOMPARE(opacityCombo->currentText(), QStringLiteral("Slow Pulse"));
    QCOMPARE(sizeCombo->currentText(), QStringLiteral("None"));
}

void ToolConfigurationPanelTest::switchingToolTypeAwayFromAndBackPreservesOpacitySizeAndColorBindings() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    panel.findChild<QComboBox*>(QStringLiteral("colorMindWaveCombo"))->setCurrentIndex(1);

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Procedural")));

    QCOMPARE(panel.toolConfiguration().colorMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
    QCOMPARE(panel.findChild<QComboBox*>(QStringLiteral("colorMindWaveCombo"))->currentText(),
              QStringLiteral("Slow Pulse"));
}

void ToolConfigurationPanelTest::opacitySizeColorCombosAreHiddenForMindShotAndMindGrain() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* opacityCombo = panel.findChild<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    auto* sizeCombo = panel.findChild<QComboBox*>(QStringLiteral("sizeMindWaveCombo"));
    auto* colorCombo = panel.findChild<QComboBox*>(QStringLiteral("colorMindWaveCombo"));
    QVERIFY(!opacityCombo->isHidden());  // Visible for the fresh Procedural default.

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));
    QVERIFY(opacityCombo->isHidden());
    QVERIFY(sizeCombo->isHidden());
    QVERIFY(colorCombo->isHidden());

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Grain")));
    QVERIFY(opacityCombo->isHidden());
    QVERIFY(sizeCombo->isHidden());
    QVERIFY(colorCombo->isHidden());

    // Still visible for Heal (a FixedStampPlacementConfiguration subtype -
    // meaningful there too, unlike Mind Shot/Mind Grain).
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Heal")));
    QVERIFY(!opacityCombo->isHidden());
    QVERIFY(!sizeCombo->isHidden());
    QVERIFY(!colorCombo->isHidden());
}

// --- Paint Tool Enhancements: operation-relative binding frame (v0.Y.54.1 Installment C) ------------------------

void ToolConfigurationPanelTest::freshPanelsBindingFrameIsCanvasSpace() {
    const ToolConfigurationPanel panel;
    QCOMPARE(panel.toolConfiguration().mindWaveBindingFrame(), MindWaveBindingFrame::CanvasSpace);
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("mindWaveBindingFrameCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->currentText(), QStringLiteral("Canvas Space"));
}

void ToolConfigurationPanelTest::changingTheBindingFrameComboEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("mindWaveBindingFrameCombo"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    combo->setCurrentIndex(combo->findText(QStringLiteral("Operation-Relative")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.toolConfiguration().mindWaveBindingFrame(), MindWaveBindingFrame::OperationRelative);
}

void ToolConfigurationPanelTest::loadingAConfigurationSyncsTheBindingFrameCombo() {
    ToolConfigurationPanel panel;
    ProceduralConfiguration config;
    config.setMindWaveBindingFrame(MindWaveBindingFrame::OperationRelative);

    panel.setToolConfiguration(config);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("mindWaveBindingFrameCombo"));
    QCOMPARE(combo->currentText(), QStringLiteral("Operation-Relative"));
}

void ToolConfigurationPanelTest::switchingToolTypeAwayFromAndBackPreservesTheBindingFrame() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* frameCombo = panel.findChild<QComboBox*>(QStringLiteral("mindWaveBindingFrameCombo"));
    frameCombo->setCurrentIndex(frameCombo->findText(QStringLiteral("Operation-Relative")));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Procedural")));

    QCOMPARE(panel.toolConfiguration().mindWaveBindingFrame(), MindWaveBindingFrame::OperationRelative);
    QCOMPARE(frameCombo->currentText(), QStringLiteral("Operation-Relative"));
}

// --- Mind Shots (v0.Y.33.1 Installment A) -----------------------------------

// --- Resonant Instruments (v0.Y.59.1 Installment D) -------------------------

void ToolConfigurationPanelTest::switchingToolTypeToResonanceShowsItsOwnGroupAndHidesProcedural() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* resonanceGroup = panel.findChild<QWidget*>(QStringLiteral("resonanceGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::Resonance);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(!resonanceGroup->isHidden());
}

void ToolConfigurationPanelTest::setProjectPopulatesTheResonantProfileCombo() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addResonanceProfile("Wire Loop", {0.1f, 0.5f});
    project.addResonanceProfile("Zigzag", {0.2f, 0.8f});

    panel.setProject(&project);

    auto* resonantProfileCombo = panel.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QVERIFY(resonantProfileCombo != nullptr);
    QCOMPARE(resonantProfileCombo->count(), 2);
    QCOMPARE(resonantProfileCombo->itemText(0), QStringLiteral("Wire Loop"));
    QCOMPARE(resonantProfileCombo->itemText(1), QStringLiteral("Zigzag"));
}

void ToolConfigurationPanelTest::refreshResonanceProfilesAddsNewEntriesAndPreservesTheCurrentSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addResonanceProfile("Wire Loop", {0.1f, 0.5f});
    panel.setProject(&project);
    auto* resonantProfileCombo = panel.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QCOMPARE(resonantProfileCombo->currentText(), QStringLiteral("Wire Loop"));

    project.addResonanceProfile("Zigzag", {0.2f, 0.8f});
    panel.refreshResonanceProfiles();

    QCOMPARE(resonantProfileCombo->count(), 2);
    // The previously-selected entry stays selected across the refresh.
    QCOMPARE(resonantProfileCombo->currentText(), QStringLiteral("Wire Loop"));
}

void ToolConfigurationPanelTest::refreshResonanceProfilesShowsThePlaceholderWhenTheLibraryIsEmpty() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});

    panel.setProject(&project);

    auto* resonantProfileCombo = panel.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QCOMPARE(resonantProfileCombo->count(), 1);
    QCOMPARE(resonantProfileCombo->itemData(0).isValid(), false);
}

void ToolConfigurationPanelTest::selectingAResonantProfileEmitsToolConfigurationChangedWithItsSpectrum() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addResonanceProfile("Wire Loop", {0.1f, 0.5f});
    const ResonanceProfileId secondId = project.addResonanceProfile("Zigzag", {0.2f, 0.8f});
    panel.setProject(&project);
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));
    auto* resonantProfileCombo = panel.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    resonantProfileCombo->setCurrentIndex(resonantProfileCombo->findText(QStringLiteral("Zigzag")));

    QCOMPARE(spy.count(), 1);
    const auto& resonant = dynamic_cast<const ResonanceConfiguration&>(panel.toolConfiguration());
    QCOMPARE(resonant.sourceResonanceProfileId(), std::optional<ResonanceProfileId>(secondId));
    QCOMPARE(resonant.spectrum(), std::vector<float>({0.2f, 0.8f}));
}

void ToolConfigurationPanelTest::changingDecayRateEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));
    auto* decayRateSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("decayRateSpinBox"));
    QVERIFY(decayRateSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    decayRateSpinBox->setValue(3.5);

    QCOMPARE(spy.count(), 1);
    const auto& resonant = dynamic_cast<const ResonanceConfiguration&>(panel.toolConfiguration());
    QCOMPARE(resonant.decayRate(), 3.5);
}

void ToolConfigurationPanelTest::loadingAResonanceConfigurationSyncsToolTypeAndThePickerSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addResonanceProfile("Wire Loop", {0.1f, 0.5f});
    const ResonanceProfileId secondId = project.addResonanceProfile("Zigzag", {0.2f, 0.8f});
    panel.setProject(&project);

    ResonanceConfiguration config;
    config.setSpectrum(secondId, {0.2f, 0.8f});
    config.setDecayRate(2.0);
    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* resonantProfileCombo = panel.findChild<QComboBox*>(QStringLiteral("resonanceProfileCombo"));
    auto* decayRateSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("decayRateSpinBox"));
    auto* resonanceGroup = panel.findChild<QWidget*>(QStringLiteral("resonanceGroup"));

    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Resonance"));
    QCOMPARE(resonantProfileCombo->currentText(), QStringLiteral("Zigzag"));
    QCOMPARE(decayRateSpinBox->value(), 2.0);
    QVERIFY(!resonanceGroup->isHidden());
}

void ToolConfigurationPanelTest::switchingToolTypeToResonanceFixesFalloffAtZeroRegardlessOfThePriorTool() {
    ToolConfigurationPanel panel;  // Fresh - already Procedural.
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* falloffSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"));
    // Procedural's own default falloff is 0.5 - give it a distinctly
    // different, non-zero value so a carried-over value would be obvious.
    falloffSpinBox->setValue(0.9);

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));

    // changeToolType()'s own generic "every shared field carries over"
    // rule has one deliberate exception for Resonance - see its own
    // comment on why unconditionally copying the outgoing falloff() would
    // otherwise silently undo ResonanceConfiguration's own fixed-at-0
    // constructor default.
    const auto& resonant = dynamic_cast<const ResonanceConfiguration&>(panel.toolConfiguration());
    QCOMPARE(resonant.falloff(), 0.0f);
}

void ToolConfigurationPanelTest::changingFrequencyScaleEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));
    auto* frequencyScaleSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("frequencyScaleSpinBox"));
    QVERIFY(frequencyScaleSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    frequencyScaleSpinBox->setValue(3.5);

    QCOMPARE(spy.count(), 1);
    const auto& resonant = dynamic_cast<const ResonanceConfiguration&>(panel.toolConfiguration());
    QCOMPARE(resonant.frequencyScale(), 3.5);
}

void ToolConfigurationPanelTest::changingTimeSpanEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));
    auto* timeSpanSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("timeSpanSpinBox"));
    QVERIFY(timeSpanSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    timeSpanSpinBox->setValue(0.5);

    QCOMPARE(spy.count(), 1);
    const auto& resonant = dynamic_cast<const ResonanceConfiguration&>(panel.toolConfiguration());
    QCOMPARE(resonant.timeSpan(), 0.5);
}

void ToolConfigurationPanelTest::loadingAResonanceConfigurationSyncsFrequencyScaleAndTimeSpan() {
    ToolConfigurationPanel panel;
    ResonanceConfiguration config;
    config.setFrequencyScale(4.0);
    config.setTimeSpan(0.3);

    panel.setToolConfiguration(config);

    auto* frequencyScaleSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("frequencyScaleSpinBox"));
    auto* timeSpanSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("timeSpanSpinBox"));
    QCOMPARE(frequencyScaleSpinBox->value(), 4.0);
    QCOMPARE(timeSpanSpinBox->value(), 0.3);
}

void ToolConfigurationPanelTest::switchingToolTypeToMindShotShowsItsOwnGroupAndHidesProcedural() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* mindShotGroup = panel.findChild<QWidget*>(QStringLiteral("mindShotGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::MindShot);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(!mindShotGroup->isHidden());
}

void ToolConfigurationPanelTest::setProjectPopulatesTheMindShotCombo() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindShot("Piano Hit", makeTestClip());
    project.addMindShot("Vocal Chop", makeTestClip());

    panel.setProject(&project);

    auto* mindShotCombo = panel.findChild<QComboBox*>(QStringLiteral("mindShotCombo"));
    QVERIFY(mindShotCombo != nullptr);
    QCOMPARE(mindShotCombo->count(), 2);
    QCOMPARE(mindShotCombo->itemText(0), QStringLiteral("Piano Hit"));
    QCOMPARE(mindShotCombo->itemText(1), QStringLiteral("Vocal Chop"));
}

void ToolConfigurationPanelTest::refreshMindShotsAddsNewEntriesAndPreservesTheCurrentSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindShot("Piano Hit", makeTestClip());
    panel.setProject(&project);
    auto* mindShotCombo = panel.findChild<QComboBox*>(QStringLiteral("mindShotCombo"));
    QCOMPARE(mindShotCombo->currentText(), QStringLiteral("Piano Hit"));

    project.addMindShot("Vocal Chop", makeTestClip());
    panel.refreshMindShots();

    QCOMPARE(mindShotCombo->count(), 2);
    // The previously-selected entry stays selected across the refresh.
    QCOMPARE(mindShotCombo->currentText(), QStringLiteral("Piano Hit"));
}

void ToolConfigurationPanelTest::refreshMindShotsShowsThePlaceholderWhenTheLibraryIsEmpty() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});

    panel.setProject(&project);

    auto* mindShotCombo = panel.findChild<QComboBox*>(QStringLiteral("mindShotCombo"));
    QCOMPARE(mindShotCombo->count(), 1);
    QCOMPARE(mindShotCombo->itemData(0).isValid(), false);
}

void ToolConfigurationPanelTest::selectingAMindShotEmitsToolConfigurationChangedWithItsClip() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindShot("Piano Hit", makeTestClip());
    const MindShotId secondId = project.addMindShot("Vocal Chop", makeTestClip());
    panel.setProject(&project);
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));
    auto* mindShotCombo = panel.findChild<QComboBox*>(QStringLiteral("mindShotCombo"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    mindShotCombo->setCurrentIndex(mindShotCombo->findText(QStringLiteral("Vocal Chop")));

    QCOMPARE(spy.count(), 1);
    const auto& mindShot = dynamic_cast<const MindShotConfiguration&>(panel.toolConfiguration());
    QCOMPARE(mindShot.sourceMindShotId(), std::optional<MindShotId>(secondId));
    QCOMPARE(mindShot.clip().frameCount, static_cast<std::uint32_t>(2));
}

void ToolConfigurationPanelTest::selectingAMindShotCopiesItsOwnFundamentalFrequencyAndStartTimeOffset() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    const MindShotId id = project.addMindShot("Piano Hit", makeTestClip());
    project.mindShotById(id)->fundamentalFrequencyHz = 261.63;
    project.mindShotById(id)->startTimeOffsetSeconds = 0.05;
    panel.setProject(&project);
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));
    auto* mindShotCombo = panel.findChild<QComboBox*>(QStringLiteral("mindShotCombo"));

    mindShotCombo->setCurrentIndex(mindShotCombo->findText(QStringLiteral("Piano Hit")));

    const auto& mindShot = dynamic_cast<const MindShotConfiguration&>(panel.toolConfiguration());
    QCOMPARE(mindShot.fundamentalFrequencyHz(), 261.63);
    QCOMPARE(mindShot.startTimeOffsetSeconds(), 0.05);
}

void ToolConfigurationPanelTest::loadingAMindShotConfigurationSyncsToolTypeAndThePickerSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindShot("Piano Hit", makeTestClip());
    const MindShotId secondId = project.addMindShot("Vocal Chop", makeTestClip());
    panel.setProject(&project);

    MindShotConfiguration config;
    config.setClip(secondId, makeTestClip());
    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* mindShotCombo = panel.findChild<QComboBox*>(QStringLiteral("mindShotCombo"));
    auto* mindShotGroup = panel.findChild<QWidget*>(QStringLiteral("mindShotGroup"));

    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Mind Shot"));
    QCOMPARE(mindShotCombo->currentText(), QStringLiteral("Vocal Chop"));
    QVERIFY(!mindShotGroup->isHidden());
}

// --- Mind Grains (v0.Y.33.1 Installment B) ----------------------------------

void ToolConfigurationPanelTest::switchingToolTypeToMindGrainShowsItsOwnGroupAndHidesProcedural() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Grain")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::MindGrain);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(!mindGrainGroup->isHidden());
}

void ToolConfigurationPanelTest::setProjectPopulatesTheMindGrainCombo() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindGrain("Rain Texture", LayerId{1}, TimeFrequencyRect{});
    project.addMindGrain("Wind Noise", LayerId{1}, TimeFrequencyRect{});

    panel.setProject(&project);

    auto* mindGrainCombo = panel.findChild<QComboBox*>(QStringLiteral("mindGrainCombo"));
    QVERIFY(mindGrainCombo != nullptr);
    QCOMPARE(mindGrainCombo->count(), 2);
    QCOMPARE(mindGrainCombo->itemText(0), QStringLiteral("Rain Texture"));
    QCOMPARE(mindGrainCombo->itemText(1), QStringLiteral("Wind Noise"));
}

void ToolConfigurationPanelTest::refreshMindGrainsAddsNewEntriesAndPreservesTheCurrentSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindGrain("Rain Texture", LayerId{1}, TimeFrequencyRect{});
    panel.setProject(&project);
    auto* mindGrainCombo = panel.findChild<QComboBox*>(QStringLiteral("mindGrainCombo"));
    QCOMPARE(mindGrainCombo->currentText(), QStringLiteral("Rain Texture"));

    project.addMindGrain("Wind Noise", LayerId{1}, TimeFrequencyRect{});
    panel.refreshMindGrains();

    QCOMPARE(mindGrainCombo->count(), 2);
    // The previously-selected entry stays selected across the refresh.
    QCOMPARE(mindGrainCombo->currentText(), QStringLiteral("Rain Texture"));
}

void ToolConfigurationPanelTest::refreshMindGrainsShowsThePlaceholderWhenTheLibraryIsEmpty() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});

    panel.setProject(&project);

    auto* mindGrainCombo = panel.findChild<QComboBox*>(QStringLiteral("mindGrainCombo"));
    QCOMPARE(mindGrainCombo->count(), 1);
    QCOMPARE(mindGrainCombo->itemData(0).isValid(), false);
}

void ToolConfigurationPanelTest::selectingAMindGrainEmitsToolConfigurationChangedWithItsReference() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindGrain("Rain Texture", LayerId{1}, TimeFrequencyRect{});
    const MindGrainId secondId = project.addMindGrain("Wind Noise", LayerId{1}, TimeFrequencyRect{0.5, 1.5, 200.0, 800.0});
    panel.setProject(&project);
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Grain")));
    auto* mindGrainCombo = panel.findChild<QComboBox*>(QStringLiteral("mindGrainCombo"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    mindGrainCombo->setCurrentIndex(mindGrainCombo->findText(QStringLiteral("Wind Noise")));

    QCOMPARE(spy.count(), 1);
    const auto& mindGrain = dynamic_cast<const MindGrainConfiguration&>(panel.toolConfiguration());
    QCOMPARE(mindGrain.sourceMindGrainId(), std::optional<MindGrainId>(secondId));
    QCOMPARE(mindGrain.sourceLayerId(), LayerId{1});
    QCOMPARE(mindGrain.bounds().startTimeSeconds, 0.5);
}

void ToolConfigurationPanelTest::selectingAMindGrainCopiesItsOwnFundamentalFrequencyAndStartTimeOffset() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    const MindGrainId id = project.addMindGrain("Rain Texture", LayerId{1}, TimeFrequencyRect{});
    project.mindGrainById(id)->fundamentalFrequencyHz = 220.0;
    project.mindGrainById(id)->startTimeOffsetSeconds = 0.02;
    panel.setProject(&project);
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Grain")));
    auto* mindGrainCombo = panel.findChild<QComboBox*>(QStringLiteral("mindGrainCombo"));

    mindGrainCombo->setCurrentIndex(mindGrainCombo->findText(QStringLiteral("Rain Texture")));

    const auto& mindGrain = dynamic_cast<const MindGrainConfiguration&>(panel.toolConfiguration());
    QCOMPARE(mindGrain.fundamentalFrequencyHz(), 220.0);
    QCOMPARE(mindGrain.startTimeOffsetSeconds(), 0.02);
}

void ToolConfigurationPanelTest::loadingAMindGrainConfigurationSyncsToolTypeAndThePickerSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    project.addMindGrain("Rain Texture", LayerId{1}, TimeFrequencyRect{});
    const MindGrainId secondId = project.addMindGrain("Wind Noise", LayerId{1}, TimeFrequencyRect{});
    panel.setProject(&project);

    MindGrainConfiguration config;
    config.setReference(secondId, LayerId{1}, TimeFrequencyRect{});
    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* mindGrainCombo = panel.findChild<QComboBox*>(QStringLiteral("mindGrainCombo"));
    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));

    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Mind Grain"));
    QCOMPARE(mindGrainCombo->currentText(), QStringLiteral("Wind Noise"));
    QVERIFY(!mindGrainGroup->isHidden());
}

void ToolConfigurationPanelTest::setActiveLayerHighlightsTheGroupWhenTheActiveLayerIsNotAboveTheSource() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    const LayerId lower = project.addLayer(Layer(0, "Lower", LayerType::Normal));
    const LayerId upper = project.addLayer(Layer(0, "Upper", LayerType::Normal));
    panel.setProject(&project);

    MindGrainConfiguration config;
    config.setReference(std::nullopt, upper, TimeFrequencyRect{});  // Source is `upper`.
    panel.setToolConfiguration(config);

    // `lower` is not above `upper` (its own configured source).
    panel.setActiveLayer(lower);

    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));
    QVERIFY(!mindGrainGroup->styleSheet().isEmpty());
    QVERIFY(!mindGrainGroup->toolTip().isEmpty());
}

void ToolConfigurationPanelTest::setActiveLayerClearsTheHighlightWhenTheActiveLayerIsAboveTheSource() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    const LayerId lower = project.addLayer(Layer(0, "Lower", LayerType::Normal));
    const LayerId upper = project.addLayer(Layer(0, "Upper", LayerType::Normal));
    panel.setProject(&project);

    MindGrainConfiguration config;
    config.setReference(std::nullopt, lower, TimeFrequencyRect{});  // Source is `lower`.
    panel.setToolConfiguration(config);

    // `upper` IS above `lower` (its own configured source).
    panel.setActiveLayer(upper);

    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));
    QVERIFY(mindGrainGroup->styleSheet().isEmpty());
    QVERIFY(mindGrainGroup->toolTip().isEmpty());
}

// --- Heal/Soften (v0.Y.34.1 Installment A) ----------------------------------

void ToolConfigurationPanelTest::switchingToolTypeToHealHidesEveryOtherGroup() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));
    auto* mindShotGroup = panel.findChild<QWidget*>(QStringLiteral("mindShotGroup"));
    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Heal")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::Heal);
    // Heal adds no group of its own - every other tool type's own group
    // must be hidden, and none takes its place.
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(instrumentGroup->isHidden());
    QVERIFY(mindShotGroup->isHidden());
    QVERIFY(mindGrainGroup->isHidden());
}

void ToolConfigurationPanelTest::switchingToolTypeToSoftenHidesEveryOtherGroup() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));
    auto* mindShotGroup = panel.findChild<QWidget*>(QStringLiteral("mindShotGroup"));
    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Soften")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::Soften);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(instrumentGroup->isHidden());
    QVERIFY(mindShotGroup->isHidden());
    QVERIFY(mindGrainGroup->isHidden());
}

void ToolConfigurationPanelTest::switchingToolTypeToHealPreservesSharedFields() {
    ToolConfigurationPanel panel;
    auto* falloffSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"));
    auto* sizeSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"));
    falloffSpinBox->setValue(0.6);
    sizeSpinBox->setValue(1.5);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Heal")));

    QCOMPARE(panel.toolConfiguration().falloff(), 0.6f);
    QCOMPARE(panel.toolConfiguration().size(), 1.5);
}

void ToolConfigurationPanelTest::loadingAHealConfigurationSyncsToolType() {
    ToolConfigurationPanel panel;
    HealConfiguration config;
    config.setFalloff(0.3f);

    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Heal"));
    QCOMPARE(panel.toolConfiguration().falloff(), 0.3f);
}

void ToolConfigurationPanelTest::loadingASoftenConfigurationSyncsToolType() {
    ToolConfigurationPanel panel;
    SoftenConfiguration config;
    config.setFalloff(0.7f);

    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Soften"));
    QCOMPARE(panel.toolConfiguration().falloff(), 0.7f);
}

// --- Smudge/Order-Chaos (v0.Y.34.1 Installment B) ---------------------------

void ToolConfigurationPanelTest::switchingToolTypeToSmudgeHidesEveryOtherGroup() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* instrumentGroup = panel.findChild<QWidget*>(QStringLiteral("instrumentGroup"));
    auto* mindShotGroup = panel.findChild<QWidget*>(QStringLiteral("mindShotGroup"));
    auto* mindGrainGroup = panel.findChild<QWidget*>(QStringLiteral("mindGrainGroup"));
    auto* orderChaosGroup = panel.findChild<QWidget*>(QStringLiteral("orderChaosGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Smudge")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::Smudge);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(instrumentGroup->isHidden());
    QVERIFY(mindShotGroup->isHidden());
    QVERIFY(mindGrainGroup->isHidden());
    QVERIFY(orderChaosGroup->isHidden());
}

void ToolConfigurationPanelTest::loadingASmudgeConfigurationSyncsToolType() {
    ToolConfigurationPanel panel;
    SmudgeConfiguration config;
    config.setFalloff(0.4f);

    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Smudge"));
    QCOMPARE(panel.toolConfiguration().falloff(), 0.4f);
}

void ToolConfigurationPanelTest::switchingToolTypeToOrderChaosShowsItsOwnGroupAndHidesProcedural() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* proceduralGroup = panel.findChild<QWidget*>(QStringLiteral("proceduralGroup"));
    auto* orderChaosGroup = panel.findChild<QWidget*>(QStringLiteral("orderChaosGroup"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Order/Chaos")));

    QCOMPARE(panel.toolConfiguration().type(), ToolType::OrderChaos);
    QVERIFY(proceduralGroup->isHidden());
    QVERIFY(!orderChaosGroup->isHidden());
}

void ToolConfigurationPanelTest::changingAmountEmitsToolConfigurationChanged() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Order/Chaos")));
    auto* amountSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("amountSpinBox"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    amountSpinBox->setValue(-0.6);

    QCOMPARE(spy.count(), 1);
    const auto& orderChaos = dynamic_cast<const OrderChaosConfiguration&>(panel.toolConfiguration());
    QCOMPARE(orderChaos.amount(), -0.6);
}

void ToolConfigurationPanelTest::loadingAnOrderChaosConfigurationSyncsToolTypeAndAmount() {
    ToolConfigurationPanel panel;
    OrderChaosConfiguration config;
    config.setAmount(0.35);

    panel.setToolConfiguration(config);

    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* amountSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("amountSpinBox"));
    QCOMPARE(toolTypeCombo->currentText(), QStringLiteral("Order/Chaos"));
    QCOMPARE(amountSpinBox->value(), 0.35);
}

// --- Shared control visibility review (v0.Y.34.1 Installment C) ------------

void ToolConfigurationPanelTest::proceduralShowsEverySharedControl() {
    const ToolConfigurationPanel panel;  // Fresh - already Procedural.

    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<GradientEditorWidget*>(QStringLiteral("gradientEditor"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::mindShotHidesFalloffSizeAndGradientEditorButKeepsStampControls() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));

    // Never consulted by applyMindShotPaintOperation() - see its own docs.
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(panel.findChild<GradientEditorWidget*>(QStringLiteral("gradientEditor"))->isHidden());
    // Still a real, meaningful placement choice.
    QVERIFY(!panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::mindGrainHidesFalloffSizeAndGradientEditorButKeepsStampControls() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Grain")));

    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(panel.findChild<GradientEditorWidget*>(QStringLiteral("gradientEditor"))->isHidden());
    QVERIFY(!panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::healHidesIntensityAndStampControlsButKeepsFalloffSizeAndOpacity() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Heal")));

    // Real, load-bearing parameters for Heal.
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<GradientEditorWidget*>(QStringLiteral("gradientEditor"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"))->isHidden());
    // Never consulted by Heal's own blend - see HealConfiguration's docs.
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::softenHidesIntensityAndStampControls() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Soften")));

    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::smudgeHidesIntensityAndStampControls() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Smudge")));

    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::resonanceHidesFalloffAndSizeButKeepsStampControlsAndGradientEditor() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Resonance")));

    // Superseded by the dedicated Frequency Scale/Time Span controls in
    // resonanceGroup_ - see updateSharedControlVisibility()'s own docs.
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    // Not a FixedStampPlacementConfiguration - stamps along the stroke
    // the same adjustable way Instrument does.
    QVERIFY(!panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
    // Still reads the gradient for its own target color/intensity.
    QVERIFY(!panel.findChild<GradientEditorWidget*>(QStringLiteral("gradientEditor"))->isHidden());
    // sizeMindWaveCombo_ stays with the broader gate, not the narrower
    // Falloff/Size one - it still works for Resonance, modulating
    // timeSpan() instead of the now-hidden plain size() field.
    QVERIFY(!panel.findChild<QComboBox*>(QStringLiteral("sizeMindWaveCombo"))->isHidden());
}

void ToolConfigurationPanelTest::orderChaosHidesIntensityAndStampControls() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Order/Chaos")));

    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->isHidden());
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"))->isHidden());
    QVERIFY(panel.findChild<QDoubleSpinBox*>(QStringLiteral("stampIntervalSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("falloffSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("sizeSpinBox"))->isHidden());
    QVERIFY(!panel.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"))->isHidden());
}

void ToolConfigurationPanelTest::switchingFromHealBackToProceduralPreservesTheOriginalStampMode() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    auto* stampModeCombo = panel.findChild<QComboBox*>(QStringLiteral("stampModeCombo"));
    // A real, deliberate choice, different from Heal's own forced AlongCurve.
    stampModeCombo->setCurrentIndex(stampModeCombo->findText(QStringLiteral("Time Axis")));
    QCOMPARE(panel.toolConfiguration().stampMode(), StampMode::TimeAxis);

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Heal")));
    QCOMPARE(panel.toolConfiguration().stampMode(), StampMode::AlongCurve);  // Forced, not Time Axis.

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Procedural")));

    // The detour through Heal didn't silently overwrite the original choice.
    QCOMPARE(panel.toolConfiguration().stampMode(), StampMode::TimeAxis);
}

// --- Blend Mode (v0.Y.37.1) -------------------------------------------------

void ToolConfigurationPanelTest::proceduralHidesTheBlendModeCombo() {
    const ToolConfigurationPanel panel;  // Fresh - already Procedural.
    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo"))->isHidden());
}

void ToolConfigurationPanelTest::mindShotShowsTheBlendModeComboDefaultedToOverwrite() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));

    auto* blendModeCombo = panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo"));
    QVERIFY(!blendModeCombo->isHidden());
    QCOMPARE(blendModeCombo->currentText(), QStringLiteral("Overwrite"));
}

void ToolConfigurationPanelTest::mindGrainShowsTheBlendModeComboDefaultedToOverwrite() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Grain")));

    auto* blendModeCombo = panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo"));
    QVERIFY(!blendModeCombo->isHidden());
    QCOMPARE(blendModeCombo->currentText(), QStringLiteral("Overwrite"));
}

void ToolConfigurationPanelTest::changingTheBlendModeComboEmitsToolConfigurationChangedWithTheNewMode() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Mind Shot")));
    auto* blendModeCombo = panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    blendModeCombo->setCurrentIndex(blendModeCombo->findText(QStringLiteral("Multiply")));

    QCOMPARE(spy.count(), 1);
    const auto& mindShot = dynamic_cast<const MindShotConfiguration&>(panel.toolConfiguration());
    QCOMPARE(mindShot.blendMode(), BlendMode::Multiply);
}

void ToolConfigurationPanelTest::loadingAMindShotConfigurationSyncsTheBlendModeCombo() {
    ToolConfigurationPanel panel;
    MindShotConfiguration config;
    config.setBlendMode(BlendMode::Screen);

    panel.setToolConfiguration(config);

    auto* blendModeCombo = panel.findChild<QComboBox*>(QStringLiteral("blendModeCombo"));
    QCOMPARE(blendModeCombo->currentText(), QStringLiteral("Screen"));
}

// --- Named Tool Configuration preset library (v0.Y.55.1 Prerequisite 2) ----

void ToolConfigurationPanelTest::freshPanelsToolPresetComboIsEmptyWhenNoProjectIsSet() {
    const ToolConfigurationPanel panel;
    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QVERIFY(toolPresetCombo != nullptr);
    // Same as mindShotCombo_/mindGrainCombo_: never populated until setProject()
    // is called - see refreshToolPresets()'s own docs.
    QCOMPARE(toolPresetCombo->count(), 0);
}

void ToolConfigurationPanelTest::setProjectPopulatesTheToolPresetCombo() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration config;
    project.addToolPreset("Soft Circle", config);
    project.addToolPreset("Hard Diamond", config);

    panel.setProject(&project);

    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QCOMPARE(toolPresetCombo->count(), 2);
    QCOMPARE(toolPresetCombo->itemText(0), QStringLiteral("Soft Circle"));
    QCOMPARE(toolPresetCombo->itemText(1), QStringLiteral("Hard Diamond"));
}

void ToolConfigurationPanelTest::saveCurrentAsToolPresetNamedAddsANamedEntryAndSelectsIt() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    panel.setProject(&project);
    auto* tipShapeCombo = panel.findChild<QComboBox*>(QStringLiteral("tipShapeCombo"));
    tipShapeCombo->setCurrentIndex(tipShapeCombo->findText(QStringLiteral("Diamond")));

    const auto id = panel.saveCurrentAsToolPresetNamed(QStringLiteral("My Diamond"));

    QVERIFY(id.has_value());
    QCOMPARE(project.toolPresets().size(), static_cast<std::size_t>(1));
    const auto* named = project.toolPresetById(*id);
    QVERIFY(named != nullptr);
    QCOMPARE(named->name, std::string("My Diamond"));
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(*named->config).tipShape(), BrushTipShape::Diamond);

    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QCOMPARE(toolPresetCombo->currentText(), QStringLiteral("My Diamond"));
}

void ToolConfigurationPanelTest::saveCurrentAsToolPresetNamedReturnsNulloptWithNoProject() {
    ToolConfigurationPanel panel;  // No setProject() call.

    const auto id = panel.saveCurrentAsToolPresetNamed(QStringLiteral("Orphan"));

    QVERIFY(!id.has_value());
}

void ToolConfigurationPanelTest::saveCurrentAsToolPresetNamedReturnsNulloptForAnEmptyOrWhitespaceOnlyName() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    panel.setProject(&project);

    QVERIFY(!panel.saveCurrentAsToolPresetNamed(QStringLiteral("")).has_value());
    QVERIFY(!panel.saveCurrentAsToolPresetNamed(QStringLiteral("   ")).has_value());
    QCOMPARE(project.toolPresets().size(), static_cast<std::size_t>(0));
}

void ToolConfigurationPanelTest::selectingAToolPresetLoadsItsConfigurationAndEmits() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration circleConfig;
    project.addToolPreset("Soft Circle", circleConfig);
    ProceduralConfiguration diamondConfig;
    diamondConfig.setTipShape(BrushTipShape::Diamond);
    project.addToolPreset("My Diamond", diamondConfig);
    panel.setProject(&project);
    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    toolPresetCombo->setCurrentIndex(toolPresetCombo->findText(QStringLiteral("My Diamond")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(panel.toolConfiguration()).tipShape(),
              BrushTipShape::Diamond);
}

void ToolConfigurationPanelTest::deleteCurrentToolPresetRemovesTheSelectedEntryAndRefreshes() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration config;
    const auto id = project.addToolPreset("Soft Circle", config);
    panel.setProject(&project);
    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    toolPresetCombo->setCurrentIndex(toolPresetCombo->findText(QStringLiteral("Soft Circle")));
    auto* deletePresetButton = panel.findChild<QPushButton*>(QStringLiteral("deletePresetButton"));
    QVERIFY(deletePresetButton != nullptr);

    deletePresetButton->click();

    QVERIFY(project.toolPresetById(id) == nullptr);
    QCOMPARE(toolPresetCombo->count(), 1);
    QCOMPARE(toolPresetCombo->itemData(0).isValid(), false);
}

void ToolConfigurationPanelTest::refreshToolPresetsPreservesTheCurrentSelection() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration config;
    project.addToolPreset("Soft Circle", config);
    panel.setProject(&project);
    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QCOMPARE(toolPresetCombo->currentText(), QStringLiteral("Soft Circle"));

    project.addToolPreset("Hard Diamond", config);
    panel.refreshToolPresets();

    QCOMPARE(toolPresetCombo->count(), 2);
    QCOMPARE(toolPresetCombo->currentText(), QStringLiteral("Soft Circle"));
}

void ToolConfigurationPanelTest::switchingProjectsRefreshesTheToolPresetCombo() {
    ToolConfigurationPanel panel;
    Project firstProject = Project::createNew(ProjectSettings{});
    ProceduralConfiguration config;
    firstProject.addToolPreset("From First Project", config);
    panel.setProject(&firstProject);
    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QCOMPARE(toolPresetCombo->currentText(), QStringLiteral("From First Project"));

    Project secondProject = Project::createNew(ProjectSettings{});
    secondProject.addToolPreset("From Second Project", config);
    panel.setProject(&secondProject);

    QCOMPARE(toolPresetCombo->count(), 1);
    QCOMPARE(toolPresetCombo->currentText(), QStringLiteral("From Second Project"));
}

void ToolConfigurationPanelTest::instrumentPresetsGetAHarmonicThumbnailIconButProceduralPresetsDoNot() {
    ToolConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration proceduralConfig;
    project.addToolPreset("Soft Circle", proceduralConfig);
    InstrumentConfiguration instrumentConfig;
    instrumentConfig.setHarmonicStrengths({1.0, 0.5});
    project.addToolPreset("Warm Pad", instrumentConfig);
    ResonanceConfiguration resonantConfig;
    resonantConfig.setSpectrum(std::nullopt, {0.3f, 0.9f});
    project.addToolPreset("Wire Loop", resonantConfig);

    panel.setProject(&project);

    auto* toolPresetCombo = panel.findChild<QComboBox*>(QStringLiteral("toolPresetCombo"));
    QVERIFY(toolPresetCombo->itemIcon(0).isNull());
    QVERIFY(!toolPresetCombo->itemIcon(1).isNull());
    QVERIFY(!toolPresetCombo->itemIcon(2).isNull());
}

// --- Instrument harmonic-series visual editor (v0.Y.58.1) ------------------

void ToolConfigurationPanelTest::switchingToolTypeToInstrumentPopulatesTheHarmonicSeriesWidgetWithDefaults() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));

    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));

    auto* harmonicSeriesWidget = panel.findChild<HarmonicSeriesWidget*>(QStringLiteral("harmonicSeriesWidget"));
    QVERIFY(harmonicSeriesWidget != nullptr);
    QCOMPARE(harmonicSeriesWidget->harmonicStrengths(), InstrumentConfiguration{}.harmonicStrengths());
}

void ToolConfigurationPanelTest::draggingTheHarmonicSeriesWidgetUpdatesASpinBoxAndEmits() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* harmonicSeriesWidget = panel.findChild<HarmonicSeriesWidget*>(QStringLiteral("harmonicSeriesWidget"));
    QVERIFY(harmonicSeriesWidget != nullptr);
    harmonicSeriesWidget->resize(240, 100);
    const std::size_t count = harmonicSeriesWidget->harmonicStrengths().size();
    auto* firstHarmonicSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox1"));
    QVERIFY(firstHarmonicSpinBox != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::toolConfigurationChanged);

    // See test_harmonic_series_widget.cpp's own tests for the widget's own
    // mouse-handling details in isolation - this test only needs to
    // confirm the panel reacts correctly to a real click on it.
    const QPoint pos(harmonicXPosForColumn(0, count), harmonicYPosFor(1.5));
    QTest::mousePress(harmonicSeriesWidget, Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseRelease(harmonicSeriesWidget, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(firstHarmonicSpinBox->value(), 1.5);
    QCOMPARE(dynamic_cast<const InstrumentConfiguration&>(panel.toolConfiguration()).harmonicStrengths().front(),
              1.5);
}

void ToolConfigurationPanelTest::changingAHarmonicStrengthSpinBoxSyncsTheHarmonicSeriesWidget() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* harmonicSeriesWidget = panel.findChild<HarmonicSeriesWidget*>(QStringLiteral("harmonicSeriesWidget"));
    QVERIFY(harmonicSeriesWidget != nullptr);
    auto* firstHarmonicSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("harmonicStrengthSpinBox1"));
    QVERIFY(firstHarmonicSpinBox != nullptr);

    firstHarmonicSpinBox->setValue(0.42);

    QCOMPARE(harmonicSeriesWidget->harmonicStrengths().front(), 0.42);
}

void ToolConfigurationPanelTest::changingHarmonicCountResyncsTheHarmonicSeriesWidget() {
    ToolConfigurationPanel panel;
    auto* toolTypeCombo = panel.findChild<QComboBox*>(QStringLiteral("toolTypeCombo"));
    toolTypeCombo->setCurrentIndex(toolTypeCombo->findText(QStringLiteral("Instrument")));
    auto* harmonicSeriesWidget = panel.findChild<HarmonicSeriesWidget*>(QStringLiteral("harmonicSeriesWidget"));
    QVERIFY(harmonicSeriesWidget != nullptr);
    auto* harmonicCountSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("harmonicCountSpinBox"));
    const int initialCount = static_cast<int>(harmonicSeriesWidget->harmonicStrengths().size());

    harmonicCountSpinBox->setValue(initialCount + 2);

    QCOMPARE(harmonicSeriesWidget->harmonicStrengths().size(),
              std::size_t{static_cast<std::size_t>(initialCount) + 2});
}

void ToolConfigurationPanelTest::loadingAnInstrumentConfigurationSyncsTheHarmonicSeriesWidget() {
    ToolConfigurationPanel panel;
    InstrumentConfiguration config;
    config.setHarmonicStrengths({1.0, 0.7, 0.4});

    panel.setToolConfiguration(config);

    auto* harmonicSeriesWidget = panel.findChild<HarmonicSeriesWidget*>(QStringLiteral("harmonicSeriesWidget"));
    QVERIFY(harmonicSeriesWidget != nullptr);
    QCOMPARE(harmonicSeriesWidget->harmonicStrengths(), std::vector<double>({1.0, 0.7, 0.4}));
}

void ToolConfigurationPanelTest::freshPanelShowsPlayPreviewNotStop() {
    // Direct user feedback: "practically wherever there is a visual
    // preview of something, give the user the ability to play an audio
    // preview of whatever it is" - the live counterpart to the Resource
    // Browser's own Tool Preset preview.
    ToolConfigurationPanel panel;
    auto* playButton = panel.findChild<QPushButton*>(QStringLiteral("playPreviewButton"));
    auto* stopButton = panel.findChild<QPushButton*>(QStringLiteral("stopPreviewButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);
    QVERIFY(!playButton->isHidden());
    QVERIFY(stopButton->isHidden());
}

void ToolConfigurationPanelTest::clickingPlayPreviewEmitsPreviewRequested() {
    ToolConfigurationPanel panel;
    auto* playButton = panel.findChild<QPushButton*>(QStringLiteral("playPreviewButton"));
    QVERIFY(playButton != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::previewRequested);

    playButton->click();

    QCOMPARE(spy.count(), 1);
}

void ToolConfigurationPanelTest::clickingStopPreviewEmitsStopPreviewRequested() {
    ToolConfigurationPanel panel;
    panel.setPreviewPlaying(true);
    auto* stopButton = panel.findChild<QPushButton*>(QStringLiteral("stopPreviewButton"));
    QVERIFY(stopButton != nullptr);
    QSignalSpy spy(&panel, &ToolConfigurationPanel::stopPreviewRequested);

    stopButton->click();

    QCOMPARE(spy.count(), 1);
}

void ToolConfigurationPanelTest::setPreviewPlayingTogglesWhichButtonIsShown() {
    ToolConfigurationPanel panel;
    auto* playButton = panel.findChild<QPushButton*>(QStringLiteral("playPreviewButton"));
    auto* stopButton = panel.findChild<QPushButton*>(QStringLiteral("stopPreviewButton"));

    panel.setPreviewPlaying(true);
    QVERIFY(playButton->isHidden());
    QVERIFY(!stopButton->isHidden());

    panel.setPreviewPlaying(false);
    QVERIFY(!playButton->isHidden());
    QVERIFY(stopButton->isHidden());
}
