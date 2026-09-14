#include "sound_mind/studio/mind_wave_editor.h"

#include <array>
#include <limits>
#include <utility>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

namespace sound_mind::studio {

namespace {

using sound_mind::core::EnvelopeShape;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::SpatialPattern;
using sound_mind::core::SteppedNoiseShape;

constexpr std::array<std::pair<GeneratorType, const char*>, 5> kGeneratorTypes{{
    {GeneratorType::Periodic, "Periodic"},
    {GeneratorType::Envelope, "Envelope"},
    {GeneratorType::SteppedNoise, "Stepped/Noise"},
    {GeneratorType::Spatial, "Spatial"},
    {GeneratorType::Fractal, "Fractal"},
}};

constexpr std::array<std::pair<MindWaveAxis, const char*>, 2> kAxes{{
    {MindWaveAxis::Time, "Time"},
    {MindWaveAxis::Frequency, "Frequency"},
}};

constexpr std::array<std::pair<PeriodicWaveform, const char*>, 5> kPeriodicWaveforms{{
    {PeriodicWaveform::Sine, "Sine"},
    {PeriodicWaveform::Triangle, "Triangle"},
    {PeriodicWaveform::Square, "Square"},
    {PeriodicWaveform::Sawtooth, "Sawtooth"},
    {PeriodicWaveform::Pulse, "Pulse"},
}};

constexpr std::array<std::pair<EnvelopeShape, const char*>, 3> kEnvelopeShapes{{
    {EnvelopeShape::ExponentialDecay, "Exponential Decay"},
    {EnvelopeShape::DecayingOscillation, "Decaying Oscillation"},
    {EnvelopeShape::SCurve, "S-Curve"},
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

/// @brief A wide-range, general-purpose double spin box - most of this
/// editor's own fields have no natural bound (a period, a phase, a noise
/// scale...), unlike FilterConfigurationPanel's own tightly-ranged fields
/// (a dB value, an opacity). Matches this editor's own "bare-bones" scope:
/// plain numeric entry, not a validated/clamped control.
QDoubleSpinBox* makeWideSpinBox(QWidget* parent, const QString& objectName, double minimum = -1'000'000.0,
                                  double maximum = 1'000'000.0, int decimals = 3) {
    auto* spinBox = new QDoubleSpinBox(parent);
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
        updateVisibleGroup();
        emitChanged();
    });
    topForm->addRow(tr("Generator Type:"), generatorTypeCombo_);

    axisCombo_ = new QComboBox(this);
    axisCombo_->setObjectName(QStringLiteral("axisCombo"));
    populateCombo(axisCombo_, kAxes);
    connect(axisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setAxis(static_cast<MindWaveAxis>(axisCombo_->itemData(index).toInt()));
        emitChanged();
    });
    topForm->addRow(tr("Axis:"), axisCombo_);
    axisLabel_ = qobject_cast<QLabel*>(topForm->labelForField(axisCombo_));

    periodSpinBox_ = makeWideSpinBox(this, QStringLiteral("periodSpinBox"), 0.000001, 1'000'000.0);
    periodSpinBox_->setToolTip(
        tr("Cycle length - seconds (Time axis) or bins (Frequency axis). Used by Periodic, Envelope's Decaying "
           "Oscillation, Stepped/Noise's Stepped, Spatial's Ripples/Checkerboard, and Fractal."));
    connect(periodSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setPeriod(value);
        emitChanged();
    });
    topForm->addRow(tr("Period:"), periodSpinBox_);

