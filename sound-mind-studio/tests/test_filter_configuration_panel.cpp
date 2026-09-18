#include "test_filter_configuration_panel.h"

#include <array>
#include <optional>
#include <utility>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSignalSpy>
#include <QSpinBox>
#include <QVariant>
#include <QtTest/QtTest>

#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/tone_curve_editor.h"

using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::MindWaveId;
using sound_mind::studio::FilterConfigurationPanel;
using sound_mind::studio::ToneCurveEditor;

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

void FilterConfigurationPanelTest::freshPanelShowsOnlyTheFrequencyAxisGradientGroup() {
    const FilterConfigurationPanel panel;

    QVERIFY(!panel.findChild<QWidget*>(QStringLiteral("frequencyAxisGradientSection"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("uniformBlurGroup"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("edgePreservingBlurGroup"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("directionalBlurGroup"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("sharpenGroup"))->isHidden());
}

void FilterConfigurationPanelTest::selectingAFilterTypeShowsOnlyThatTypesOwnGroupAndEmitsTheNewType() {
    FilterConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("filterTypeCombo"));
    QVERIFY(combo != nullptr);
    std::optional<FilterConfiguration> received;
    connect(&panel, &FilterConfigurationPanel::filterConfigurationChanged,
            [&](const FilterConfiguration& config) { received = config; });

    const int index = combo->findData(QVariant::fromValue(static_cast<int>(FilterType::DirectionalBlur)));
    QVERIFY(index >= 0);
    combo->setCurrentIndex(index);

    QVERIFY(received.has_value());
    QCOMPARE(received->type(), FilterType::DirectionalBlur);
    QCOMPARE(panel.filterConfiguration().type(), FilterType::DirectionalBlur);
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("frequencyAxisGradientSection"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("uniformBlurGroup"))->isHidden());
    QVERIFY(!panel.findChild<QGroupBox*>(QStringLiteral("directionalBlurGroup"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("sharpenGroup"))->isHidden());
}

void FilterConfigurationPanelTest::changingBlurSigmaUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("blurSigmaSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(5.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().blurSigma(), 5.5f);
}

void FilterConfigurationPanelTest::changingMedianSizeUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QSpinBox*>(QStringLiteral("medianSizeSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(9);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().medianSize(), 9);
}

void FilterConfigurationPanelTest::changingDirectionalBlurLengthAndAngleUpdateConfigAndEmit() {
    FilterConfigurationPanel panel;
    auto* lengthSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("directionalBlurLengthSpinBox"));
    auto* angleSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("directionalBlurAngleSpinBox"));
    QVERIFY(lengthSpinBox != nullptr);
    QVERIFY(angleSpinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    lengthSpinBox->setValue(42);
    angleSpinBox->setValue(135.0);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(panel.filterConfiguration().directionalBlurLength(), 42);
    QCOMPARE(panel.filterConfiguration().directionalBlurAngleDegrees(), 135.0f);
}

void FilterConfigurationPanelTest::changingSharpenAmountUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("sharpenAmountSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(2.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().sharpenAmount(), 2.5f);
}

void FilterConfigurationPanelTest::setFilterConfigurationSyncsTheTypeComboAndNewSpinBoxesWithoutEmitting() {
    FilterConfigurationPanel panel;
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setBlurSigma(3.3f);
    config.setMedianSize(7);
    config.setDirectionalBlurLength(20);
    config.setDirectionalBlurAngleDegrees(45.0f);
    config.setSharpenAmount(1.8f);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.setFilterConfiguration(config);

    QCOMPARE(spy.count(), 0);
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("filterTypeCombo"));
    QCOMPARE(combo->currentData().toInt(), static_cast<int>(FilterType::Sharpen));
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("blurSigmaSpinBox"))->value(), 3.3);
    QCOMPARE(panel.findChild<QSpinBox*>(QStringLiteral("medianSizeSpinBox"))->value(), 7);
    QCOMPARE(panel.findChild<QSpinBox*>(QStringLiteral("directionalBlurLengthSpinBox"))->value(), 20);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("directionalBlurAngleSpinBox"))->value(), 45.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("sharpenAmountSpinBox"))->value(), 1.8);
    // Sharpen's own group is now the visible one.
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("frequencyAxisGradientSection"))->isHidden());
    QVERIFY(!panel.findChild<QGroupBox*>(QStringLiteral("sharpenGroup"))->isHidden());
}

