#include "sound_mind/studio/mind_wave_editor.h"

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/studio/numeric_field.h"
#include "sound_mind/studio/rotary_dial_widget.h"
#include "sound_mind/studio/waveform_preview_widget.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::EnvelopeShape;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::SpatialPattern;
using sound_mind::core::SteppedNoiseShape;

constexpr std::array<std::pair<GeneratorType, const char*>, 10> kGeneratorTypes{{
    {GeneratorType::Periodic, "Periodic"},
    {GeneratorType::Envelope, "Envelope"},
    {GeneratorType::SteppedNoise, "Stepped/Noise"},
    {GeneratorType::Spatial, "Spatial"},
    {GeneratorType::Fractal, "Fractal"},
    {GeneratorType::Drawn, "Drawn"},
    {GeneratorType::StepGrid, "Step Grid"},
    {GeneratorType::Continuous, "Continuous"},
    {GeneratorType::Resonance, "Resonance"},
    {GeneratorType::Noise, "Noise"},
}};

/// @brief The noise colors a noise MindWave offers - Gray (an audio-loudness
/// notion) only makes sense for layer noise; Custom takes a slope instead.
constexpr std::array<std::pair<sound_mind::core::NoiseColor, const char*>, 5> kMindWaveNoiseColors{{
    {sound_mind::core::NoiseColor::White, "White"},
    {sound_mind::core::NoiseColor::Pink, "Pink"},
    {sound_mind::core::NoiseColor::Red, "Red / Brownian"},
    {sound_mind::core::NoiseColor::Purple, "Purple / Violet"},
    {sound_mind::core::NoiseColor::Custom, "Custom slope"},
}};

constexpr std::array<std::pair<MindWaveAxis, const char*>, 2> kAxes{{
    {MindWaveAxis::Time, "Time"},
    {MindWaveAxis::Frequency, "Frequency"},
}};

constexpr std::array<std::pair<PeriodicWaveform, const char*>, 6> kPeriodicWaveforms{{
    {PeriodicWaveform::Sine, "Sine"},
    {PeriodicWaveform::Triangle, "Triangle"},
    {PeriodicWaveform::Square, "Square"},
    {PeriodicWaveform::Sawtooth, "Sawtooth"},
    {PeriodicWaveform::Pulse, "Pulse"},
    {PeriodicWaveform::RectifiedSine, "Rectified Sine"},
}};

constexpr std::array<std::pair<EnvelopeShape, const char*>, 5> kEnvelopeShapes{{
    {EnvelopeShape::ExponentialDecay, "Exponential Decay"},
    {EnvelopeShape::DecayingOscillation, "Decaying Oscillation"},
    {EnvelopeShape::SCurve, "S-Curve"},
    {EnvelopeShape::Peak, "Peak (Bell)"},
    {EnvelopeShape::Ramp, "Ramp"},
}};

constexpr std::array<std::pair<SteppedNoiseShape, const char*>, 3> kSteppedNoiseShapes{{
    {SteppedNoiseShape::Stepped, "Stepped"},
    {SteppedNoiseShape::GaussianNoise, "Gaussian Noise"},
    {SteppedNoiseShape::FractalNoise, "Fractal Noise"},
}};

constexpr std::array<std::pair<SpatialPattern, const char*>, 4> kSpatialPatterns{{
    {SpatialPattern::Ripples, "Ripples"},
    {SpatialPattern::Checkerboard, "Checkerboard"},
    {SpatialPattern::Cellular, "Cellular"},
    {SpatialPattern::DomainWarpedNoise, "Domain-Warped Noise"},
}};

/// @brief How many samples `updateContinuousPreview()` asks
/// `reduceMindWaveToSignal()` for - smooth enough for
/// `WaveformPreviewWidget`'s own `240px`-wide `sizeHint()` without being
/// wasteful to recompute on every single spin-box/dial edit.
constexpr std::uint32_t kContinuousPreviewSampleCount = 200;

/// @brief Wraps `dial` and `spinBox` side by side - the same role
/// `FilterConfigurationPanel`'s/`ToolConfigurationPanel`'s own
/// `makeBoundFieldRow()` plays for a spin-box-plus-combo pair, duplicated
/// here rather than shared across translation units (none of the three
/// expose a header any UI code outside their own translation unit would
/// reach into).
QHBoxLayout* makeDialFieldRow(QWidget* dial, QWidget* spinBox) {
    auto* row = new QHBoxLayout();
    row->addWidget(dial);
    row->addWidget(spinBox, 1);
    return row;
}

/// @brief Where the time-axis sliders (Period, Envelope Center, Ripples'
///        Center X) end, in seconds - a minute covers any useful cycle or
///        position; longer values can still be typed (v0.1.51.1).
constexpr double kTimeSliderMaximumSeconds = 60.0;
/// @brief Where the frequency-axis sliders end, in bins - a typical
///        project's own height.
constexpr double kFrequencySliderMaximumBins = 512.0;