    phaseSpinBox_ = makeWideSpinBox(this, QStringLiteral("phaseSpinBox"));
    phaseSpinBox_->setToolTip(tr("Phase offset, in radians."));
    connect(phaseSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setPhaseRadians(value);
        emitChanged();
    });
    topForm->addRow(tr("Phase (radians):"), phaseSpinBox_);

    seedSpinBox_ = new QSpinBox(this);
    seedSpinBox_->setObjectName(QStringLiteral("seedSpinBox"));
    // MindWave::seed() is a full uint32_t; QSpinBox is int-only - a UI-
    // level limit (matching LayersPanel's own translationSpinBox docs for
    // the same int-vs-wider-type shape), not a Core-side restriction.
    seedSpinBox_->setRange(0, std::numeric_limits<int>::max());
    seedSpinBox_->setToolTip(tr("Seed for every noise-based shape (Gaussian Noise, Fractal Noise, Cellular, "
                                 "Domain-Warped Noise, and Fractal)."));
    connect(seedSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        wave_.setSeed(static_cast<std::uint32_t>(value));
        emitChanged();
    });
    topForm->addRow(tr("Seed:"), seedSpinBox_);

    noiseScaleSpinBox_ = makeWideSpinBox(this, QStringLiteral("noiseScaleSpinBox"), 0.000001, 1'000'000.0);
    noiseScaleSpinBox_->setToolTip(tr("Lattice/cell size for every noise-based shape."));
    connect(noiseScaleSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setNoiseScale(value);
        emitChanged();
    });
    topForm->addRow(tr("Noise Scale:"), noiseScaleSpinBox_);

    noiseOctavesSpinBox_ = new QSpinBox(this);
    noiseOctavesSpinBox_->setObjectName(QStringLiteral("noiseOctavesSpinBox"));
    noiseOctavesSpinBox_->setRange(1, 16);
    noiseOctavesSpinBox_->setToolTip(tr("Octave count for Fractal Noise and Domain-Warped Noise."));
    connect(noiseOctavesSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        wave_.setNoiseOctaves(value);
        emitChanged();
    });
    topForm->addRow(tr("Noise Octaves:"), noiseOctavesSpinBox_);

    noisePersistenceSpinBox_ = new QDoubleSpinBox(this);
    noisePersistenceSpinBox_->setObjectName(QStringLiteral("noisePersistenceSpinBox"));
    noisePersistenceSpinBox_->setRange(0.0, 1.0);
    noisePersistenceSpinBox_->setSingleStep(0.05);
    noisePersistenceSpinBox_->setDecimals(2);
    noisePersistenceSpinBox_->setToolTip(tr("Octave falloff for Fractal Noise and Domain-Warped Noise."));
    connect(noisePersistenceSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setNoisePersistence(value);
        emitChanged();
    });
    topForm->addRow(tr("Noise Persistence:"), noisePersistenceSpinBox_);

    root->addLayout(topForm);

    periodicGroup_ = new QGroupBox(tr("Periodic"), this);
    periodicGroup_->setObjectName(QStringLiteral("periodicGroup"));
    auto* periodicForm = new QFormLayout(periodicGroup_);
    periodicWaveformCombo_ = new QComboBox(periodicGroup_);
    periodicWaveformCombo_->setObjectName(QStringLiteral("periodicWaveformCombo"));
    populateCombo(periodicWaveformCombo_, kPeriodicWaveforms);
    connect(periodicWaveformCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setPeriodicWaveform(static_cast<PeriodicWaveform>(periodicWaveformCombo_->itemData(index).toInt()));
        emitChanged();
    });
    periodicForm->addRow(tr("Waveform:"), periodicWaveformCombo_);
    dutyCycleSpinBox_ = new QDoubleSpinBox(periodicGroup_);
    dutyCycleSpinBox_->setObjectName(QStringLiteral("dutyCycleSpinBox"));
    dutyCycleSpinBox_->setRange(0.0, 1.0);
    dutyCycleSpinBox_->setSingleStep(0.05);
    dutyCycleSpinBox_->setDecimals(2);
    dutyCycleSpinBox_->setToolTip(tr("Pulse only - the default 0.5 makes Pulse identical to Square."));
    connect(dutyCycleSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
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
        emitChanged();
    });
    envelopeForm->addRow(tr("Shape:"), envelopeShapeCombo_);
    envelopeCenterSpinBox_ = makeWideSpinBox(envelopeGroup_, QStringLiteral("envelopeCenterSpinBox"));
    connect(envelopeCenterSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setEnvelopeCenter(value);
        emitChanged();
    });
    envelopeForm->addRow(tr("Center:"), envelopeCenterSpinBox_);
    envelopeSteepnessSpinBox_ = makeWideSpinBox(envelopeGroup_, QStringLiteral("envelopeSteepnessSpinBox"));
    envelopeSteepnessSpinBox_->setToolTip(tr("S-Curve only."));
    connect(envelopeSteepnessSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setEnvelopeSteepness(value);
        emitChanged();
    });
    envelopeForm->addRow(tr("Steepness:"), envelopeSteepnessSpinBox_);
    decayRateSpinBox_ = makeWideSpinBox(envelopeGroup_, QStringLiteral("decayRateSpinBox"));
    decayRateSpinBox_->setToolTip(tr("Exponential Decay and Decaying Oscillation only."));
    connect(decayRateSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setDecayRate(value);
        emitChanged();
    });
    envelopeForm->addRow(tr("Decay Rate:"), decayRateSpinBox_);
    root->addWidget(envelopeGroup_);

    steppedNoiseGroup_ = new QGroupBox(tr("Stepped/Noise"), this);
    steppedNoiseGroup_->setObjectName(QStringLiteral("steppedNoiseGroup"));
    auto* steppedNoiseForm = new QFormLayout(steppedNoiseGroup_);
    steppedNoiseShapeCombo_ = new QComboBox(steppedNoiseGroup_);
    steppedNoiseShapeCombo_->setObjectName(QStringLiteral("steppedNoiseShapeCombo"));
    populateCombo(steppedNoiseShapeCombo_, kSteppedNoiseShapes);
    connect(steppedNoiseShapeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        wave_.setSteppedNoiseShape(static_cast<SteppedNoiseShape>(steppedNoiseShapeCombo_->itemData(index).toInt()));
        emitChanged();
    });
    steppedNoiseForm->addRow(tr("Shape:"), steppedNoiseShapeCombo_);
    stepCountSpinBox_ = new QSpinBox(steppedNoiseGroup_);
    stepCountSpinBox_->setObjectName(QStringLiteral("stepCountSpinBox"));
    stepCountSpinBox_->setRange(2, 64);
    stepCountSpinBox_->setToolTip(tr("Stepped only."));
    connect(stepCountSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
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
        emitChanged();
    });
    spatialForm->addRow(tr("Pattern:"), spatialPatternCombo_);
    spatialCenterXSpinBox_ = makeWideSpinBox(spatialGroup_, QStringLiteral("spatialCenterXSpinBox"));
    spatialCenterXSpinBox_->setToolTip(tr("Ripples only - time-axis component, in seconds."));
    connect(spatialCenterXSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setSpatialCenterX(value);
        emitChanged();
    });
    spatialForm->addRow(tr("Center X:"), spatialCenterXSpinBox_);
    spatialCenterYSpinBox_ = makeWideSpinBox(spatialGroup_, QStringLiteral("spatialCenterYSpinBox"));
    spatialCenterYSpinBox_->setToolTip(tr("Ripples only - frequency-axis component, in bins."));
    connect(spatialCenterYSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setSpatialCenterY(value);
        emitChanged();
    });
    spatialForm->addRow(tr("Center Y:"), spatialCenterYSpinBox_);
    domainWarpStrengthSpinBox_ = makeWideSpinBox(spatialGroup_, QStringLiteral("domainWarpStrengthSpinBox"));
    domainWarpStrengthSpinBox_->setToolTip(tr("Domain-Warped Noise only."));
    connect(domainWarpStrengthSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setDomainWarpStrength(value);
        emitChanged();
    });
    spatialForm->addRow(tr("Domain Warp Strength:"), domainWarpStrengthSpinBox_);
    root->addWidget(spatialGroup_);

    fractalGroup_ = new QGroupBox(tr("Fractal"), this);
    fractalGroup_->setObjectName(QStringLiteral("fractalGroup"));
    auto* fractalForm = new QFormLayout(fractalGroup_);
    fractalRoughnessSpinBox_ = new QDoubleSpinBox(fractalGroup_);
    fractalRoughnessSpinBox_->setObjectName(QStringLiteral("fractalRoughnessSpinBox"));
    fractalRoughnessSpinBox_->setRange(0.0, 1.0);
    fractalRoughnessSpinBox_->setSingleStep(0.05);
    fractalRoughnessSpinBox_->setDecimals(2);
    connect(fractalRoughnessSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        wave_.setFractalRoughness(value);
        emitChanged();
    });
    fractalForm->addRow(tr("Roughness:"), fractalRoughnessSpinBox_);
    fractalIterationsSpinBox_ = new QSpinBox(fractalGroup_);
    fractalIterationsSpinBox_->setObjectName(QStringLiteral("fractalIterationsSpinBox"));
    fractalIterationsSpinBox_->setRange(0, 16);
    connect(fractalIterationsSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        wave_.setFractalIterations(value);
        emitChanged();
    });
    fractalForm->addRow(tr("Iterations:"), fractalIterationsSpinBox_);
    root->addWidget(fractalGroup_);

    root->addStretch();

    updateVisibleGroup();
}

