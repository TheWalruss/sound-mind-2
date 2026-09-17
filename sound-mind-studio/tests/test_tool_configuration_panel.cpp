#include "test_tool_configuration_panel.h"

#include <cstdint>
#include <memory>
#include <optional>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_shot.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/tool_configuration_panel.h"

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
using sound_mind::core::OrderChaosConfiguration;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::SmudgeConfiguration;
using sound_mind::core::SoftenConfiguration;
using sound_mind::core::StampMode;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::ToolType;
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

}  // namespace

void ToolConfigurationPanelTest::freshPanelIsAnOpaqueCircularBrush() {
    const ToolConfigurationPanel panel;
    const ToolConfiguration& config = panel.toolConfiguration();
    QCOMPARE(config.type(), ToolType::Procedural);
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(config).tipShape(), BrushTipShape::Circle);
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
    // displayByteToDb(128) ~= -47.8 dB, displayByteToDb(64) ~= -71.9 dB, over
    // the -96..0 dB display range dbToDisplayByte()/displayByteToDb() share
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
    // own docs) - dbToDisplayByte()/displayByteToDb() are exact inverses over
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

// --- Mind Shots (v0.Y.33.1 Installment A) -----------------------------------

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