/// @brief A wide-range, general-purpose double spin box - most of this
/// editor's own fields have no natural bound (a period, a phase, a noise
/// scale...), unlike FilterConfigurationPanel's own tightly-ranged fields
/// (a dB value, an opacity). Matches this editor's own "bare-bones" scope:
/// plain numeric entry, not a validated/clamped control.
NumericField* makeWideSpinBox(QWidget* parent, const QString& objectName, double minimum = -1'000'000.0,
                                  double maximum = 1'000'000.0, int decimals = 3) {
    auto* spinBox = new NumericField(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(decimals);
    spinBox->setSingleStep(0.1);
    return spinBox;
}

template <typename EnumType, std::size_t N>
void populateCombo(QComboBox* combo, const std::array<std::pair<EnumType, const char*>, N>& entries) {
    for (const auto& [value, name] : entries) {
        combo->addItem(QObject::tr(name), QVariant::fromValue(static_cast<int>(value)));
    }
}

template <typename EnumType>
void selectComboValue(QComboBox* combo, EnumType value) {
    const int index = combo->findData(QVariant::fromValue(static_cast<int>(value)));
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

}  // namespace

MindWaveEditor::MindWaveEditor(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* topForm = new QFormLayout();

    generatorTypeCombo_ = new QComboBox(this);
    generatorTypeCombo_->setObjectName(QStringLiteral("generatorTypeCombo"));
    populateCombo(generatorTypeCombo_, kGeneratorTypes);
    connect(generatorTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setType(static_cast<GeneratorType>(generatorTypeCombo_->itemData(index).toInt()));
        updateVisibleFields();
        emitChanged();
    });
    topForm->addRow(tr("Generator Type:"), generatorTypeCombo_);

    // The settings several types share, grouped by what they do (v0.1.51.1)
    // and placed after the type's own group below; rows a type doesn't use
    // are hidden - see updateVisibleFields().
    axisGroup_ = new QGroupBox(tr("Axis & Timing"), this);
    axisGroup_->setObjectName(QStringLiteral("axisGroup"));
    auto* axisForm = new QFormLayout(axisGroup_);
    randomnessGroup_ = new QGroupBox(tr("Randomness"), this);
    randomnessGroup_->setObjectName(QStringLiteral("randomnessGroup"));
    auto* randomnessForm = new QFormLayout(randomnessGroup_);
    advancedToggle_ = new QToolButton(this);
    advancedToggle_->setObjectName(QStringLiteral("advancedToggleButton"));
    advancedToggle_->setText(tr("Advanced"));
    advancedToggle_->setCheckable(true);
    advancedToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    advancedToggle_->setArrowType(Qt::RightArrow);
    advancedToggle_->setAutoRaise(true);
    advancedToggle_->setToolTip(tr("Phase, noise detail, and reversing or inverting the shape"));
    advancedContainer_ = new QWidget(this);
    advancedContainer_->setObjectName(QStringLiteral("advancedContainer"));
    advancedContainer_->hide();
    auto* advancedForm = new QFormLayout(advancedContainer_);
    advancedForm->setContentsMargins(0, 0, 0, 0);
    connect(advancedToggle_, &QToolButton::toggled, this, [this](bool open) {
        advancedToggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        advancedContainer_->setVisible(open);
    });

    axisCombo_ = new QComboBox(this);
    axisCombo_->setObjectName(QStringLiteral("axisCombo"));
    populateCombo(axisCombo_, kAxes);
    connect(axisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setAxis(static_cast<MindWaveAxis>(axisCombo_->itemData(index).toInt()));
        updateVisibleFields();  // the axis sets the sliders' units.
        emitChanged();
    });
    axisForm->addRow(tr("Axis:"), axisCombo_);

    periodSpinBox_ = makeWideSpinBox(this, QStringLiteral("periodSpinBox"), 0.000001, 1'000'000.0);
    periodSpinBox_->setLogarithmicSlider(true);  // its slider range follows the axis - see updateVisibleFields().
    periodSpinBox_->setToolTip(
        tr("Cycle length - seconds (Time axis) or bins (Frequency axis). Used by Periodic, Envelope's Decaying "
           "Oscillation, Stepped/Noise's Stepped, Spatial's Ripples/Checkerboard, and Fractal."));
    connect(periodSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setPeriod(value);
        emitChanged();
    });
    axisForm->addRow(tr("Period:"), periodSpinBox_);

    // One cycle is the whole range: any phase beyond 2 pi repeats one within it.
    phaseSpinBox_ = makeWideSpinBox(this, QStringLiteral("phaseSpinBox"), 0.0, 2.0 * std::numbers::pi);
    phaseSpinBox_->setToolTip(tr("Phase offset, in radians - one cycle is 0 to 6.283 (2 pi)."));
    connect(phaseSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setPhaseRadians(value);
        emitChanged();
    });
    advancedForm->addRow(tr("Phase (radians):"), phaseSpinBox_);

    reversedCheckBox_ = new QCheckBox(tr("Reversed"), this);
    reversedCheckBox_->setObjectName(QStringLiteral("reversedCheckBox"));
    reversedCheckBox_->setToolTip(
        tr("Run backwards along the axis. An Exponential Decay becomes a rise toward its center."));
    connect(reversedCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        wave_.setReversed(checked);
        emitChanged();
    });
    advancedForm->addRow(QString(), reversedCheckBox_);

    invertedCheckBox_ = new QCheckBox(tr("Inverted"), this);
    invertedCheckBox_->setObjectName(QStringLiteral("invertedCheckBox"));
    invertedCheckBox_->setToolTip(tr("Flip the polarity: every value v becomes 1 - v."));
    connect(invertedCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        wave_.setInverted(checked);
        emitChanged();
    });
    advancedForm->addRow(QString(), invertedCheckBox_);

    seedSpinBox_ = new IntegerField(this);
    seedSpinBox_->setObjectName(QStringLiteral("seedSpinBox"));
    // MindWave::seed() is a full uint32_t; IntegerField is int-only - a UI-
    // level limit (matching LayersPanel's own translationSpinBox docs for
    // the same int-vs-wider-type shape), not a Core-side restriction.
    seedSpinBox_->setRange(0, std::numeric_limits<int>::max());
    seedSpinBox_->setToolTip(tr("Seed for every noise-based shape (Gaussian Noise, Fractal Noise, Cellular, "
                                 "Domain-Warped Noise, and Fractal)."));
    connect(seedSpinBox_, &IntegerField::valueChanged, this, [this](int value) {
        wave_.setSeed(static_cast<std::uint32_t>(value));
        emitChanged();
    });
    randomnessForm->addRow(tr("Seed:"), seedSpinBox_);

    noiseScaleSpinBox_ = makeWideSpinBox(this, QStringLiteral("noiseScaleSpinBox"), 0.000001, 1'000'000.0);
    noiseScaleSpinBox_->setLogarithmicSlider(true);
    noiseScaleSpinBox_->setSliderRange(0.01, 100.0);
    noiseScaleSpinBox_->setToolTip(tr("Lattice/cell size for every noise-based shape."));
    connect(noiseScaleSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setNoiseScale(value);
        emitChanged();
    });
    randomnessForm->addRow(tr("Noise Scale:"), noiseScaleSpinBox_);

    noiseOctavesSpinBox_ = new IntegerField(this);
    noiseOctavesSpinBox_->setObjectName(QStringLiteral("noiseOctavesSpinBox"));
    noiseOctavesSpinBox_->setRange(1, 16);
    noiseOctavesSpinBox_->setToolTip(tr("Octave count for Fractal Noise and Domain-Warped Noise."));
    connect(noiseOctavesSpinBox_, &IntegerField::valueChanged, this, [this](int value) {
        wave_.setNoiseOctaves(value);
        emitChanged();
    });
    advancedForm->addRow(tr("Noise Octaves:"), noiseOctavesSpinBox_);

    noisePersistenceSpinBox_ = new NumericField(this);
    noisePersistenceSpinBox_->setObjectName(QStringLiteral("noisePersistenceSpinBox"));
    noisePersistenceSpinBox_->setRange(0.0, 1.0);
    noisePersistenceSpinBox_->setSingleStep(0.05);
    noisePersistenceSpinBox_->setDecimals(2);
    noisePersistenceSpinBox_->setToolTip(tr("Octave falloff for Fractal Noise and Domain-Warped Noise."));
    connect(noisePersistenceSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setNoisePersistence(value);
        emitChanged();
    });
    advancedForm->addRow(tr("Noise Persistence:"), noisePersistenceSpinBox_);

    root->addLayout(topForm);

    periodicGroup_ = new QGroupBox(tr("Periodic"), this);
    periodicGroup_->setObjectName(QStringLiteral("periodicGroup"));
    auto* periodicForm = new QFormLayout(periodicGroup_);
    periodicWaveformCombo_ = new QComboBox(periodicGroup_);
    periodicWaveformCombo_->setObjectName(QStringLiteral("periodicWaveformCombo"));
    populateCombo(periodicWaveformCombo_, kPeriodicWaveforms);
    connect(periodicWaveformCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setPeriodicWaveform(static_cast<PeriodicWaveform>(periodicWaveformCombo_->itemData(index).toInt()));
        updateVisibleFields();
        emitChanged();
    });
    periodicForm->addRow(tr("Waveform:"), periodicWaveformCombo_);
    dutyCycleSpinBox_ = new NumericField(periodicGroup_);
    dutyCycleSpinBox_->setObjectName(QStringLiteral("dutyCycleSpinBox"));
    dutyCycleSpinBox_->setRange(0.0, 1.0);
    dutyCycleSpinBox_->setSingleStep(0.05);
    dutyCycleSpinBox_->setDecimals(2);
    dutyCycleSpinBox_->setToolTip(tr("The share of each cycle the pulse is high - 0.5 makes it a square wave."));
    connect(dutyCycleSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setDutyCycle(value);
        emitChanged();
    });
    periodicForm->addRow(tr("Duty Cycle:"), dutyCycleSpinBox_);
    root->addWidget(periodicGroup_);

    envelopeGroup_ = new QGroupBox(tr("Envelope"), this);
    envelopeGroup_->setObjectName(QStringLiteral("envelopeGroup"));
    auto* envelopeForm = new QFormLayout(envelopeGroup_);
    envelopeShapeCombo_ = new QComboBox(envelopeGroup_);
    envelopeShapeCombo_->setObjectName(QStringLiteral("envelopeShapeCombo"));
    populateCombo(envelopeShapeCombo_, kEnvelopeShapes);
    connect(envelopeShapeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setEnvelopeShape(static_cast<EnvelopeShape>(envelopeShapeCombo_->itemData(index).toInt()));
        updateVisibleFields();
        emitChanged();
    });
    envelopeForm->addRow(tr("Shape:"), envelopeShapeCombo_);
    envelopeCenterSpinBox_ = makeWideSpinBox(envelopeGroup_, QStringLiteral("envelopeCenterSpinBox"));
    connect(envelopeCenterSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setEnvelopeCenter(value);
        emitChanged();
    });
    envelopeForm->addRow(tr("Center:"), envelopeCenterSpinBox_);
    envelopeSteepnessSpinBox_ = makeWideSpinBox(envelopeGroup_, QStringLiteral("envelopeSteepnessSpinBox"));
    envelopeSteepnessSpinBox_->setToolTip(tr("How sharp the S-Curve, Peak or Ramp is."));
    envelopeSteepnessSpinBox_->setLogarithmicSlider(true);
    envelopeSteepnessSpinBox_->setSliderRange(0.01, 100.0);
    connect(envelopeSteepnessSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setEnvelopeSteepness(value);
        emitChanged();
    });
    envelopeForm->addRow(tr("Steepness:"), envelopeSteepnessSpinBox_);
    decayRateSpinBox_ = makeWideSpinBox(envelopeGroup_, QStringLiteral("decayRateSpinBox"));
    decayRateSpinBox_->setToolTip(tr("How fast the decay falls off."));
    decayRateSpinBox_->setLogarithmicSlider(true);
    decayRateSpinBox_->setSliderRange(0.01, 100.0);
    connect(decayRateSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setDecayRate(value);
        emitChanged();
    });
    envelopeForm->addRow(tr("Decay Rate:"), decayRateSpinBox_);
    root->addWidget(envelopeGroup_);

    noiseGroup_ = new QGroupBox(tr("Noise"), this);
    noiseGroup_->setObjectName(QStringLiteral("noiseGroup"));
    auto* noiseForm = new QFormLayout(noiseGroup_);
    noiseColorCombo_ = new QComboBox(noiseGroup_);
    noiseColorCombo_->setObjectName(QStringLiteral("noiseColorCombo"));
    populateCombo(noiseColorCombo_, kMindWaveNoiseColors);
    connect(noiseColorCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setNoiseColor(static_cast<sound_mind::core::NoiseColor>(noiseColorCombo_->itemData(index).toInt()));
        noiseSlopeSpinBox_->setEnabled(wave_.noiseColor() == sound_mind::core::NoiseColor::Custom);
        updateVisibleFields();
        emitChanged();
    });
    noiseForm->addRow(tr("Color:"), noiseColorCombo_);
    noiseSlopeSpinBox_ = new NumericField(noiseGroup_);
    noiseSlopeSpinBox_->setObjectName(QStringLiteral("noiseSlopeSpinBox"));
    noiseSlopeSpinBox_->setRange(-2.0, 2.0);
    noiseSlopeSpinBox_->setDecimals(2);
    noiseSlopeSpinBox_->setToolTip(tr("Custom slope: -2 red, -1 pink, 0 white, +2 purple."));
    connect(noiseSlopeSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setNoiseSlope(value);
        emitChanged();
    });
    noiseForm->addRow(tr("Slope:"), noiseSlopeSpinBox_);
    noiseCellWidthSpinBox_ = new NumericField(noiseGroup_);
    noiseCellWidthSpinBox_->setObjectName(QStringLiteral("noiseCellWidthSpinBox"));
    noiseCellWidthSpinBox_->setRange(0.001, 60.0);
    noiseCellWidthSpinBox_->setDecimals(3);
    noiseCellWidthSpinBox_->setToolTip(tr("Horizontal resolution: how many seconds one noise cell spans."));
    connect(noiseCellWidthSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setNoiseCellWidth(value);
        emitChanged();
    });
    noiseForm->addRow(tr("Cell Width (s):"), noiseCellWidthSpinBox_);
    noiseCellHeightSpinBox_ = new NumericField(noiseGroup_);
    noiseCellHeightSpinBox_->setObjectName(QStringLiteral("noiseCellHeightSpinBox"));
    noiseCellHeightSpinBox_->setRange(0.1, 2048.0);
    noiseCellHeightSpinBox_->setDecimals(1);
    noiseCellHeightSpinBox_->setToolTip(tr("Vertical resolution: how many frequency rows one noise cell spans."));
    connect(noiseCellHeightSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setNoiseCellHeight(value);
        emitChanged();
    });
    noiseForm->addRow(tr("Cell Height (bins):"), noiseCellHeightSpinBox_);
    root->addWidget(noiseGroup_);

    steppedNoiseGroup_ = new QGroupBox(tr("Stepped/Noise"), this);
    steppedNoiseGroup_->setObjectName(QStringLiteral("steppedNoiseGroup"));
    auto* steppedNoiseForm = new QFormLayout(steppedNoiseGroup_);
    steppedNoiseShapeCombo_ = new QComboBox(steppedNoiseGroup_);
    steppedNoiseShapeCombo_->setObjectName(QStringLiteral("steppedNoiseShapeCombo"));
    populateCombo(steppedNoiseShapeCombo_, kSteppedNoiseShapes);
    connect(steppedNoiseShapeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setSteppedNoiseShape(static_cast<SteppedNoiseShape>(steppedNoiseShapeCombo_->itemData(index).toInt()));
        updateVisibleFields();
        emitChanged();
    });
    steppedNoiseForm->addRow(tr("Shape:"), steppedNoiseShapeCombo_);
    stepCountSpinBox_ = new IntegerField(steppedNoiseGroup_);
    stepCountSpinBox_->setObjectName(QStringLiteral("stepCountSpinBox"));
    stepCountSpinBox_->setRange(2, 64);
    stepCountSpinBox_->setToolTip(tr("How many levels each cycle steps through."));
    connect(stepCountSpinBox_, &IntegerField::valueChanged, this, [this](int value) {
        wave_.setStepCount(value);
        emitChanged();
    });
    steppedNoiseForm->addRow(tr("Step Count:"), stepCountSpinBox_);
    root->addWidget(steppedNoiseGroup_);

    spatialGroup_ = new QGroupBox(tr("Spatial"), this);
    spatialGroup_->setObjectName(QStringLiteral("spatialGroup"));
    auto* spatialForm = new QFormLayout(spatialGroup_);
    spatialPatternCombo_ = new QComboBox(spatialGroup_);
    spatialPatternCombo_->setObjectName(QStringLiteral("spatialPatternCombo"));
    populateCombo(spatialPatternCombo_, kSpatialPatterns);
    connect(spatialPatternCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setSpatialPattern(static_cast<SpatialPattern>(spatialPatternCombo_->itemData(index).toInt()));
        updateVisibleFields();
        emitChanged();
    });
    spatialForm->addRow(tr("Pattern:"), spatialPatternCombo_);
    spatialCenterXSpinBox_ = makeWideSpinBox(spatialGroup_, QStringLiteral("spatialCenterXSpinBox"));
    spatialCenterXSpinBox_->setToolTip(tr("Where the ripples start on the time axis, in seconds."));
    spatialCenterXSpinBox_->setSliderRange(0.0, kTimeSliderMaximumSeconds);
    connect(spatialCenterXSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setSpatialCenterX(value);
        emitChanged();
    });
    spatialForm->addRow(tr("Center X:"), spatialCenterXSpinBox_);
    spatialCenterYSpinBox_ = makeWideSpinBox(spatialGroup_, QStringLiteral("spatialCenterYSpinBox"));
    spatialCenterYSpinBox_->setToolTip(tr("Where the ripples start on the frequency axis, in bins."));
    spatialCenterYSpinBox_->setSliderRange(0.0, kFrequencySliderMaximumBins);
    connect(spatialCenterYSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setSpatialCenterY(value);
        emitChanged();
    });
    spatialForm->addRow(tr("Center Y:"), spatialCenterYSpinBox_);
    domainWarpStrengthSpinBox_ = makeWideSpinBox(spatialGroup_, QStringLiteral("domainWarpStrengthSpinBox"));
    domainWarpStrengthSpinBox_->setToolTip(tr("How far the noise bends its own coordinates."));
    domainWarpStrengthSpinBox_->setSliderRange(0.0, 50.0);
    connect(domainWarpStrengthSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setDomainWarpStrength(value);
        emitChanged();
    });
    spatialForm->addRow(tr("Domain Warp Strength:"), domainWarpStrengthSpinBox_);
    root->addWidget(spatialGroup_);

    fractalGroup_ = new QGroupBox(tr("Fractal"), this);
    fractalGroup_->setObjectName(QStringLiteral("fractalGroup"));
    auto* fractalForm = new QFormLayout(fractalGroup_);
    fractalRoughnessSpinBox_ = new NumericField(fractalGroup_);
    fractalRoughnessSpinBox_->setObjectName(QStringLiteral("fractalRoughnessSpinBox"));
    fractalRoughnessSpinBox_->setRange(0.0, 1.0);
    fractalRoughnessSpinBox_->setSingleStep(0.05);
    fractalRoughnessSpinBox_->setDecimals(2);
    connect(fractalRoughnessSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setFractalRoughness(value);
        emitChanged();
    });
    fractalForm->addRow(tr("Roughness:"), fractalRoughnessSpinBox_);
    fractalIterationsSpinBox_ = new IntegerField(fractalGroup_);
    fractalIterationsSpinBox_->setObjectName(QStringLiteral("fractalIterationsSpinBox"));
    fractalIterationsSpinBox_->setRange(0, 16);
    connect(fractalIterationsSpinBox_, &IntegerField::valueChanged, this, [this](int value) {
        wave_.setFractalIterations(value);
        emitChanged();
    });
    fractalForm->addRow(tr("Iterations:"), fractalIterationsSpinBox_);
    root->addWidget(fractalGroup_);

    drawnGroup_ = new QGroupBox(tr("Drawn"), this);
    drawnGroup_->setObjectName(QStringLiteral("drawnGroup"));
    auto* drawnLayout = new QVBoxLayout(drawnGroup_);
    drawnStatusLabel_ = new QLabel(drawnGroup_);
    drawnStatusLabel_->setObjectName(QStringLiteral("drawnStatusLabel"));
    drawnStatusLabel_->setWordWrap(true);
    drawnLayout->addWidget(drawnStatusLabel_);
    root->addWidget(drawnGroup_);

    stepGridGroup_ = new QGroupBox(tr("Step Grid"), this);
    stepGridGroup_->setObjectName(QStringLiteral("stepGridGroup"));
    auto* stepGridLayout = new QVBoxLayout(stepGridGroup_);

    auto* stepGridForm = new QFormLayout();
    stepGridCountSpinBox_ = new IntegerField(stepGridGroup_);
    stepGridCountSpinBox_->setObjectName(QStringLiteral("stepGridCountSpinBox"));
    stepGridCountSpinBox_->setRange(1, 32);
    connect(stepGridCountSpinBox_, &IntegerField::valueChanged, this, [this](int count) {
        rebuildStepGridValueRows(static_cast<std::size_t>(count));
        wave_.setStepGridValues(currentStepGridValues());
        emitChanged();
    });
    stepGridForm->addRow(tr("Steps:"), stepGridCountSpinBox_);
    stepGridLayout->addLayout(stepGridForm);

    stepGridLayout->addWidget(new QLabel(tr("Step Values:"), stepGridGroup_));
    stepGridValuesLayout_ = new QVBoxLayout();
    stepGridLayout->addLayout(stepGridValuesLayout_);
    root->addWidget(stepGridGroup_);

    continuousGroup_ = new QGroupBox(tr("Continuous"), this);
    continuousGroup_->setObjectName(QStringLiteral("continuousGroup"));
    auto* continuousLayout = new QVBoxLayout(continuousGroup_);

    // The live shape preview sits above the three dial/spin-box rows - see
    // updateContinuousPreview()'s own docs for how it's computed.
    continuousPreviewWidget_ = new WaveformPreviewWidget(continuousGroup_);
    continuousPreviewWidget_->setObjectName(QStringLiteral("continuousPreviewWidget"));
    continuousLayout->addWidget(continuousPreviewWidget_);

    auto* continuousForm = new QFormLayout();

    continuousShapeSpinBox_ = new NumericField(continuousGroup_);
    continuousShapeSpinBox_->setObjectName(QStringLiteral("continuousShapeSpinBox"));
    continuousShapeSpinBox_->setRange(0.0, 1.0);
    continuousShapeSpinBox_->setSingleStep(0.05);
    continuousShapeSpinBox_->setDecimals(2);
    continuousShapeSpinBox_->setToolTip(tr("Sweeps from a clean sine cycle (0) toward fractal noise (1)."));
    continuousShapeDial_ = new RotaryDialWidget(continuousGroup_);
    continuousShapeDial_->setObjectName(QStringLiteral("continuousShapeDial"));
    connect(continuousShapeSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setContinuousShape(value);
        const QSignalBlocker blocker(continuousShapeDial_);
        continuousShapeDial_->setValue(value);
        emitChanged();
    });
    connect(continuousShapeDial_, &RotaryDialWidget::valueChanged, this, [this](double value) {
        wave_.setContinuousShape(value);
        const QSignalBlocker blocker(continuousShapeSpinBox_);
        continuousShapeSpinBox_->setValue(value);
        emitChanged();
    });
    continuousForm->addRow(tr("Shape:"), makeDialFieldRow(continuousShapeDial_, continuousShapeSpinBox_));

    continuousSkewSpinBox_ = new NumericField(continuousGroup_);
    continuousSkewSpinBox_->setObjectName(QStringLiteral("continuousSkewSpinBox"));
    continuousSkewSpinBox_->setRange(0.0, 1.0);
    continuousSkewSpinBox_->setSingleStep(0.05);
    continuousSkewSpinBox_->setDecimals(2);
    continuousSkewSpinBox_->setToolTip(tr("Biases the sine component earlier or later in its cycle - 0.5 is no bias."));
    continuousSkewDial_ = new RotaryDialWidget(continuousGroup_);
    continuousSkewDial_->setObjectName(QStringLiteral("continuousSkewDial"));
    connect(continuousSkewSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setContinuousSkew(value);
        const QSignalBlocker blocker(continuousSkewDial_);
        continuousSkewDial_->setValue(value);
        emitChanged();
    });
    connect(continuousSkewDial_, &RotaryDialWidget::valueChanged, this, [this](double value) {
        wave_.setContinuousSkew(value);
        const QSignalBlocker blocker(continuousSkewSpinBox_);
        continuousSkewSpinBox_->setValue(value);
        emitChanged();
    });
    continuousForm->addRow(tr("Skew:"), makeDialFieldRow(continuousSkewDial_, continuousSkewSpinBox_));

    continuousCharacterSpinBox_ = new NumericField(continuousGroup_);
    continuousCharacterSpinBox_->setObjectName(QStringLiteral("continuousCharacterSpinBox"));
    continuousCharacterSpinBox_->setRange(0.0, 1.0);
    continuousCharacterSpinBox_->setSingleStep(0.05);
    continuousCharacterSpinBox_->setDecimals(2);
    continuousCharacterSpinBox_->setToolTip(tr("Layers fine turbulence on top, independent of Shape."));
    continuousCharacterDial_ = new RotaryDialWidget(continuousGroup_);
    continuousCharacterDial_->setObjectName(QStringLiteral("continuousCharacterDial"));
    connect(continuousCharacterSpinBox_, &NumericField::valueChanged, this, [this](double value) {
        wave_.setContinuousCharacter(value);
        const QSignalBlocker blocker(continuousCharacterDial_);
        continuousCharacterDial_->setValue(value);
        emitChanged();
    });
    connect(continuousCharacterDial_, &RotaryDialWidget::valueChanged, this, [this](double value) {
        wave_.setContinuousCharacter(value);
        const QSignalBlocker blocker(continuousCharacterSpinBox_);
        continuousCharacterSpinBox_->setValue(value);
        emitChanged();
    });
    continuousForm->addRow(tr("Character:"), makeDialFieldRow(continuousCharacterDial_, continuousCharacterSpinBox_));

    continuousLayout->addLayout(continuousForm);

    root->addWidget(continuousGroup_);

    // --- Resonance's own group - v0.Y.59.1 Installment E -------------------
    resonanceGroup_ = new QGroupBox(tr("Resonance"), this);
    resonanceGroup_->setObjectName(QStringLiteral("resonanceGroup"));
    auto* resonantForm = new QFormLayout(resonanceGroup_);

    resonanceProfileCombo_ = new QComboBox(resonanceGroup_);
    resonanceProfileCombo_->setObjectName(QStringLiteral("resonanceProfileCombo"));
    resonanceProfileCombo_->addItem(tr("(none captured yet)"));
    connect(resonanceProfileCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QVariant data = resonanceProfileCombo_->itemData(index);
        if (!data.isValid()) {
            return;  // The "(none captured yet)" placeholder.
        }
        const auto id = static_cast<sound_mind::core::ResonanceProfileId>(data.toULongLong());
        const auto it = std::find_if(availableResonantProfiles_.begin(), availableResonantProfiles_.end(),
                                      [id](const sound_mind::core::NamedResonanceProfile& named) {
                                          return named.id == id;
                                      });
        if (it == availableResonantProfiles_.end()) {
            return;
        }
        wave_.setResonanceSpectrum(id, it->spectrum);
        emitChanged();
    });
    resonantForm->addRow(tr("Resonance Profile:"), resonanceProfileCombo_);

    root->addWidget(resonanceGroup_);

    root->addWidget(axisGroup_);
    root->addWidget(randomnessGroup_);
    root->addWidget(advancedToggle_);
    root->addWidget(advancedContainer_);
    root->addStretch();

    // Seeded from wave_'s own default stepGridValues() (present regardless
    // of its own default type) - the same "build the Instrument group's
    // rows from config_'s own defaults even while Procedural is selected"
    // reasoning ToolConfigurationPanel's own constructor already uses, so
    // switching to StepGrid for the first time shows a real, already-
    // populated sequence rather than an empty one.
    rebuildStepGridValueRows(wave_.stepGridValues().size());
    {
        const QSignalBlocker blocker(stepGridCountSpinBox_);
        stepGridCountSpinBox_->setValue(static_cast<int>(stepGridValueSpinBoxes_.size()));
    }
    for (std::size_t i = 0; i < stepGridValueSpinBoxes_.size() && i < wave_.stepGridValues().size(); ++i) {
        const QSignalBlocker blocker(stepGridValueSpinBoxes_[i]);
        stepGridValueSpinBoxes_[i]->setValue(wave_.stepGridValues()[i]);
    }

    // Seeded from wave_'s own defaults directly, the same reasoning as the
    // step-grid rows just above - NumericField's own built-in default
    // (0.0) happens to already match continuousShape()'s/
    // continuousCharacter()'s own defaults, but not continuousSkew()'s own
    // (0.5, a genuine neutral-bias position, not 0), so this can't be
    // skipped for just one of the three.
    {
        const QSignalBlocker shapeBlocker(continuousShapeSpinBox_);
        continuousShapeSpinBox_->setValue(wave_.continuousShape());
        continuousShapeDial_->setValue(wave_.continuousShape());
        const QSignalBlocker skewBlocker(continuousSkewSpinBox_);
        continuousSkewSpinBox_->setValue(wave_.continuousSkew());
        continuousSkewDial_->setValue(wave_.continuousSkew());
        const QSignalBlocker characterBlocker(continuousCharacterSpinBox_);
        continuousCharacterSpinBox_->setValue(wave_.continuousCharacter());
        continuousCharacterDial_->setValue(wave_.continuousCharacter());
    }

    updateVisibleFields();
    updateContinuousPreview();
}