void FilterConfigurationPanelTest::selectingToneCurveShowsItsOwnGroup() {
    FilterConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("filterTypeCombo"));
    QVERIFY(combo != nullptr);

    const int index = combo->findData(QVariant::fromValue(static_cast<int>(FilterType::ToneCurve)));
    QVERIFY(index >= 0);
    combo->setCurrentIndex(index);

    QCOMPARE(panel.filterConfiguration().type(), FilterType::ToneCurve);
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("frequencyAxisGradientSection"))->isHidden());
    QVERIFY(!panel.findChild<QGroupBox*>(QStringLiteral("toneCurveGroup"))->isHidden());
}

void FilterConfigurationPanelTest::editingTheToneCurveEditorUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    FilterConfiguration config;
    config.setType(FilterType::ToneCurve);
    panel.setFilterConfiguration(config);

    auto* editor = panel.findChild<ToneCurveEditor*>(QStringLiteral("toneCurveEditor"));
    QVERIFY(editor != nullptr);
    editor->resize(240, 160);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    // Drags the first point (widget-space (8,152) for a 240x160 widget
    // with ToneCurveEditor's own 8px plot margin) up to y=0.5.
    QTest::mousePress(editor, Qt::LeftButton, Qt::NoModifier, QPoint(8, 152));
    QTest::mouseMove(editor, QPoint(8, 80));
    QTest::mouseRelease(editor, Qt::LeftButton, Qt::NoModifier, QPoint(8, 80));

    QVERIFY(spy.count() >= 1);
    const auto& points = panel.filterConfiguration().toneCurvePoints();
    QVERIFY(qAbs(points.front()[0] - 0.0f) < 0.01);
    QVERIFY(qAbs(points.front()[1] - 0.5f) < 0.02);
}

void FilterConfigurationPanelTest::setFilterConfigurationSyncsTheToneCurveEditorWithoutEmitting() {
    FilterConfigurationPanel panel;
    FilterConfiguration config;
    config.setType(FilterType::ToneCurve);
    config.setToneCurvePoints({{0.0f, 0.0f}, {0.3f, 0.1f}, {1.0f, 1.0f}});
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.setFilterConfiguration(config);

    QCOMPARE(spy.count(), 0);
    auto* editor = panel.findChild<ToneCurveEditor*>(QStringLiteral("toneCurveEditor"));
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->points(), config.toneCurvePoints());
}

void FilterConfigurationPanelTest::equalizerModeHidesTheFilterTypeComboAndShowsTheCutGroup() {
    FilterConfigurationPanel panel;

    panel.setEqualizerMode(true);

    QVERIFY(panel.findChild<QComboBox*>(QStringLiteral("filterTypeCombo"))->isHidden());
    QVERIFY(!panel.findChild<QGroupBox*>(QStringLiteral("equalizerCutGroup"))->isHidden());
    // Every per-type group stays hidden while in Equalizer mode, even
    // though config_.type() is still FrequencyAxisGradient (the default).
    QVERIFY(panel.findChild<QWidget*>(QStringLiteral("frequencyAxisGradientSection"))->isHidden());
}

void FilterConfigurationPanelTest::equalizerModeOffRestoresTheNormalPerTypeGroup() {
    FilterConfigurationPanel panel;
    panel.setEqualizerMode(true);

    panel.setEqualizerMode(false);

    QVERIFY(!panel.findChild<QComboBox*>(QStringLiteral("filterTypeCombo"))->isHidden());
    QVERIFY(panel.findChild<QGroupBox*>(QStringLiteral("equalizerCutGroup"))->isHidden());
    QVERIFY(!panel.findChild<QWidget*>(QStringLiteral("frequencyAxisGradientSection"))->isHidden());
}