void MindWaveEditor::emitChanged() { emit mindWaveChanged(wave_); }

void MindWaveEditor::updateVisibleGroup() {
    const GeneratorType type = wave_.type();
    axisCombo_->setVisible(type != GeneratorType::Spatial);
    if (axisLabel_ != nullptr) {
        axisLabel_->setVisible(type != GeneratorType::Spatial);
    }
    periodicGroup_->setVisible(type == GeneratorType::Periodic);
    envelopeGroup_->setVisible(type == GeneratorType::Envelope);
    steppedNoiseGroup_->setVisible(type == GeneratorType::SteppedNoise);
    spatialGroup_->setVisible(type == GeneratorType::Spatial);
    fractalGroup_->setVisible(type == GeneratorType::Fractal);
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
    const QSignalBlocker seedBlocker(seedSpinBox_);
    const QSignalBlocker noiseScaleBlocker(noiseScaleSpinBox_);
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

    selectComboValue(generatorTypeCombo_, wave_.type());
    selectComboValue(axisCombo_, wave_.axis());
    periodSpinBox_->setValue(wave_.period());
    phaseSpinBox_->setValue(wave_.phaseRadians());
    seedSpinBox_->setValue(static_cast<int>(std::min<std::uint32_t>(
        wave_.seed(), static_cast<std::uint32_t>(std::numeric_limits<int>::max()))));
    noiseScaleSpinBox_->setValue(wave_.noiseScale());
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

    updateVisibleGroup();
}

}  // namespace sound_mind::studio