void MindWaveEditor::emitChanged() {
    updateContinuousPreview();
    emit mindWaveChanged(wave_);
}

MindWaveEditor::FieldRelevance MindWaveEditor::relevantFields(const sound_mind::core::MindWave& wave) {
    using sound_mind::core::NoiseColor;
    FieldRelevance r;
    const GeneratorType type = wave.type();
    r.axis = type != GeneratorType::Spatial;
    r.reversed = type != GeneratorType::Spatial;  // Spatial reads both axes directly.
    switch (type) {
        case GeneratorType::Periodic:
            r.period = r.phase = true;
            r.dutyCycle = wave.periodicWaveform() == PeriodicWaveform::Pulse;
            break;
        case GeneratorType::Envelope: {
            const EnvelopeShape shape = wave.envelopeShape();
            const bool decays = shape == EnvelopeShape::ExponentialDecay || shape == EnvelopeShape::DecayingOscillation;
            r.decayRate = decays;
            r.envelopeSteepness = !decays;
            r.period = r.phase = shape == EnvelopeShape::DecayingOscillation;
            break;
        }
        case GeneratorType::SteppedNoise:
            if (wave.steppedNoiseShape() == SteppedNoiseShape::Stepped) {
                r.period = r.phase = r.stepCount = true;
            } else {
                r.seed = r.noiseScale = true;
                r.noiseDetail = wave.steppedNoiseShape() == SteppedNoiseShape::FractalNoise;
            }
            break;
        case GeneratorType::Spatial:
            switch (wave.spatialPattern()) {
                case SpatialPattern::Ripples:
                    r.period = r.phase = r.spatialCenter = true;
                    break;
                case SpatialPattern::Checkerboard:
                    r.period = true;
                    break;
                case SpatialPattern::Cellular:
                    r.seed = r.noiseScale = true;
                    break;
                case SpatialPattern::DomainWarpedNoise:
                    r.seed = r.noiseScale = r.noiseDetail = r.domainWarpStrength = true;
                    break;
            }
            break;
        case GeneratorType::Fractal:
            r.period = r.seed = true;
            break;
        case GeneratorType::Continuous:
            r.period = r.phase = r.seed = r.noiseScale = r.noiseDetail = true;
            break;
        case GeneratorType::Noise:
            r.seed = true;
            r.noiseSlope = wave.noiseColor() == NoiseColor::Custom;
            break;
        case GeneratorType::Drawn:
        case GeneratorType::StepGrid:
        case GeneratorType::Resonance:
            r.period = r.phase = true;
            break;
    }
    return r;
}