void FilterConfigurationPanelTest::editingACutSpinBoxWritesOpacityAndForcesIntensityToTheSilenceFloor() {
    FilterConfigurationPanel panel;
    panel.setEqualizerMode(true);
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("startLeftCutSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(0.75);

    QCOMPARE(spy.count(), 1);
    const auto& stop = panel.filterConfiguration().frequencyGradient().stops().front();
    QCOMPARE(stop.leftOpacity, 0.75f);
    QCOMPARE(stop.leftIntensity, -96.0f);
    // The other channel/endpoint are untouched.
    QCOMPARE(stop.rightOpacity, 0.0f);
    QCOMPARE(panel.filterConfiguration().frequencyGradient().stops().back().leftOpacity, 0.0f);
}

void FilterConfigurationPanelTest::setFilterConfigurationSyncsTheCutSpinBoxesFromOpacityWithoutEmitting() {
    FilterConfigurationPanel panel;
    panel.setEqualizerMode(true);
    FilterConfiguration config;
    config.frequencyGradient().setStopValues(0, {0.0f, -96.0f, -96.0f, 0.3f, 0.4f});
    config.frequencyGradient().setStopValues(1, {1.0f, -96.0f, -96.0f, 0.5f, 0.6f});
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.setFilterConfiguration(config);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("startLeftCutSpinBox"))->value(), 0.3);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("startRightCutSpinBox"))->value(), 0.4);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("endLeftCutSpinBox"))->value(), 0.5);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("endRightCutSpinBox"))->value(), 0.6);
}

// --- v0.Y.31.1 Installment D3: filter-parameter MindWave bindings -----

namespace {

/// @brief Every one of the five bindable parameters' own combo object
/// name - shared by every test below that needs to iterate all five.
const std::array<QString, 5> kBindComboNames{{
    QStringLiteral("blurSigmaMindWaveCombo"),
    QStringLiteral("medianSizeMindWaveCombo"),
    QStringLiteral("directionalBlurLengthMindWaveCombo"),
    QStringLiteral("directionalBlurAngleMindWaveCombo"),
    QStringLiteral("sharpenAmountMindWaveCombo"),
}};

}  // namespace

void FilterConfigurationPanelTest::freshCombosOfferOnlyNoneUntilSetAvailableMindWavesIsCalled() {
    const FilterConfigurationPanel panel;
    for (const auto& comboName : kBindComboNames) {
        auto* combo = panel.findChild<QComboBox*>(comboName);
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 1);
        QCOMPARE(combo->currentText(), QStringLiteral("None"));
    }
}

void FilterConfigurationPanelTest::setAvailableMindWavesPopulatesEveryCombo() {
    FilterConfigurationPanel panel;

    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")},
                                  {MindWaveId{6}, QStringLiteral("Fast Pulse")}});

    for (const auto& comboName : kBindComboNames) {
        auto* combo = panel.findChild<QComboBox*>(comboName);
        QCOMPARE(combo->count(), 3);  // None + two MindWaves.
        QCOMPARE(combo->itemText(1), QStringLiteral("Slow Pulse"));
        QCOMPARE(combo->itemText(2), QStringLiteral("Fast Pulse"));
    }
}

void FilterConfigurationPanelTest::changingABindCombosEmitsFilterConfigurationChangedWithTheNewBinding() {
    FilterConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.findChild<QComboBox*>(QStringLiteral("blurSigmaMindWaveCombo"))->setCurrentIndex(1);  // "Slow Pulse".

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().blurSigmaMindWave(), std::optional<MindWaveId>(MindWaveId{5}));
    // Every other bindable parameter is untouched.
    QVERIFY(!panel.filterConfiguration().medianSizeMindWave().has_value());
}

void FilterConfigurationPanelTest::selectingNoneUnbindsAndEmits() {
    FilterConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("sharpenAmountMindWaveCombo"));
    combo->setCurrentIndex(1);  // Bind first.
    QVERIFY(panel.filterConfiguration().sharpenAmountMindWave().has_value());
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    combo->setCurrentIndex(0);  // "None".

    QCOMPARE(spy.count(), 1);
    QVERIFY(!panel.filterConfiguration().sharpenAmountMindWave().has_value());
}

