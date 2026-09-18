#include "sound_mind/studio/filter_configuration_panel.h"

#include <array>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include "sound_mind/studio/tone_curve_editor.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::FilterType;
using sound_mind::core::GradientStop;
using sound_mind::core::MindWaveId;

/// @brief A `None`-plus-library MindWave-binding combo, styled to match
/// `LayersPanel`'s own per-row `opacityMindWaveCombo` - item population
/// happens separately, in `FilterConfigurationPanel::
/// rebuildMindWaveCombos()`.
QComboBox* makeMindWaveCombo(QWidget* parent, const QString& objectName) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName(objectName);
    combo->setToolTip(QObject::tr("Bind this parameter to a MindWave"));
    return combo;
}

/// @brief Wraps `spinBox` and `combo` side by side - every bindable
/// parameter's own row uses this instead of the spin box alone.
QHBoxLayout* makeBoundFieldRow(QWidget* spinBox, QWidget* combo) {
    auto* row = new QHBoxLayout();
    row->addWidget(spinBox);
    row->addWidget(combo);
    return row;
}

/// @brief Every `FilterType`, in `docs/sound-mind-design.md`'s own family
/// order (Blur & focus, then Noise & distortion, then Tonal, then
/// Spectral shaping) - the first six shipped in `v0.Y.28.1`, the eight
/// Noise & distortion types in `v0.Y.36.1` Installment A.
constexpr std::array<std::pair<FilterType, const char*>, 14> kSelectableFilterTypes{{
    {FilterType::UniformBlur, "Uniform Blur"},
    {FilterType::EdgePreservingBlur, "Edge-Preserving Blur"},
    {FilterType::DirectionalBlur, "Directional Blur"},
    {FilterType::Sharpen, "Sharpen"},
    {FilterType::SpeckleAdd, "Speckle Add"},
    {FilterType::SpeckleRemove, "Speckle Remove"},
    {FilterType::Denoise, "Denoise"},
    {FilterType::BitDepthCrush, "Bit-Depth Crush"},
    {FilterType::GranularNoise, "Granular Noise"},
    {FilterType::DynamicSpeckle, "Dynamic Speckle"},
    {FilterType::FeedbackDistortion, "Feedback Distortion"},
    {FilterType::SpectralWavefold, "Spectral Wavefold"},
    {FilterType::ToneCurve, "Tone Curve"},
    {FilterType::FrequencyAxisGradient, "Frequency-Axis Gradient"},
}};

/// @brief A `[-96, 0]` dB spin box, matching `silenceGradient()`'s own
/// floor and `color_mapping.cpp`'s own display range - the same range
/// every other dB-valued control in this codebase already uses.
QDoubleSpinBox* makeIntensitySpinBox(QWidget* parent, const QString& objectName) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(-96.0, 0.0);
    spinBox->setSingleStep(1.0);
    spinBox->setSuffix(QStringLiteral(" dB"));
    return spinBox;
}

/// @brief A `[0, 1]` opacity spin box.
QDoubleSpinBox* makeOpacitySpinBox(QWidget* parent, const QString& objectName) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(0.0, 1.0);
    spinBox->setSingleStep(0.05);
    spinBox->setDecimals(2);
    return spinBox;
}

}  // namespace