namespace {

/// @brief Shows or hides `field`'s whole row (its label too) in the form it
///        sits in.
void setRowShown(QWidget* field, bool shown) {
    auto* form = field->parentWidget() != nullptr ? qobject_cast<QFormLayout*>(field->parentWidget()->layout())
                                                  : nullptr;
    if (form != nullptr && form->indexOf(field) >= 0) {
        form->setRowVisible(field, shown);
    } else {
        field->setVisible(shown);
    }
}

}  // namespace

void MindWaveEditor::updateVisibleFields() {
    const GeneratorType type = wave_.type();
    periodicGroup_->setVisible(type == GeneratorType::Periodic);
    envelopeGroup_->setVisible(type == GeneratorType::Envelope);
    steppedNoiseGroup_->setVisible(type == GeneratorType::SteppedNoise);
    noiseGroup_->setVisible(type == GeneratorType::Noise);
    spatialGroup_->setVisible(type == GeneratorType::Spatial);
    fractalGroup_->setVisible(type == GeneratorType::Fractal);
    drawnGroup_->setVisible(type == GeneratorType::Drawn);
    stepGridGroup_->setVisible(type == GeneratorType::StepGrid);
    continuousGroup_->setVisible(type == GeneratorType::Continuous);
    resonanceGroup_->setVisible(type == GeneratorType::Resonance);

    const FieldRelevance r = relevantFields(wave_);
    setRowShown(axisCombo_, r.axis);
    setRowShown(periodSpinBox_, r.period);
    axisGroup_->setVisible(r.axis || r.period);
    setRowShown(seedSpinBox_, r.seed);
    setRowShown(noiseScaleSpinBox_, r.noiseScale);
    randomnessGroup_->setVisible(r.seed || r.noiseScale);
    setRowShown(phaseSpinBox_, r.phase);
    setRowShown(reversedCheckBox_, r.reversed);
    setRowShown(noiseOctavesSpinBox_, r.noiseDetail);
    setRowShown(noisePersistenceSpinBox_, r.noiseDetail);
    setRowShown(dutyCycleSpinBox_, r.dutyCycle);
    setRowShown(envelopeSteepnessSpinBox_, r.envelopeSteepness);
    setRowShown(decayRateSpinBox_, r.decayRate);
    setRowShown(stepCountSpinBox_, r.stepCount);
    setRowShown(spatialCenterXSpinBox_, r.spatialCenter);
    setRowShown(spatialCenterYSpinBox_, r.spatialCenter);
    setRowShown(domainWarpStrengthSpinBox_, r.domainWarpStrength);
    setRowShown(noiseSlopeSpinBox_, r.noiseSlope);

    // The sliders cover a useful range in the axis' own units; typing can
    // still go beyond it.
    const bool frequencyAxis = wave_.axis() == MindWaveAxis::Frequency && type != GeneratorType::Spatial;
    periodSpinBox_->setSliderRange(frequencyAxis ? 1.0 : 0.01,
                                   frequencyAxis ? kFrequencySliderMaximumBins : kTimeSliderMaximumSeconds);
    envelopeCenterSpinBox_->setSliderRange(0.0, frequencyAxis ? kFrequencySliderMaximumBins : kTimeSliderMaximumSeconds);
}