void FilterConfigurationPanelTest::setFilterConfigurationSyncsAllFiveCombosWithoutEmitting() {
    FilterConfigurationPanel panel;
    panel.setAvailableMindWaves({{MindWaveId{5}, QStringLiteral("Slow Pulse")}});
    FilterConfiguration config;
    config.setBlurSigmaMindWave(MindWaveId{5});
    config.setMedianSizeMindWave(MindWaveId{5});
    config.setDirectionalBlurLengthMindWave(MindWaveId{5});
    config.setDirectionalBlurAngleMindWave(MindWaveId{5});
    config.setSharpenAmountMindWave(MindWaveId{5});
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.setFilterConfiguration(config);

    QCOMPARE(spy.count(), 0);
    for (const auto& comboName : kBindComboNames) {
        auto* combo = panel.findChild<QComboBox*>(comboName);
        QCOMPARE(combo->currentText(), QStringLiteral("Slow Pulse"));
    }
}

// --- v0.Y.36.1 Installment A: Noise & distortion -----------------------

namespace {

/// @brief Every one of the eight new Noise & distortion groups' own
/// object name - shared by every test below that needs to iterate them.
const std::array<QString, 8> kNoiseGroupNames{{
    QStringLiteral("speckleAddGroup"),
    QStringLiteral("speckleRemoveGroup"),
    QStringLiteral("denoiseGroup"),
    QStringLiteral("bitDepthCrushGroup"),
    QStringLiteral("granularNoiseGroup"),
    QStringLiteral("dynamicSpeckleGroup"),
    QStringLiteral("feedbackDistortionGroup"),
    QStringLiteral("spectralWavefoldGroup"),
}};

const std::array<FilterType, 8> kNoiseTypesInGroupOrder{{
    FilterType::SpeckleAdd,
    FilterType::SpeckleRemove,
    FilterType::Denoise,
    FilterType::BitDepthCrush,
    FilterType::GranularNoise,
    FilterType::DynamicSpeckle,
    FilterType::FeedbackDistortion,
    FilterType::SpectralWavefold,
}};

}  // namespace

void FilterConfigurationPanelTest::freshPanelHasAllEightNoiseGroupsHidden() {
    const FilterConfigurationPanel panel;
    for (const auto& groupName : kNoiseGroupNames) {
        QVERIFY(panel.findChild<QGroupBox*>(groupName)->isHidden());
    }
}

void FilterConfigurationPanelTest::selectingEachNoiseTypeShowsOnlyItsOwnGroup() {
    FilterConfigurationPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("filterTypeCombo"));
    QVERIFY(combo != nullptr);

    for (std::size_t i = 0; i < kNoiseTypesInGroupOrder.size(); ++i) {
        const int index = combo->findData(QVariant::fromValue(static_cast<int>(kNoiseTypesInGroupOrder[i])));
        QVERIFY(index >= 0);
        combo->setCurrentIndex(index);

        for (std::size_t j = 0; j < kNoiseGroupNames.size(); ++j) {
            const bool shouldBeVisible = (i == j);
            QCOMPARE(!panel.findChild<QGroupBox*>(kNoiseGroupNames[j])->isHidden(), shouldBeVisible);
        }
    }
}

void FilterConfigurationPanelTest::changingSpeckleAddDensityAndIntensityUpdateConfigAndEmit() {
    FilterConfigurationPanel panel;
    auto* densitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleAddDensitySpinBox"));
    auto* intensitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleAddIntensitySpinBox"));
    QVERIFY(densitySpinBox != nullptr);
    QVERIFY(intensitySpinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    densitySpinBox->setValue(0.3);
    intensitySpinBox->setValue(0.6);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(panel.filterConfiguration().speckleDensity(), 0.3f);
    QCOMPARE(panel.filterConfiguration().speckleIntensity(), 0.6f);
}

void FilterConfigurationPanelTest::changingSpeckleThresholdUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleThresholdSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(20.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().speckleThresholdDb(), 20.0f);
}

void FilterConfigurationPanelTest::changingDenoiseNoiseFloorAndReductionUpdateConfigAndEmit() {
    FilterConfigurationPanel panel;
    auto* floorSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("noiseFloorSpinBox"));
    auto* reductionSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("reductionSpinBox"));
    QVERIFY(floorSpinBox != nullptr);
    QVERIFY(reductionSpinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    floorSpinBox->setValue(-50.0);
    reductionSpinBox->setValue(18.0);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(panel.filterConfiguration().noiseFloorDb(), -50.0f);
    QCOMPARE(panel.filterConfiguration().reductionDb(), 18.0f);
}