FilterConfigurationPanel::FilterConfigurationPanel(QWidget* parent)
    : QDockWidget(tr("Filter Configuration"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* typeForm = new QFormLayout();
    filterTypeCombo_ = new QComboBox(container);
    filterTypeCombo_->setObjectName(QStringLiteral("filterTypeCombo"));
    for (const auto& [type, name] : kSelectableFilterTypes) {
        filterTypeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(type)));
    }
    connect(filterTypeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        config_.setType(static_cast<FilterType>(filterTypeCombo_->itemData(index).toInt()));
        updateVisibleGroup();
        emitConfigChanged();
    });
    typeForm->addRow(tr("Filter Type:"), filterTypeCombo_);
    filterTypeLabel_ = qobject_cast<QLabel*>(typeForm->labelForField(filterTypeCombo_));
    root->addLayout(typeForm);

    auto* frequencyGroupContainer = new QWidget(container);
    frequencyGroupContainer->setObjectName(QStringLiteral("frequencyAxisGradientSection"));
    auto* frequencyLayout = new QVBoxLayout(frequencyGroupContainer);
    frequencyLayout->setContentsMargins(0, 0, 0, 0);
    frequencyAxisGradientSection_ = frequencyGroupContainer;

    frequencyGradientLabel_ = new QLabel(tr("Frequency-Axis Gradient"), frequencyGroupContainer);
    frequencyGradientLabel_->setObjectName(QStringLiteral("frequencyGradientLabel"));
    frequencyLayout->addWidget(frequencyGradientLabel_);

    auto* startGroup = new QGroupBox(tr("Start (t=0, lowest frequency)"), frequencyGroupContainer);
    auto* startForm = new QFormLayout(startGroup);
    startLeftIntensitySpinBox_ = makeIntensitySpinBox(startGroup, QStringLiteral("startLeftIntensitySpinBox"));
    startForm->addRow(tr("Left Intensity:"), startLeftIntensitySpinBox_);
    startLeftOpacitySpinBox_ = makeOpacitySpinBox(startGroup, QStringLiteral("startLeftOpacitySpinBox"));
    startForm->addRow(tr("Left Opacity:"), startLeftOpacitySpinBox_);
    startRightIntensitySpinBox_ = makeIntensitySpinBox(startGroup, QStringLiteral("startRightIntensitySpinBox"));
    startForm->addRow(tr("Right Intensity:"), startRightIntensitySpinBox_);
    startRightOpacitySpinBox_ = makeOpacitySpinBox(startGroup, QStringLiteral("startRightOpacitySpinBox"));
    startForm->addRow(tr("Right Opacity:"), startRightOpacitySpinBox_);
    frequencyLayout->addWidget(startGroup);

    auto* endGroup = new QGroupBox(tr("End (t=1, highest frequency)"), frequencyGroupContainer);
    auto* endForm = new QFormLayout(endGroup);
    endLeftIntensitySpinBox_ = makeIntensitySpinBox(endGroup, QStringLiteral("endLeftIntensitySpinBox"));
    endForm->addRow(tr("Left Intensity:"), endLeftIntensitySpinBox_);
    endLeftOpacitySpinBox_ = makeOpacitySpinBox(endGroup, QStringLiteral("endLeftOpacitySpinBox"));
    endForm->addRow(tr("Left Opacity:"), endLeftOpacitySpinBox_);
    endRightIntensitySpinBox_ = makeIntensitySpinBox(endGroup, QStringLiteral("endRightIntensitySpinBox"));
    endForm->addRow(tr("Right Intensity:"), endRightIntensitySpinBox_);
    endRightOpacitySpinBox_ = makeOpacitySpinBox(endGroup, QStringLiteral("endRightOpacitySpinBox"));
    endForm->addRow(tr("Right Opacity:"), endRightOpacitySpinBox_);
    frequencyLayout->addWidget(endGroup);

    root->addWidget(frequencyGroupContainer);

    // Every spin box in a group re-reads all four of that group's own
    // current values and writes them back as one stop - simpler than
    // updating a single field in place, and no less correct, since
    // every control is always visible/current at once (unlike, say, a
    // multi-page wizard where only one field might be on-screen).
    const auto applyStart = [this]() {
        GradientStop stop = config_.frequencyGradient().stops().front();
        stop.leftIntensity = static_cast<float>(startLeftIntensitySpinBox_->value());
        stop.leftOpacity = static_cast<float>(startLeftOpacitySpinBox_->value());
        stop.rightIntensity = static_cast<float>(startRightIntensitySpinBox_->value());
        stop.rightOpacity = static_cast<float>(startRightOpacitySpinBox_->value());
        config_.frequencyGradient().setStopValues(0, stop);
        emitConfigChanged();
    };
    connect(startLeftIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });
    connect(startLeftOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });
    connect(startRightIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });
    connect(startRightOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyStart](double) { applyStart(); });

    const auto applyEnd = [this]() {
        GradientStop stop = config_.frequencyGradient().stops().back();
        stop.leftIntensity = static_cast<float>(endLeftIntensitySpinBox_->value());
        stop.leftOpacity = static_cast<float>(endLeftOpacitySpinBox_->value());
        stop.rightIntensity = static_cast<float>(endRightIntensitySpinBox_->value());
        stop.rightOpacity = static_cast<float>(endRightOpacitySpinBox_->value());
        const auto& stops = config_.frequencyGradient().stops();
        config_.frequencyGradient().setStopValues(stops.size() - 1, stop);
        emitConfigChanged();
    };
    connect(endLeftIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });
    connect(endLeftOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });
    connect(endRightIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });
    connect(endRightOpacitySpinBox_, &QDoubleSpinBox::valueChanged, this, [applyEnd](double) { applyEnd(); });

    uniformBlurGroup_ = new QGroupBox(tr("Uniform Blur"), container);
    uniformBlurGroup_->setObjectName(QStringLiteral("uniformBlurGroup"));
    auto* uniformBlurForm = new QFormLayout(uniformBlurGroup_);
    blurSigmaSpinBox_ = new QDoubleSpinBox(uniformBlurGroup_);
    blurSigmaSpinBox_->setObjectName(QStringLiteral("blurSigmaSpinBox"));
    blurSigmaSpinBox_->setRange(0.1, 50.0);
    blurSigmaSpinBox_->setSingleStep(0.1);
    blurSigmaSpinBox_->setDecimals(1);
    connect(blurSigmaSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setBlurSigma(static_cast<float>(value));
        emitConfigChanged();
    });
    blurSigmaMindWaveCombo_ = makeMindWaveCombo(uniformBlurGroup_, QStringLiteral("blurSigmaMindWaveCombo"));
    connect(blurSigmaMindWaveCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto rawId = blurSigmaMindWaveCombo_->itemData(index).toULongLong();
        config_.setBlurSigmaMindWave(rawId == 0 ? std::nullopt : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
        emitConfigChanged();
    });
    uniformBlurForm->addRow(tr("Sigma:"), makeBoundFieldRow(blurSigmaSpinBox_, blurSigmaMindWaveCombo_));
    root->addWidget(uniformBlurGroup_);

    edgePreservingBlurGroup_ = new QGroupBox(tr("Edge-Preserving Blur"), container);
    edgePreservingBlurGroup_->setObjectName(QStringLiteral("edgePreservingBlurGroup"));
    auto* edgePreservingBlurForm = new QFormLayout(edgePreservingBlurGroup_);
    medianSizeSpinBox_ = new QSpinBox(edgePreservingBlurGroup_);
    medianSizeSpinBox_->setObjectName(QStringLiteral("medianSizeSpinBox"));
    medianSizeSpinBox_->setRange(3, 31);
    medianSizeSpinBox_->setToolTip(
        tr("The median window's own size, in bins/columns - an even value behaves as if rounded up to the next "
           "odd one."));
    connect(medianSizeSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        config_.setMedianSize(value);
        emitConfigChanged();
    });
    medianSizeMindWaveCombo_ = makeMindWaveCombo(edgePreservingBlurGroup_, QStringLiteral("medianSizeMindWaveCombo"));
    connect(medianSizeMindWaveCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto rawId = medianSizeMindWaveCombo_->itemData(index).toULongLong();
        config_.setMedianSizeMindWave(rawId == 0 ? std::nullopt : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
        emitConfigChanged();
    });
    edgePreservingBlurForm->addRow(tr("Size:"), makeBoundFieldRow(medianSizeSpinBox_, medianSizeMindWaveCombo_));
    root->addWidget(edgePreservingBlurGroup_);

    directionalBlurGroup_ = new QGroupBox(tr("Directional Blur"), container);
    directionalBlurGroup_->setObjectName(QStringLiteral("directionalBlurGroup"));
    auto* directionalBlurForm = new QFormLayout(directionalBlurGroup_);
    directionalBlurLengthSpinBox_ = new QSpinBox(directionalBlurGroup_);
    directionalBlurLengthSpinBox_->setObjectName(QStringLiteral("directionalBlurLengthSpinBox"));
    directionalBlurLengthSpinBox_->setRange(1, 200);
    connect(directionalBlurLengthSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        config_.setDirectionalBlurLength(value);
        emitConfigChanged();
    });
    directionalBlurLengthMindWaveCombo_ =
        makeMindWaveCombo(directionalBlurGroup_, QStringLiteral("directionalBlurLengthMindWaveCombo"));
    connect(directionalBlurLengthMindWaveCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto rawId = directionalBlurLengthMindWaveCombo_->itemData(index).toULongLong();
        config_.setDirectionalBlurLengthMindWave(
            rawId == 0 ? std::nullopt : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
        emitConfigChanged();
    });
    directionalBlurForm->addRow(tr("Length:"),
                                 makeBoundFieldRow(directionalBlurLengthSpinBox_, directionalBlurLengthMindWaveCombo_));
    directionalBlurAngleSpinBox_ = new QDoubleSpinBox(directionalBlurGroup_);
    directionalBlurAngleSpinBox_->setObjectName(QStringLiteral("directionalBlurAngleSpinBox"));
    directionalBlurAngleSpinBox_->setRange(0.0, 360.0);
    directionalBlurAngleSpinBox_->setSingleStep(1.0);
    directionalBlurAngleSpinBox_->setDecimals(1);
    directionalBlurAngleSpinBox_->setSuffix(QStringLiteral("°"));
    directionalBlurAngleSpinBox_->setToolTip(
        tr("0° blurs along the time axis (columns), 90° along the frequency axis (bins)."));
    connect(directionalBlurAngleSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setDirectionalBlurAngleDegrees(static_cast<float>(value));
        emitConfigChanged();
    });
    directionalBlurAngleMindWaveCombo_ =
        makeMindWaveCombo(directionalBlurGroup_, QStringLiteral("directionalBlurAngleMindWaveCombo"));
    connect(directionalBlurAngleMindWaveCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto rawId = directionalBlurAngleMindWaveCombo_->itemData(index).toULongLong();
        config_.setDirectionalBlurAngleMindWave(
            rawId == 0 ? std::nullopt : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
        emitConfigChanged();
    });
    directionalBlurForm->addRow(tr("Angle:"),
                                 makeBoundFieldRow(directionalBlurAngleSpinBox_, directionalBlurAngleMindWaveCombo_));
    root->addWidget(directionalBlurGroup_);

    sharpenGroup_ = new QGroupBox(tr("Sharpen"), container);
    sharpenGroup_->setObjectName(QStringLiteral("sharpenGroup"));
    auto* sharpenForm = new QFormLayout(sharpenGroup_);
    sharpenAmountSpinBox_ = new QDoubleSpinBox(sharpenGroup_);
    sharpenAmountSpinBox_->setObjectName(QStringLiteral("sharpenAmountSpinBox"));
    sharpenAmountSpinBox_->setRange(0.1, 5.0);
    sharpenAmountSpinBox_->setSingleStep(0.1);
    sharpenAmountSpinBox_->setDecimals(1);
    connect(sharpenAmountSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSharpenAmount(static_cast<float>(value));
        emitConfigChanged();
    });
    sharpenAmountMindWaveCombo_ = makeMindWaveCombo(sharpenGroup_, QStringLiteral("sharpenAmountMindWaveCombo"));
    connect(sharpenAmountMindWaveCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto rawId = sharpenAmountMindWaveCombo_->itemData(index).toULongLong();
        config_.setSharpenAmountMindWave(rawId == 0 ? std::nullopt
                                                     : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
        emitConfigChanged();
    });
    sharpenForm->addRow(tr("Amount:"), makeBoundFieldRow(sharpenAmountSpinBox_, sharpenAmountMindWaveCombo_));
    root->addWidget(sharpenGroup_);

    speckleAddGroup_ = new QGroupBox(tr("Speckle Add"), container);
    speckleAddGroup_->setObjectName(QStringLiteral("speckleAddGroup"));
    auto* speckleAddForm = new QFormLayout(speckleAddGroup_);
    speckleAddDensitySpinBox_ = new QDoubleSpinBox(speckleAddGroup_);
    speckleAddDensitySpinBox_->setObjectName(QStringLiteral("speckleAddDensitySpinBox"));
    speckleAddDensitySpinBox_->setRange(0.0, 1.0);
    speckleAddDensitySpinBox_->setSingleStep(0.01);
    speckleAddDensitySpinBox_->setDecimals(2);
    connect(speckleAddDensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSpeckleDensity(static_cast<float>(value));
        // Keeps DynamicSpeckle's own sibling widget in sync - see this
        // class's own docs on why the two share a field but not a widget.
        const QSignalBlocker blocker(dynamicSpeckleDensitySpinBox_);
        dynamicSpeckleDensitySpinBox_->setValue(value);
        emitConfigChanged();
    });
    speckleAddForm->addRow(tr("Density:"), speckleAddDensitySpinBox_);
    speckleAddIntensitySpinBox_ = new QDoubleSpinBox(speckleAddGroup_);
    speckleAddIntensitySpinBox_->setObjectName(QStringLiteral("speckleAddIntensitySpinBox"));
    speckleAddIntensitySpinBox_->setRange(0.0, 1.0);
    speckleAddIntensitySpinBox_->setSingleStep(0.01);
    speckleAddIntensitySpinBox_->setDecimals(2);
    connect(speckleAddIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSpeckleIntensity(static_cast<float>(value));
        const QSignalBlocker blocker(dynamicSpeckleIntensitySpinBox_);
        dynamicSpeckleIntensitySpinBox_->setValue(value);
        emitConfigChanged();
    });
    speckleAddForm->addRow(tr("Intensity:"), speckleAddIntensitySpinBox_);
    speckleAddGroup_->setToolTip(
        tr("Deterministic per Filter layer - the same pattern every recomposite, only changing if you touch these "
           "controls or the content underneath."));
    root->addWidget(speckleAddGroup_);

    speckleRemoveGroup_ = new QGroupBox(tr("Speckle Remove"), container);
    speckleRemoveGroup_->setObjectName(QStringLiteral("speckleRemoveGroup"));
    auto* speckleRemoveForm = new QFormLayout(speckleRemoveGroup_);
    speckleThresholdSpinBox_ = new QDoubleSpinBox(speckleRemoveGroup_);
    speckleThresholdSpinBox_->setObjectName(QStringLiteral("speckleThresholdSpinBox"));
    speckleThresholdSpinBox_->setRange(0.0, 96.0);
    speckleThresholdSpinBox_->setSingleStep(1.0);
    speckleThresholdSpinBox_->setSuffix(QStringLiteral(" dB"));
    connect(speckleThresholdSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSpeckleThresholdDb(static_cast<float>(value));
        emitConfigChanged();
    });
    speckleRemoveForm->addRow(tr("Threshold:"), speckleThresholdSpinBox_);
    root->addWidget(speckleRemoveGroup_);

    denoiseGroup_ = new QGroupBox(tr("Denoise"), container);
    denoiseGroup_->setObjectName(QStringLiteral("denoiseGroup"));
    auto* denoiseForm = new QFormLayout(denoiseGroup_);
    noiseFloorSpinBox_ = new QDoubleSpinBox(denoiseGroup_);
    noiseFloorSpinBox_->setObjectName(QStringLiteral("noiseFloorSpinBox"));
    noiseFloorSpinBox_->setRange(-96.0, 0.0);
    noiseFloorSpinBox_->setSingleStep(1.0);
    noiseFloorSpinBox_->setSuffix(QStringLiteral(" dB"));
    connect(noiseFloorSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setNoiseFloorDb(static_cast<float>(value));
        emitConfigChanged();
    });
    denoiseForm->addRow(tr("Noise Floor:"), noiseFloorSpinBox_);
    reductionSpinBox_ = new QDoubleSpinBox(denoiseGroup_);
    reductionSpinBox_->setObjectName(QStringLiteral("reductionSpinBox"));
    reductionSpinBox_->setRange(0.0, 96.0);
    reductionSpinBox_->setSingleStep(1.0);
    reductionSpinBox_->setSuffix(QStringLiteral(" dB"));
    connect(reductionSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setReductionDb(static_cast<float>(value));
        emitConfigChanged();
    });
    denoiseForm->addRow(tr("Reduction:"), reductionSpinBox_);
    root->addWidget(denoiseGroup_);

    bitDepthCrushGroup_ = new QGroupBox(tr("Bit-Depth Crush"), container);
    bitDepthCrushGroup_->setObjectName(QStringLiteral("bitDepthCrushGroup"));
    auto* bitDepthCrushForm = new QFormLayout(bitDepthCrushGroup_);
    crushAmountSpinBox_ = new QDoubleSpinBox(bitDepthCrushGroup_);
    crushAmountSpinBox_->setObjectName(QStringLiteral("crushAmountSpinBox"));
    crushAmountSpinBox_->setRange(0.0, 1.0);
    crushAmountSpinBox_->setSingleStep(0.01);
    crushAmountSpinBox_->setDecimals(2);
    connect(crushAmountSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setCrushAmount(static_cast<float>(value));
        emitConfigChanged();
    });
    bitDepthCrushForm->addRow(tr("Amount:"), crushAmountSpinBox_);
    root->addWidget(bitDepthCrushGroup_);

    granularNoiseGroup_ = new QGroupBox(tr("Granular Noise"), container);
    granularNoiseGroup_->setObjectName(QStringLiteral("granularNoiseGroup"));
    auto* granularNoiseForm = new QFormLayout(granularNoiseGroup_);
    grainSizeSpinBox_ = new QSpinBox(granularNoiseGroup_);
    grainSizeSpinBox_->setObjectName(QStringLiteral("grainSizeSpinBox"));
    grainSizeSpinBox_->setRange(1, 64);
    grainSizeSpinBox_->setToolTip(tr("The grain block's own size, in bins/columns."));
    connect(grainSizeSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        config_.setGrainSize(value);
        emitConfigChanged();
    });
    granularNoiseForm->addRow(tr("Grain Size:"), grainSizeSpinBox_);
    grainAmountSpinBox_ = new QDoubleSpinBox(granularNoiseGroup_);
    grainAmountSpinBox_->setObjectName(QStringLiteral("grainAmountSpinBox"));
    grainAmountSpinBox_->setRange(0.0, 50.0);
    grainAmountSpinBox_->setSingleStep(0.5);
    grainAmountSpinBox_->setSuffix(QStringLiteral(" dB"));
    connect(grainAmountSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setGrainAmountDb(static_cast<float>(value));
        emitConfigChanged();
    });
    granularNoiseForm->addRow(tr("Grain Amount:"), grainAmountSpinBox_);
    granularNoiseGroup_->setToolTip(
        tr("Deterministic per Filter layer - the same pattern every recomposite, only changing if you touch these "
           "controls or the content underneath."));
    root->addWidget(granularNoiseGroup_);

    dynamicSpeckleGroup_ = new QGroupBox(tr("Dynamic Speckle"), container);
    dynamicSpeckleGroup_->setObjectName(QStringLiteral("dynamicSpeckleGroup"));
    auto* dynamicSpeckleForm = new QFormLayout(dynamicSpeckleGroup_);
    dynamicSpeckleDensitySpinBox_ = new QDoubleSpinBox(dynamicSpeckleGroup_);
    dynamicSpeckleDensitySpinBox_->setObjectName(QStringLiteral("dynamicSpeckleDensitySpinBox"));
    dynamicSpeckleDensitySpinBox_->setRange(0.0, 1.0);
    dynamicSpeckleDensitySpinBox_->setSingleStep(0.01);
    dynamicSpeckleDensitySpinBox_->setDecimals(2);
    connect(dynamicSpeckleDensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSpeckleDensity(static_cast<float>(value));
        const QSignalBlocker blocker(speckleAddDensitySpinBox_);
        speckleAddDensitySpinBox_->setValue(value);
        emitConfigChanged();
    });
    dynamicSpeckleForm->addRow(tr("Density:"), dynamicSpeckleDensitySpinBox_);
    dynamicSpeckleIntensitySpinBox_ = new QDoubleSpinBox(dynamicSpeckleGroup_);
    dynamicSpeckleIntensitySpinBox_->setObjectName(QStringLiteral("dynamicSpeckleIntensitySpinBox"));
    dynamicSpeckleIntensitySpinBox_->setRange(0.0, 1.0);
    dynamicSpeckleIntensitySpinBox_->setSingleStep(0.01);
    dynamicSpeckleIntensitySpinBox_->setDecimals(2);
    connect(dynamicSpeckleIntensitySpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setSpeckleIntensity(static_cast<float>(value));
        const QSignalBlocker blocker(speckleAddIntensitySpinBox_);
        speckleAddIntensitySpinBox_->setValue(value);
        emitConfigChanged();
    });
    dynamicSpeckleForm->addRow(tr("Intensity:"), dynamicSpeckleIntensitySpinBox_);
    dynamicSpeckleGroup_->setToolTip(
        tr("Live - freshly re-randomized on every recomposite (any edit, scroll, or repaint), computed over fixed "
           "2x2 blocks to stay cheap on a large canvas."));
    root->addWidget(dynamicSpeckleGroup_);

    feedbackDistortionGroup_ = new QGroupBox(tr("Feedback Distortion"), container);
    feedbackDistortionGroup_->setObjectName(QStringLiteral("feedbackDistortionGroup"));
    auto* feedbackDistortionForm = new QFormLayout(feedbackDistortionGroup_);
    feedbackAmountSpinBox_ = new QDoubleSpinBox(feedbackDistortionGroup_);
    feedbackAmountSpinBox_->setObjectName(QStringLiteral("feedbackAmountSpinBox"));
    feedbackAmountSpinBox_->setRange(0.0, 0.99);
    feedbackAmountSpinBox_->setSingleStep(0.01);
    feedbackAmountSpinBox_->setDecimals(2);
    connect(feedbackAmountSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setFeedbackAmount(static_cast<float>(value));
        emitConfigChanged();
    });
    feedbackDistortionForm->addRow(tr("Amount:"), feedbackAmountSpinBox_);
    root->addWidget(feedbackDistortionGroup_);

    spectralWavefoldGroup_ = new QGroupBox(tr("Spectral Wavefold"), container);
    spectralWavefoldGroup_->setObjectName(QStringLiteral("spectralWavefoldGroup"));
    auto* spectralWavefoldForm = new QFormLayout(spectralWavefoldGroup_);
    foldGainSpinBox_ = new QDoubleSpinBox(spectralWavefoldGroup_);
    foldGainSpinBox_->setObjectName(QStringLiteral("foldGainSpinBox"));
    foldGainSpinBox_->setRange(1.0, 20.0);
    foldGainSpinBox_->setSingleStep(0.1);
    foldGainSpinBox_->setDecimals(1);
    connect(foldGainSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        config_.setFoldGain(static_cast<float>(value));
        emitConfigChanged();
    });
    spectralWavefoldForm->addRow(tr("Fold Gain:"), foldGainSpinBox_);
    root->addWidget(spectralWavefoldGroup_);

    toneCurveGroup_ = new QGroupBox(tr("Tone Curve"), container);
    toneCurveGroup_->setObjectName(QStringLiteral("toneCurveGroup"));
    auto* toneCurveLayout = new QVBoxLayout(toneCurveGroup_);
    auto* toneCurveHint = new QLabel(
        tr("Click to add a point, drag to move it, double-click an interior point to remove it."), toneCurveGroup_);
    toneCurveHint->setWordWrap(true);
    toneCurveLayout->addWidget(toneCurveHint);
    toneCurveEditor_ = new ToneCurveEditor(toneCurveGroup_);
    toneCurveEditor_->setObjectName(QStringLiteral("toneCurveEditor"));
    connect(toneCurveEditor_, &ToneCurveEditor::pointsChanged, this,
            [this](const std::vector<std::array<float, 2>>& points) {
                config_.setToneCurvePoints(points);
                emitConfigChanged();
            });
    toneCurveLayout->addWidget(toneCurveEditor_);
    root->addWidget(toneCurveGroup_);

    equalizerCutGroup_ = new QGroupBox(tr("Cut"), container);
    equalizerCutGroup_->setObjectName(QStringLiteral("equalizerCutGroup"));
    auto* cutLayout = new QVBoxLayout(equalizerCutGroup_);

    auto* cutStartGroup = new QGroupBox(tr("Start (t=0, lowest frequency)"), equalizerCutGroup_);
    auto* cutStartForm = new QFormLayout(cutStartGroup);
    startLeftCutSpinBox_ = makeOpacitySpinBox(cutStartGroup, QStringLiteral("startLeftCutSpinBox"));
    cutStartForm->addRow(tr("Left Cut:"), startLeftCutSpinBox_);
    startRightCutSpinBox_ = makeOpacitySpinBox(cutStartGroup, QStringLiteral("startRightCutSpinBox"));
    cutStartForm->addRow(tr("Right Cut:"), startRightCutSpinBox_);
    cutLayout->addWidget(cutStartGroup);

    auto* cutEndGroup = new QGroupBox(tr("End (t=1, highest frequency)"), equalizerCutGroup_);
    auto* cutEndForm = new QFormLayout(cutEndGroup);
    endLeftCutSpinBox_ = makeOpacitySpinBox(cutEndGroup, QStringLiteral("endLeftCutSpinBox"));
    cutEndForm->addRow(tr("Left Cut:"), endLeftCutSpinBox_);
    endRightCutSpinBox_ = makeOpacitySpinBox(cutEndGroup, QStringLiteral("endRightCutSpinBox"));
    cutEndForm->addRow(tr("Right Cut:"), endRightCutSpinBox_);
    cutLayout->addWidget(cutEndGroup);

    // Cut only ever writes opacity - intensity is always the silence
    // floor underneath (-96 dB, this codebase's own established floor),
    // never shown or user-editable here - see this class's own docs.
    const auto applyCutStart = [this]() {
        GradientStop stop = config_.frequencyGradient().stops().front();
        stop.leftIntensity = -96.0f;
        stop.rightIntensity = -96.0f;
        stop.leftOpacity = static_cast<float>(startLeftCutSpinBox_->value());
        stop.rightOpacity = static_cast<float>(startRightCutSpinBox_->value());
        config_.frequencyGradient().setStopValues(0, stop);
        emitConfigChanged();
    };
    connect(startLeftCutSpinBox_, &QDoubleSpinBox::valueChanged, this, [applyCutStart](double) { applyCutStart(); });
    connect(startRightCutSpinBox_, &QDoubleSpinBox::valueChanged, this, [applyCutStart](double) { applyCutStart(); });

    const auto applyCutEnd = [this]() {
        GradientStop stop = config_.frequencyGradient().stops().back();
        stop.leftIntensity = -96.0f;
        stop.rightIntensity = -96.0f;
        stop.leftOpacity = static_cast<float>(endLeftCutSpinBox_->value());
        stop.rightOpacity = static_cast<float>(endRightCutSpinBox_->value());
        const auto& stops = config_.frequencyGradient().stops();
        config_.frequencyGradient().setStopValues(stops.size() - 1, stop);
        emitConfigChanged();
    };
    connect(endLeftCutSpinBox_, &QDoubleSpinBox::valueChanged, this, [applyCutEnd](double) { applyCutEnd(); });
    connect(endRightCutSpinBox_, &QDoubleSpinBox::valueChanged, this, [applyCutEnd](double) { applyCutEnd(); });

    root->addWidget(equalizerCutGroup_);

    root->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);

    rebuildMindWaveCombos();
    updateVisibleGroup();
}