void MindWaveEditor::updateContinuousPreview() {
    if (wave_.type() != GeneratorType::Continuous) {
        return;
    }
    const sound_mind::codec::StreamCodecConfig config{};
    const auto samples = sound_mind::core::reduceMindWaveToSignal(
        wave_, config, kContinuousPreviewSampleCount, wave_.period(), sound_mind::core::ReduceMode::Integrate);
    continuousPreviewWidget_->setSamples(samples);
}

void MindWaveEditor::setMindWave(const sound_mind::core::MindWave& wave) {
    wave_ = wave;

    // QSignalBlocker on every control - see FilterConfigurationPanel::
    // setFilterConfiguration()'s own docs for why: only the *display*
    // should change here, not re-trigger each control's own change
    // handler.
    const QSignalBlocker generatorTypeBlocker(generatorTypeCombo_);
    const QSignalBlocker axisBlocker(axisCombo_);
    const QSignalBlocker periodBlocker(periodSpinBox_);
    const QSignalBlocker phaseBlocker(phaseSpinBox_);
    const QSignalBlocker reversedBlocker(reversedCheckBox_);
    const QSignalBlocker invertedBlocker(invertedCheckBox_);
    const QSignalBlocker seedBlocker(seedSpinBox_);
    const QSignalBlocker noiseScaleBlocker(noiseScaleSpinBox_);
    const QSignalBlocker noiseColorBlocker(noiseColorCombo_);
    const QSignalBlocker noiseSlopeBlocker(noiseSlopeSpinBox_);
    const QSignalBlocker noiseCellWidthBlocker(noiseCellWidthSpinBox_);
    const QSignalBlocker noiseCellHeightBlocker(noiseCellHeightSpinBox_);
    const QSignalBlocker noiseOctavesBlocker(noiseOctavesSpinBox_);
    const QSignalBlocker noisePersistenceBlocker(noisePersistenceSpinBox_);
    const QSignalBlocker periodicWaveformBlocker(periodicWaveformCombo_);
    const QSignalBlocker dutyCycleBlocker(dutyCycleSpinBox_);
    const QSignalBlocker envelopeShapeBlocker(envelopeShapeCombo_);
    const QSignalBlocker envelopeCenterBlocker(envelopeCenterSpinBox_);
    const QSignalBlocker envelopeSteepnessBlocker(envelopeSteepnessSpinBox_);
    const QSignalBlocker decayRateBlocker(decayRateSpinBox_);
    const QSignalBlocker steppedNoiseShapeBlocker(steppedNoiseShapeCombo_);
    const QSignalBlocker stepCountBlocker(stepCountSpinBox_);
    const QSignalBlocker spatialPatternBlocker(spatialPatternCombo_);
    const QSignalBlocker spatialCenterXBlocker(spatialCenterXSpinBox_);
    const QSignalBlocker spatialCenterYBlocker(spatialCenterYSpinBox_);
    const QSignalBlocker domainWarpStrengthBlocker(domainWarpStrengthSpinBox_);
    const QSignalBlocker fractalRoughnessBlocker(fractalRoughnessSpinBox_);
    const QSignalBlocker fractalIterationsBlocker(fractalIterationsSpinBox_);
    const QSignalBlocker continuousShapeBlocker(continuousShapeSpinBox_);
    const QSignalBlocker continuousSkewBlocker(continuousSkewSpinBox_);
    const QSignalBlocker continuousCharacterBlocker(continuousCharacterSpinBox_);
    const QSignalBlocker resonantProfileBlocker(resonanceProfileCombo_);

    selectComboValue(generatorTypeCombo_, wave_.type());
    selectComboValue(axisCombo_, wave_.axis());
    periodSpinBox_->setValue(wave_.period());
    phaseSpinBox_->setValue(wave_.phaseRadians());
    reversedCheckBox_->setChecked(wave_.reversed());
    invertedCheckBox_->setChecked(wave_.inverted());
    seedSpinBox_->setValue(static_cast<int>(std::min<std::uint32_t>(
        wave_.seed(), static_cast<std::uint32_t>(std::numeric_limits<int>::max()))));
    noiseScaleSpinBox_->setValue(wave_.noiseScale());
    noiseColorCombo_->setCurrentIndex(noiseColorCombo_->findData(static_cast<int>(wave_.noiseColor())));
    noiseSlopeSpinBox_->setValue(wave_.noiseSlope());
    noiseSlopeSpinBox_->setEnabled(wave_.noiseColor() == sound_mind::core::NoiseColor::Custom);
    noiseCellWidthSpinBox_->setValue(wave_.noiseCellWidth());
    noiseCellHeightSpinBox_->setValue(wave_.noiseCellHeight());
    noiseOctavesSpinBox_->setValue(wave_.noiseOctaves());
    noisePersistenceSpinBox_->setValue(wave_.noisePersistence());
    selectComboValue(periodicWaveformCombo_, wave_.periodicWaveform());
    dutyCycleSpinBox_->setValue(wave_.dutyCycle());
    selectComboValue(envelopeShapeCombo_, wave_.envelopeShape());
    envelopeCenterSpinBox_->setValue(wave_.envelopeCenter());
    envelopeSteepnessSpinBox_->setValue(wave_.envelopeSteepness());
    decayRateSpinBox_->setValue(wave_.decayRate());
    selectComboValue(steppedNoiseShapeCombo_, wave_.steppedNoiseShape());
    stepCountSpinBox_->setValue(wave_.stepCount());
    selectComboValue(spatialPatternCombo_, wave_.spatialPattern());
    spatialCenterXSpinBox_->setValue(wave_.spatialCenterX());
    spatialCenterYSpinBox_->setValue(wave_.spatialCenterY());
    domainWarpStrengthSpinBox_->setValue(wave_.domainWarpStrength());
    fractalRoughnessSpinBox_->setValue(wave_.fractalRoughness());
    fractalIterationsSpinBox_->setValue(wave_.fractalIterations());
    continuousShapeSpinBox_->setValue(wave_.continuousShape());
    continuousShapeDial_->setValue(wave_.continuousShape());
    continuousSkewSpinBox_->setValue(wave_.continuousSkew());
    continuousSkewDial_->setValue(wave_.continuousSkew());
    continuousCharacterSpinBox_->setValue(wave_.continuousCharacter());
    continuousCharacterDial_->setValue(wave_.continuousCharacter());

    if (wave_.sourceResonanceProfileId().has_value()) {
        const int index = resonanceProfileCombo_->findData(
            QVariant::fromValue(static_cast<qulonglong>(*wave_.sourceResonanceProfileId())));
        resonanceProfileCombo_->setCurrentIndex(index >= 0 ? index : 0);
    } else {
        resonanceProfileCombo_->setCurrentIndex(0);
    }

    const int nodeCount = static_cast<int>(wave_.drawnPath().nodes().size());
    drawnStatusLabel_->setText(
        nodeCount >= 2
            ? tr("%1-node shape captured. Draw a new stroke, Pick it, and use Edit -> Use Picked Path as MindWave "
                 "Shape to replace it.")
                  .arg(nodeCount)
            : tr("No shape captured yet. Draw a stroke, switch to Pick and select it, then use Edit -> Use Picked "
                 "Path as MindWave Shape."));

    rebuildStepGridValueRows(wave_.stepGridValues().size());
    {
        const QSignalBlocker blocker(stepGridCountSpinBox_);
        stepGridCountSpinBox_->setValue(static_cast<int>(stepGridValueSpinBoxes_.size()));
    }
    for (std::size_t i = 0; i < stepGridValueSpinBoxes_.size(); ++i) {
        const QSignalBlocker blocker(stepGridValueSpinBoxes_[i]);
        stepGridValueSpinBoxes_[i]->setValue(wave_.stepGridValues()[i]);
    }

    updateVisibleFields();
    updateContinuousPreview();
}