void FilterConfigurationPanelTest::changingCrushAmountUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("crushAmountSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(0.9);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().crushAmount(), 0.9f);
}

void FilterConfigurationPanelTest::changingGrainSizeAndAmountUpdateConfigAndEmit() {
    FilterConfigurationPanel panel;
    auto* sizeSpinBox = panel.findChild<QSpinBox*>(QStringLiteral("grainSizeSpinBox"));
    auto* amountSpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("grainAmountSpinBox"));
    QVERIFY(sizeSpinBox != nullptr);
    QVERIFY(amountSpinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    sizeSpinBox->setValue(8);
    amountSpinBox->setValue(3.0);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(panel.filterConfiguration().grainSize(), 8);
    QCOMPARE(panel.filterConfiguration().grainAmountDb(), 3.0f);
}

void FilterConfigurationPanelTest::changingDynamicSpeckleDensityAndIntensityUpdateTheSameFieldsAsSpeckleAdd() {
    FilterConfigurationPanel panel;
    auto* densitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("dynamicSpeckleDensitySpinBox"));
    auto* intensitySpinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("dynamicSpeckleIntensitySpinBox"));
    QVERIFY(densitySpinBox != nullptr);
    QVERIFY(intensitySpinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    densitySpinBox->setValue(0.4);
    intensitySpinBox->setValue(0.7);

    QCOMPARE(spy.count(), 2);
    // Shares SpeckleAdd's own underlying fields - see this class's own docs.
    QCOMPARE(panel.filterConfiguration().speckleDensity(), 0.4f);
    QCOMPARE(panel.filterConfiguration().speckleIntensity(), 0.7f);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleAddDensitySpinBox"))->value(), 0.4);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleAddIntensitySpinBox"))->value(), 0.7);
}

void FilterConfigurationPanelTest::changingFeedbackAmountUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("feedbackAmountSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(0.75);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().feedbackAmount(), 0.75f);
}

void FilterConfigurationPanelTest::changingFoldGainUpdatesConfigAndEmits() {
    FilterConfigurationPanel panel;
    auto* spinBox = panel.findChild<QDoubleSpinBox*>(QStringLiteral("foldGainSpinBox"));
    QVERIFY(spinBox != nullptr);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    spinBox->setValue(4.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.filterConfiguration().foldGain(), 4.0f);
}

void FilterConfigurationPanelTest::setFilterConfigurationSyncsAllNoiseSpinBoxesWithoutEmitting() {
    FilterConfigurationPanel panel;
    FilterConfiguration config;
    config.setType(FilterType::GranularNoise);
    config.setSpeckleDensity(0.3f);
    config.setSpeckleIntensity(0.6f);
    config.setSpeckleThresholdDb(15.0f);
    config.setNoiseFloorDb(-45.0f);
    config.setReductionDb(15.0f);
    config.setCrushAmount(0.4f);
    config.setGrainSize(6);
    config.setGrainAmountDb(4.0f);
    config.setFeedbackAmount(0.6f);
    config.setFoldGain(3.0f);
    QSignalSpy spy(&panel, &FilterConfigurationPanel::filterConfigurationChanged);

    panel.setFilterConfiguration(config);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleAddDensitySpinBox"))->value(), 0.3);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleAddIntensitySpinBox"))->value(), 0.6);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("dynamicSpeckleDensitySpinBox"))->value(), 0.3);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("dynamicSpeckleIntensitySpinBox"))->value(), 0.6);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("speckleThresholdSpinBox"))->value(), 15.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("noiseFloorSpinBox"))->value(), -45.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("reductionSpinBox"))->value(), 15.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("crushAmountSpinBox"))->value(), 0.4);
    QCOMPARE(panel.findChild<QSpinBox*>(QStringLiteral("grainSizeSpinBox"))->value(), 6);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("grainAmountSpinBox"))->value(), 4.0);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("feedbackAmountSpinBox"))->value(), 0.6);
    QCOMPARE(panel.findChild<QDoubleSpinBox*>(QStringLiteral("foldGainSpinBox"))->value(), 3.0);
    // GranularNoise's own group is now the visible one.
    QVERIFY(!panel.findChild<QGroupBox*>(QStringLiteral("granularNoiseGroup"))->isHidden());
}