void FilterConfigurationPanel::emitConfigChanged() { emit filterConfigurationChanged(config_); }

void FilterConfigurationPanel::updateVisibleGroup() {
    filterTypeCombo_->setVisible(!isEqualizerMode_);
    if (filterTypeLabel_ != nullptr) {
        filterTypeLabel_->setVisible(!isEqualizerMode_);
    }
    equalizerCutGroup_->setVisible(isEqualizerMode_);

    // In Equalizer mode, the Cut group above is the only one shown -
    // config_.type() is always FrequencyAxisGradient for the Equalizer
    // anyway (it's never switched away, since the combo above is
    // hidden), but every per-type group stays hidden regardless of
    // type() while this mode is active.
    const FilterType type = config_.type();
    frequencyAxisGradientSection_->setVisible(!isEqualizerMode_ && type == FilterType::FrequencyAxisGradient);
    uniformBlurGroup_->setVisible(!isEqualizerMode_ && type == FilterType::UniformBlur);
    edgePreservingBlurGroup_->setVisible(!isEqualizerMode_ && type == FilterType::EdgePreservingBlur);
    directionalBlurGroup_->setVisible(!isEqualizerMode_ && type == FilterType::DirectionalBlur);
    sharpenGroup_->setVisible(!isEqualizerMode_ && type == FilterType::Sharpen);
    speckleAddGroup_->setVisible(!isEqualizerMode_ && type == FilterType::SpeckleAdd);
    speckleRemoveGroup_->setVisible(!isEqualizerMode_ && type == FilterType::SpeckleRemove);
    denoiseGroup_->setVisible(!isEqualizerMode_ && type == FilterType::Denoise);
    bitDepthCrushGroup_->setVisible(!isEqualizerMode_ && type == FilterType::BitDepthCrush);
    granularNoiseGroup_->setVisible(!isEqualizerMode_ && type == FilterType::GranularNoise);
    dynamicSpeckleGroup_->setVisible(!isEqualizerMode_ && type == FilterType::DynamicSpeckle);
    feedbackDistortionGroup_->setVisible(!isEqualizerMode_ && type == FilterType::FeedbackDistortion);
    spectralWavefoldGroup_->setVisible(!isEqualizerMode_ && type == FilterType::SpectralWavefold);
    toneCurveGroup_->setVisible(!isEqualizerMode_ && type == FilterType::ToneCurve);
}