void MindWaveEditor::setAvailableResonanceProfiles(
    const std::vector<sound_mind::core::NamedResonanceProfile>& profiles) {
    availableResonantProfiles_ = profiles;

    const QVariant previousData = resonanceProfileCombo_->currentData();
    const QSignalBlocker blocker(resonanceProfileCombo_);
    resonanceProfileCombo_->clear();
    if (profiles.empty()) {
        resonanceProfileCombo_->addItem(tr("(none captured yet)"));
        return;
    }
    for (const auto& named : profiles) {
        resonanceProfileCombo_->addItem(QString::fromStdString(named.name),
                                        QVariant::fromValue(static_cast<qulonglong>(named.id)));
    }
    const int index = resonanceProfileCombo_->findData(previousData);
    resonanceProfileCombo_->setCurrentIndex(index >= 0 ? index : 0);
}

void MindWaveEditor::rebuildStepGridValueRows(std::size_t count) {
    // Same immediate-delete, no deleteLater() reasoning
    // ToolConfigurationPanel::rebuildHarmonicStrengthRows()'s own docs
    // give - shrinking is only ever triggered by stepGridCountSpinBox_'s
    // own change or a fresh setMindWave() sync, never by one of these very
    // spin boxes' own signal still on the call stack.
    while (stepGridValueSpinBoxes_.size() > count) {
        NumericField* spinBox = stepGridValueSpinBoxes_.back();
        stepGridValueSpinBoxes_.pop_back();
        stepGridValuesLayout_->removeWidget(spinBox);
        delete spinBox;
    }
    while (stepGridValueSpinBoxes_.size() < count) {
        const int stepNumber = static_cast<int>(stepGridValueSpinBoxes_.size()) + 1;
        auto* spinBox = new NumericField(stepGridGroup_);
        spinBox->setObjectName(QStringLiteral("stepGridValueSpinBox%1").arg(stepNumber));
        spinBox->setRange(0.0, 1.0);
        spinBox->setSingleStep(0.05);
        spinBox->setDecimals(3);
        spinBox->setValue(0.5);
        spinBox->setPrefix(tr("Step %1: ").arg(stepNumber));
        connect(spinBox, &NumericField::valueChanged, this, [this](double) {
            wave_.setStepGridValues(currentStepGridValues());
            emitChanged();
        });
        stepGridValuesLayout_->addWidget(spinBox);
        stepGridValueSpinBoxes_.push_back(spinBox);
    }
}

std::vector<double> MindWaveEditor::currentStepGridValues() const {
    std::vector<double> values;
    values.reserve(stepGridValueSpinBoxes_.size());
    for (const NumericField* spinBox : stepGridValueSpinBoxes_) {
        values.push_back(spinBox->value());
    }
    return values;
}

}  // namespace sound_mind::studio