void FilterConfigurationPanel::setEqualizerMode(bool isEqualizer) {
    isEqualizerMode_ = isEqualizer;
    updateVisibleGroup();
}

void FilterConfigurationPanel::setFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    config_ = config;

    // QSignalBlocker on every control - see ToolConfigurationPanel::
    // setToolConfiguration()'s own docs for why: only the *display*
    // should change here, not re-trigger each control's own change
    // handler (which would both redundantly re-set config_ to the value
    // it already has and spuriously emit filterConfigurationChanged()).
    const QSignalBlocker filterTypeBlocker(filterTypeCombo_);
    const QSignalBlocker startLeftIntensityBlocker(startLeftIntensitySpinBox_);
    const QSignalBlocker startLeftOpacityBlocker(startLeftOpacitySpinBox_);
    const QSignalBlocker startRightIntensityBlocker(startRightIntensitySpinBox_);
    const QSignalBlocker startRightOpacityBlocker(startRightOpacitySpinBox_);
    const QSignalBlocker endLeftIntensityBlocker(endLeftIntensitySpinBox_);
    const QSignalBlocker endLeftOpacityBlocker(endLeftOpacitySpinBox_);
    const QSignalBlocker endRightIntensityBlocker(endRightIntensitySpinBox_);
    const QSignalBlocker endRightOpacityBlocker(endRightOpacitySpinBox_);
    const QSignalBlocker blurSigmaBlocker(blurSigmaSpinBox_);
    const QSignalBlocker medianSizeBlocker(medianSizeSpinBox_);
    const QSignalBlocker directionalBlurLengthBlocker(directionalBlurLengthSpinBox_);
    const QSignalBlocker directionalBlurAngleBlocker(directionalBlurAngleSpinBox_);
    const QSignalBlocker sharpenAmountBlocker(sharpenAmountSpinBox_);
    const QSignalBlocker speckleAddDensityBlocker(speckleAddDensitySpinBox_);
    const QSignalBlocker speckleAddIntensityBlocker(speckleAddIntensitySpinBox_);
    const QSignalBlocker speckleThresholdBlocker(speckleThresholdSpinBox_);
    const QSignalBlocker noiseFloorBlocker(noiseFloorSpinBox_);
    const QSignalBlocker reductionBlocker(reductionSpinBox_);
    const QSignalBlocker crushAmountBlocker(crushAmountSpinBox_);
    const QSignalBlocker grainSizeBlocker(grainSizeSpinBox_);
    const QSignalBlocker grainAmountBlocker(grainAmountSpinBox_);
    const QSignalBlocker dynamicSpeckleDensityBlocker(dynamicSpeckleDensitySpinBox_);
    const QSignalBlocker dynamicSpeckleIntensityBlocker(dynamicSpeckleIntensitySpinBox_);
    const QSignalBlocker feedbackAmountBlocker(feedbackAmountSpinBox_);
    const QSignalBlocker foldGainBlocker(foldGainSpinBox_);
    const QSignalBlocker startLeftCutBlocker(startLeftCutSpinBox_);
    const QSignalBlocker startRightCutBlocker(startRightCutSpinBox_);
    const QSignalBlocker endLeftCutBlocker(endLeftCutSpinBox_);
    const QSignalBlocker endRightCutBlocker(endRightCutSpinBox_);

    // Every FilterType is selectable now - findData() only ever falls
    // back to index 0 here for a corrupted/out-of-range stored value
    // (hand-edited project JSON), the same defensive fallback
    // ToolConfigurationPanel's own combos already establish, without
    // mutating config_.type() itself.
    const int typeIndex = filterTypeCombo_->findData(QVariant::fromValue(static_cast<int>(config_.type())));
    filterTypeCombo_->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);

    const auto& stops = config_.frequencyGradient().stops();
    startLeftIntensitySpinBox_->setValue(stops.front().leftIntensity);
    startLeftOpacitySpinBox_->setValue(stops.front().leftOpacity);
    startRightIntensitySpinBox_->setValue(stops.front().rightIntensity);
    startRightOpacitySpinBox_->setValue(stops.front().rightOpacity);
    endLeftIntensitySpinBox_->setValue(stops.back().leftIntensity);
    endLeftOpacitySpinBox_->setValue(stops.back().leftOpacity);
    endRightIntensitySpinBox_->setValue(stops.back().rightIntensity);
    endRightOpacitySpinBox_->setValue(stops.back().rightOpacity);
    // Cut mirrors the same gradient's own opacity - intensity has no Cut
    // counterpart to sync (it's write-only from this panel's own side).
    startLeftCutSpinBox_->setValue(stops.front().leftOpacity);
    startRightCutSpinBox_->setValue(stops.front().rightOpacity);
    endLeftCutSpinBox_->setValue(stops.back().leftOpacity);
    endRightCutSpinBox_->setValue(stops.back().rightOpacity);

    blurSigmaSpinBox_->setValue(config_.blurSigma());
    medianSizeSpinBox_->setValue(config_.medianSize());
    directionalBlurLengthSpinBox_->setValue(config_.directionalBlurLength());
    directionalBlurAngleSpinBox_->setValue(config_.directionalBlurAngleDegrees());
    sharpenAmountSpinBox_->setValue(config_.sharpenAmount());
    // SpeckleAdd and DynamicSpeckle share the same underlying fields (see
    // this class's own docs) - both sets of widgets sync to them here.
    speckleAddDensitySpinBox_->setValue(config_.speckleDensity());
    speckleAddIntensitySpinBox_->setValue(config_.speckleIntensity());
    dynamicSpeckleDensitySpinBox_->setValue(config_.speckleDensity());
    dynamicSpeckleIntensitySpinBox_->setValue(config_.speckleIntensity());
    speckleThresholdSpinBox_->setValue(config_.speckleThresholdDb());
    noiseFloorSpinBox_->setValue(config_.noiseFloorDb());
    reductionSpinBox_->setValue(config_.reductionDb());
    crushAmountSpinBox_->setValue(config_.crushAmount());
    grainSizeSpinBox_->setValue(config_.grainSize());
    grainAmountSpinBox_->setValue(config_.grainAmountDb());
    feedbackAmountSpinBox_->setValue(config_.feedbackAmount());
    foldGainSpinBox_->setValue(config_.foldGain());
    // ToneCurveEditor::setPoints() doesn't emit pointsChanged() by its
    // own contract, so no QSignalBlocker is needed here.
    toneCurveEditor_->setPoints(config_.toneCurvePoints());
    // rebuildMindWaveCombos() blocks its own five combos' signals
    // internally - no separate QSignalBlocker needed here for them.
    rebuildMindWaveCombos();

    updateVisibleGroup();
}

void FilterConfigurationPanel::setAvailableMindWaves(
    const std::vector<std::pair<MindWaveId, QString>>& mindWaves) {
    availableMindWaves_ = mindWaves;
    rebuildMindWaveCombos();
}

void FilterConfigurationPanel::rebuildMindWaveCombos() {
    const auto populate = [this](QComboBox* combo, std::optional<MindWaveId> boundId) {
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(tr("None"), QVariant::fromValue(qulonglong{0}));
        int selectedIndex = 0;
        for (const auto& [mindWaveId, name] : availableMindWaves_) {
            combo->addItem(name, QVariant::fromValue(static_cast<qulonglong>(mindWaveId)));
            if (boundId.has_value() && *boundId == mindWaveId) {
                selectedIndex = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(selectedIndex);
    };
    populate(blurSigmaMindWaveCombo_, config_.blurSigmaMindWave());
    populate(medianSizeMindWaveCombo_, config_.medianSizeMindWave());
    populate(directionalBlurLengthMindWaveCombo_, config_.directionalBlurLengthMindWave());
    populate(directionalBlurAngleMindWaveCombo_, config_.directionalBlurAngleMindWave());
    populate(sharpenAmountMindWaveCombo_, config_.sharpenAmountMindWave());
}

}  // namespace sound_mind::studio
