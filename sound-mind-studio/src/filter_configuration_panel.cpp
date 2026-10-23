#include "sound_mind/studio/filter_configuration_panel.h"

#include <algorithm>
#include <string_view>
#include <stdexcept>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include "sound_mind/studio/equalizer_curve_widget.h"
#include "sound_mind/studio/gradient_editor_widget.h"
#include "sound_mind/studio/numeric_field.h"
#include "sound_mind/studio/preset_indicator_label.h"
#include "sound_mind/studio/tone_curve_editor.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::ConvolutionKernelId;
using sound_mind::core::DownsampleMode;
using sound_mind::core::FilterType;
using sound_mind::core::FilterParameter;
using sound_mind::core::MindWaveId;

/// @brief One of Convolve's own eight built-in presets - a name plus a
/// classic 3x3 image-processing kernel, matching the legacy Python
/// Studio's own `CONVOLVE_PRESETS` exactly. Box Blur/Gaussian Blur default
/// `normalize` to `true` (their own coefficients don't already sum to 1);
/// the rest default it to `false`.
struct ConvolvePreset {
    const char* name;
    std::array<float, 9> kernel;
    bool normalize;
};

constexpr std::array<ConvolvePreset, 8> kConvolvePresets{{
    {"Identity", {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, false},
    {"Sharpen", {0.0f, -1.0f, 0.0f, -1.0f, 5.0f, -1.0f, 0.0f, -1.0f, 0.0f}, false},
    {"Edge Detect", {-1.0f, -1.0f, -1.0f, -1.0f, 8.0f, -1.0f, -1.0f, -1.0f, -1.0f}, false},
    {"Emboss", {-2.0f, -1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 2.0f}, false},
    {"Box Blur", {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, true},
    {"Gaussian Blur", {1.0f, 2.0f, 1.0f, 2.0f, 4.0f, 2.0f, 1.0f, 2.0f, 1.0f}, true},
    {"Sobel X", {-1.0f, 0.0f, 1.0f, -2.0f, 0.0f, 2.0f, -1.0f, 0.0f, 1.0f}, false},
    {"Sobel Y", {-1.0f, -2.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f}, false},
}};

/// @brief See `tool_configuration_panel.cpp`'s own identical constant -
/// duplicated for the same "neither panel exposes a header the other
/// would reach into" reason `makeMindWaveCombo()`'s own docs already
/// give. `v0.Y.62.1` Installment G.
constexpr qulonglong kCreateNewMindWaveSentinel = std::numeric_limits<qulonglong>::max();

/// @brief Wraps `spinBox` and `combo` side by side - every bindable
/// parameter's own row uses this instead of the spin box alone.
QHBoxLayout* makeBoundFieldRow(QWidget* spinBox, QWidget* combo) {
    auto* row = new QHBoxLayout();
    row->addWidget(spinBox);
    row->addWidget(combo);
    return row;
}

/// @brief How one numeric field looks (`v0.1.48.10`): its object-name stem
///        (`<stem>SpinBox`, `<stem>MindWaveCombo`), its parameter, and the
///        presentation the core table (`filterParameterSpecs()`) doesn't
///        carry. Ranges and integer-ness come from the core table. The
///        Convolve kernel size isn't here: changing it also resets the
///        kernel, so it keeps its own handler.
struct ParameterFieldLook {
    const char* stem;           ///< Object names' stem.
    FilterParameter parameter;  ///< What it edits.
    double step;                ///< Single step; 0 keeps the field's default.
    int decimals;               ///< Decimals; -1 keeps the field's default.
    const char* suffix;         ///< Unit suffix (UTF-8), or `nullptr`.
    const char* toolTip;        ///< Tooltip (translated in the panel's context), or `nullptr`.
};

/// @brief Every numeric field the panel shows, in construction order.
/// SpeckleAdd and DynamicSpeckle each have their own Density and Intensity
/// fields, both editing the same shared parameters.
constexpr std::array<ParameterFieldLook, 29> kParameterFieldLooks{{
    {"blurSigma", FilterParameter::BlurSigma, 0.1, 1, nullptr,
     nullptr},
    {"medianSize", FilterParameter::MedianSize, 0.0, -1, nullptr,
     "The median window's own size, in bins/columns - an even value behaves as if rounded up to the next "
         "odd one."},
    {"directionalBlurLength", FilterParameter::DirectionalBlurLength, 0.0, -1, nullptr,
     nullptr},
    {"directionalBlurAngle", FilterParameter::DirectionalBlurAngle, 1.0, 1, "\u00B0",
     "0° blurs along the time axis (columns), 90° along the frequency axis (bins)."},
    {"sharpenAmount", FilterParameter::SharpenAmount, 0.1, 1, nullptr,
     nullptr},
    {"speckleAddDensity", FilterParameter::SpeckleDensity, 0.01, 2, nullptr,
     nullptr},
    {"speckleAddIntensity", FilterParameter::SpeckleIntensity, 0.01, 2, nullptr,
     nullptr},
    {"speckleThreshold", FilterParameter::SpeckleThreshold, 1.0, -1, " dB",
     nullptr},
    {"noiseFloor", FilterParameter::NoiseFloor, 1.0, -1, " dB",
     nullptr},
    {"reduction", FilterParameter::Reduction, 1.0, -1, " dB",
     nullptr},
    {"crushAmount", FilterParameter::CrushAmount, 0.01, 2, nullptr,
     nullptr},
    {"grainSize", FilterParameter::GrainSize, 0.0, -1, nullptr,
     "The grain block's own size, in bins/columns."},
    {"grainAmount", FilterParameter::GrainAmount, 0.5, -1, " dB",
     nullptr},
    {"dynamicSpeckleDensity", FilterParameter::SpeckleDensity, 0.01, 2, nullptr,
     nullptr},
    {"dynamicSpeckleIntensity", FilterParameter::SpeckleIntensity, 0.01, 2, nullptr,
     nullptr},
    {"feedbackAmount", FilterParameter::FeedbackAmount, 0.01, 2, nullptr,
     nullptr},
    {"foldGain", FilterParameter::FoldGain, 0.1, 1, nullptr,
     nullptr},
    {"displaceDistance", FilterParameter::DisplaceDistance, 1.0, -1, nullptr,
     nullptr},
    {"displaceAngle", FilterParameter::DisplaceAngle, 1.0, 1, "\u00B0",
     "0° shifts along the time axis (columns), 90° along the frequency axis (bins)."},
    {"channelCycleAngle", FilterParameter::ChannelCycleAngle, 1.0, 1, "\u00B0",
     "0°/360° is no effect; every 120° is one full step rotating left loudness, right loudness, and phase "
         "into each other."},
    {"channelBalance", FilterParameter::ChannelBalance, 0.05, 2, nullptr,
     "0 sends all energy to the left channel, 1 to the right; 0.5 only reproduces the input exactly when "
         "left and right are already equal."},
    {"convolveAmount", FilterParameter::ConvolveAmount, 0.05, 2, nullptr,
     nullptr},
    {"reverbPreDelay", FilterParameter::ReverbPreDelay, 0.0, -1, nullptr,
     nullptr},
    {"reverbDecay", FilterParameter::ReverbDecay, 0.0, -1, nullptr,
     "The impulse response's own RT60 (-60dB point), before Room Size scales it."},
    {"reverbRoomSize", FilterParameter::ReverbRoomSize, 0.05, 2, nullptr,
     "Scales Decay down - a smaller room decays faster."},
    {"reverbDiffusion", FilterParameter::ReverbDiffusion, 0.05, 2, nullptr,
     nullptr},
    {"reverbAbsorption", FilterParameter::ReverbAbsorption, 0.05, 2, nullptr,
     "Damps higher encoded frequencies more, the way a real room would."},
    {"reverbMix", FilterParameter::ReverbMix, 0.05, 2, nullptr,
     nullptr},
    {"downsampleBlockSize", FilterParameter::DownsampleBlockSize, 0.0, -1, nullptr,
     "The pixelation block's own size, in bins/columns."},
}};

/// @brief The look for `stem`.
const ParameterFieldLook& parameterFieldLook(std::string_view stem) {
    for (const ParameterFieldLook& look : kParameterFieldLooks) {
        if (stem == look.stem) {
            return look;
        }
    }
    throw std::logic_error("No such filter parameter field");
}


/// @brief Every `FilterType`, in `docs/sound-mind-design.md`'s own family
/// order (Blur & focus, then Noise & distortion, then Geometric, then
/// Tonal, then Spectral shaping, then Space), and each family's own listed
/// sub-order - the first six shipped in `v0.Y.28.1`; the rest across
/// `v0.Y.36.1`'s own four installments (Noise & distortion, then
/// ChannelBalance/Invert/Convolve, then Displace/ChannelCycle, then
/// SpectralReverb, closing out the milestone).
constexpr std::array<std::pair<FilterType, const char*>, 21> kSelectableFilterTypes{{
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
    {FilterType::Displace, "Displace"},
    {FilterType::ChannelCycle, "Channel Cycle"},
    {FilterType::ToneCurve, "Tone Curve"},
    {FilterType::ChannelBalance, "Channel Balance"},
    {FilterType::Invert, "Invert"},
    {FilterType::FrequencyAxisGradient, "Frequency-Axis Gradient"},
    {FilterType::Convolve, "Convolve"},
    {FilterType::SpectralReverb, "Spectral Reverb"},
    {FilterType::Downsample, "Downsample"},
}};

/// @brief Every `DownsampleMode`, for `downsampleModeCombo_` - real-world
/// testing pass, 2026-09-20, finding #18.
constexpr std::array<std::pair<DownsampleMode, const char*>, 2> kDownsampleModes{{
    {DownsampleMode::BlockHold, "Block Hold"},
    {DownsampleMode::BlockAverage, "Block Average"},
}};

}  // namespace

FilterConfigurationPanel::FilterConfigurationPanel(QWidget* parent)
    : QDockWidget(tr("Filter Configuration"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    // The whole-FilterConfiguration counterpart to ToolConfigurationPanel's
    // own Tool Preset row - confirmed with the user ("make filter
    // configuration save-able to a filter preset, just like tool
    // configurations are save-able to tool preset").
    auto* presetRow = new QHBoxLayout();
    filterPresetCombo_ = new QComboBox(container);
    filterPresetCombo_->setObjectName(QStringLiteral("filterPresetCombo"));
    filterPresetCombo_->setToolTip(tr("Load a saved Filter Preset"));
    filterPresetCombo_->addItem(tr("Load Preset..."));
    connect(filterPresetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index <= 0 || static_cast<std::size_t>(index - 1) >= availableFilterPresets_.size()) {
            return;
        }
        const auto& chosen = availableFilterPresets_[static_cast<std::size_t>(index - 1)];
        setFilterConfiguration(chosen.config);
        emitConfigChanged();
        presetIndicator_->setActivePreset(QString::fromStdString(chosen.name));
        // One-shot trigger, not a sticky selection - the same
        // "immediately apply, then reset to the placeholder" contract
        // convolveLoadKernelCombo_ already establishes.
        const QSignalBlocker blocker(filterPresetCombo_);
        filterPresetCombo_->setCurrentIndex(0);
    });
    presetRow->addWidget(filterPresetCombo_, 1);

    savePresetButton_ = new QPushButton(tr("Save..."), container);
    savePresetButton_->setObjectName(QStringLiteral("savePresetButton"));
    savePresetButton_->setToolTip(tr("Save the current configuration as a new Filter Preset"));
    connect(savePresetButton_, &QPushButton::clicked, this,
            [this]() { emit savePresetRequested(config_); });
    presetRow->addWidget(savePresetButton_);

    auto* presetForm = new QFormLayout();
    presetForm->addRow(tr("Filter Preset:"), presetRow);
    root->addLayout(presetForm);

    presetIndicator_ = new PresetIndicatorLabel(container);
    root->addWidget(presetIndicator_);

    // Pre-Beta Plan v0.Y.76.1: hear the configuration while tuning it - see
    // previewToggled()'s docs.
    previewButton_ = new QPushButton(tr("Loop Preview"), container);
    previewButton_->setObjectName(QStringLiteral("filterPreviewButton"));
    previewButton_->setCheckable(true);
    previewButton_->setToolTip(tr("Loop a few seconds (the selection, or from the playback position) through this "
                                  "filter, updating as you change it"));
    connect(previewButton_, &QPushButton::toggled, this, &FilterConfigurationPanel::previewToggled);
    root->addWidget(previewButton_);

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

    // Real-world testing pass, 2026-09-20, finding #17: a real, shared
    // GradientEditorWidget replaces the old two-endpoint-only Start/End
    // spin-box groups - also reused directly for the Equalizer's own Cut
    // mode (see setEqualizerMode()'s own docs), rather than a second,
    // separate widget mirroring the same config_.frequencyGradient().
    auto* frequencyGroupContainer = new QWidget(container);
    frequencyGroupContainer->setObjectName(QStringLiteral("frequencyAxisGradientSection"));
    auto* frequencyLayout = new QVBoxLayout(frequencyGroupContainer);
    frequencyLayout->setContentsMargins(0, 0, 0, 0);
    frequencyAxisGradientSection_ = frequencyGroupContainer;

    frequencyGradientLabel_ = new QLabel(tr("Frequency-Axis Gradient"), frequencyGroupContainer);
    frequencyGradientLabel_->setObjectName(QStringLiteral("frequencyGradientLabel"));
    frequencyLayout->addWidget(frequencyGradientLabel_);

    frequencyGradientEditor_ = new GradientEditorWidget(frequencyGroupContainer);
    frequencyGradientEditor_->setObjectName(QStringLiteral("frequencyGradientEditor"));
    connect(frequencyGradientEditor_, &GradientEditorWidget::gradientChanged, this,
            [this](const sound_mind::core::Gradient& gradient) {
                config_.frequencyGradient() = gradient;
                // Keeps the vertical curve view in sync with every edit
                // made through the numeric spin boxes - setGradient()
                // doesn't itself emit gradientChanged(), so this can't
                // loop back - see EqualizerCurveWidget's own docs.
                equalizerCurveWidget_->setGradient(gradient);
                emitConfigChanged();
            });
    frequencyLayout->addWidget(frequencyGradientEditor_);

    // Equalizer-mode-only - see this class's own docs. Lives in the same
    // container/section as frequencyGradientEditor_ above (both edit the
    // same config_.frequencyGradient()), but its own visibility is gated
    // separately in updateVisibleGroup(), on isEqualizerMode_ alone, not
    // "isEqualizerMode_ || type == FrequencyAxisGradient" the way the rest
    // of this section is - a plain (non-Equalizer) Frequency-Axis Gradient
    // Filter layer still gets only the numeric editor, unchanged.
    equalizerCurveWidget_ = new EqualizerCurveWidget(frequencyGroupContainer);
    equalizerCurveWidget_->setObjectName(QStringLiteral("equalizerCurveWidget"));
    connect(equalizerCurveWidget_, &EqualizerCurveWidget::gradientChanged, this,
            [this](const sound_mind::core::Gradient& gradient) {
                config_.frequencyGradient() = gradient;
                frequencyGradientEditor_->setGradient(gradient);
                emitConfigChanged();
            });
    frequencyLayout->addWidget(equalizerCurveWidget_);

    equalizerPreviewCheckBox_ = new QCheckBox(tr("Preview on Canvas"), frequencyGroupContainer);
    equalizerPreviewCheckBox_->setObjectName(QStringLiteral("equalizerPreviewCheckBox"));
    equalizerPreviewCheckBox_->setToolTip(
        tr("Overlay this curve's own effect on the spectrogram - darker means more cut"));
    connect(equalizerPreviewCheckBox_, &QCheckBox::toggled, this,
            &FilterConfigurationPanel::equalizerPreviewToggled);
    frequencyLayout->addWidget(equalizerPreviewCheckBox_);

    root->addWidget(frequencyGroupContainer);

    uniformBlurGroup_ = new QGroupBox(tr("Uniform Blur"), container);
    uniformBlurGroup_->setObjectName(QStringLiteral("uniformBlurGroup"));
    auto* uniformBlurForm = new QFormLayout(uniformBlurGroup_);
    addParameterRow(uniformBlurForm, tr("Sigma:"), makeParameterField(uniformBlurGroup_, "blurSigma"));
    root->addWidget(uniformBlurGroup_);

    edgePreservingBlurGroup_ = new QGroupBox(tr("Edge-Preserving Blur"), container);
    edgePreservingBlurGroup_->setObjectName(QStringLiteral("edgePreservingBlurGroup"));
    auto* edgePreservingBlurForm = new QFormLayout(edgePreservingBlurGroup_);
    addParameterRow(edgePreservingBlurForm, tr("Size:"), makeParameterField(edgePreservingBlurGroup_, "medianSize"));
    root->addWidget(edgePreservingBlurGroup_);

    directionalBlurGroup_ = new QGroupBox(tr("Directional Blur"), container);
    directionalBlurGroup_->setObjectName(QStringLiteral("directionalBlurGroup"));
    auto* directionalBlurForm = new QFormLayout(directionalBlurGroup_);
    addParameterRow(directionalBlurForm, tr("Length:"),
                    makeParameterField(directionalBlurGroup_, "directionalBlurLength"));
    addParameterRow(directionalBlurForm, tr("Angle:"),
                    makeParameterField(directionalBlurGroup_, "directionalBlurAngle"));
    root->addWidget(directionalBlurGroup_);

    sharpenGroup_ = new QGroupBox(tr("Sharpen"), container);
    sharpenGroup_->setObjectName(QStringLiteral("sharpenGroup"));
    auto* sharpenForm = new QFormLayout(sharpenGroup_);
    addParameterRow(sharpenForm, tr("Amount:"), makeParameterField(sharpenGroup_, "sharpenAmount"));
    root->addWidget(sharpenGroup_);

    speckleAddGroup_ = new QGroupBox(tr("Speckle Add"), container);
    speckleAddGroup_->setObjectName(QStringLiteral("speckleAddGroup"));
    auto* speckleAddForm = new QFormLayout(speckleAddGroup_);
    addParameterRow(speckleAddForm, tr("Density:"), makeParameterField(speckleAddGroup_, "speckleAddDensity"));
    addParameterRow(speckleAddForm, tr("Intensity:"), makeParameterField(speckleAddGroup_, "speckleAddIntensity"));
    speckleAddGroup_->setToolTip(
        tr("Deterministic per Filter layer - the same pattern every recomposite, only changing if you touch these "
           "controls or the content underneath."));
    root->addWidget(speckleAddGroup_);

    speckleRemoveGroup_ = new QGroupBox(tr("Speckle Remove"), container);
    speckleRemoveGroup_->setObjectName(QStringLiteral("speckleRemoveGroup"));
    auto* speckleRemoveForm = new QFormLayout(speckleRemoveGroup_);
    addParameterRow(speckleRemoveForm, tr("Threshold:"), makeParameterField(speckleRemoveGroup_, "speckleThreshold"));
    root->addWidget(speckleRemoveGroup_);

    denoiseGroup_ = new QGroupBox(tr("Denoise"), container);
    denoiseGroup_->setObjectName(QStringLiteral("denoiseGroup"));
    auto* denoiseForm = new QFormLayout(denoiseGroup_);
    const ParameterField noiseFloorField = makeParameterField(denoiseGroup_, "noiseFloor");
    auto* noiseFloorUnitSelector = new IntensityUnitSelector(denoiseGroup_);
    noiseFloorUnitSelector->addField(noiseFloorField.real);
    denoiseForm->addRow(tr("Noise floor in:"), noiseFloorUnitSelector);
    addParameterRow(denoiseForm, tr("Noise Floor:"), noiseFloorField);
    addParameterRow(denoiseForm, tr("Reduction:"), makeParameterField(denoiseGroup_, "reduction"));
    root->addWidget(denoiseGroup_);

    bitDepthCrushGroup_ = new QGroupBox(tr("Bit-Depth Crush"), container);
    bitDepthCrushGroup_->setObjectName(QStringLiteral("bitDepthCrushGroup"));
    auto* bitDepthCrushForm = new QFormLayout(bitDepthCrushGroup_);
    addParameterRow(bitDepthCrushForm, tr("Amount:"), makeParameterField(bitDepthCrushGroup_, "crushAmount"));
    root->addWidget(bitDepthCrushGroup_);

    granularNoiseGroup_ = new QGroupBox(tr("Granular Noise"), container);
    granularNoiseGroup_->setObjectName(QStringLiteral("granularNoiseGroup"));
    auto* granularNoiseForm = new QFormLayout(granularNoiseGroup_);
    addParameterRow(granularNoiseForm, tr("Grain Size:"), makeParameterField(granularNoiseGroup_, "grainSize"));
    addParameterRow(granularNoiseForm, tr("Grain Amount:"), makeParameterField(granularNoiseGroup_, "grainAmount"));
    granularNoiseGroup_->setToolTip(
        tr("Deterministic per Filter layer - the same pattern every recomposite, only changing if you touch these "
           "controls or the content underneath."));
    root->addWidget(granularNoiseGroup_);

    dynamicSpeckleGroup_ = new QGroupBox(tr("Dynamic Speckle"), container);
    dynamicSpeckleGroup_->setObjectName(QStringLiteral("dynamicSpeckleGroup"));
    auto* dynamicSpeckleForm = new QFormLayout(dynamicSpeckleGroup_);
    addParameterRow(dynamicSpeckleForm, tr("Density:"),
                    makeParameterField(dynamicSpeckleGroup_, "dynamicSpeckleDensity"));
    addParameterRow(dynamicSpeckleForm, tr("Intensity:"),
                    makeParameterField(dynamicSpeckleGroup_, "dynamicSpeckleIntensity"));
    dynamicSpeckleGroup_->setToolTip(
        tr("Live - freshly re-randomized on every recomposite (any edit, scroll, or repaint), computed over fixed "
           "2x2 blocks to stay cheap on a large canvas."));
    root->addWidget(dynamicSpeckleGroup_);

    feedbackDistortionGroup_ = new QGroupBox(tr("Feedback Distortion"), container);
    feedbackDistortionGroup_->setObjectName(QStringLiteral("feedbackDistortionGroup"));
    auto* feedbackDistortionForm = new QFormLayout(feedbackDistortionGroup_);
    addParameterRow(feedbackDistortionForm, tr("Amount:"),
                    makeParameterField(feedbackDistortionGroup_, "feedbackAmount"));
    root->addWidget(feedbackDistortionGroup_);

    spectralWavefoldGroup_ = new QGroupBox(tr("Spectral Wavefold"), container);
    spectralWavefoldGroup_->setObjectName(QStringLiteral("spectralWavefoldGroup"));
    auto* spectralWavefoldForm = new QFormLayout(spectralWavefoldGroup_);
    addParameterRow(spectralWavefoldForm, tr("Fold Gain:"), makeParameterField(spectralWavefoldGroup_, "foldGain"));
    root->addWidget(spectralWavefoldGroup_);

    displaceGroup_ = new QGroupBox(tr("Displace"), container);
    displaceGroup_->setObjectName(QStringLiteral("displaceGroup"));
    auto* displaceForm = new QFormLayout(displaceGroup_);
    addParameterRow(displaceForm, tr("Distance:"), makeParameterField(displaceGroup_, "displaceDistance"));
    addParameterRow(displaceForm, tr("Angle:"), makeParameterField(displaceGroup_, "displaceAngle"));
    root->addWidget(displaceGroup_);

    channelCycleGroup_ = new QGroupBox(tr("Channel Cycle"), container);
    channelCycleGroup_->setObjectName(QStringLiteral("channelCycleGroup"));
    auto* channelCycleForm = new QFormLayout(channelCycleGroup_);
    addParameterRow(channelCycleForm, tr("Angle:"), makeParameterField(channelCycleGroup_, "channelCycleAngle"));
    root->addWidget(channelCycleGroup_);

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

    channelBalanceGroup_ = new QGroupBox(tr("Channel Balance"), container);
    channelBalanceGroup_->setObjectName(QStringLiteral("channelBalanceGroup"));
    auto* channelBalanceForm = new QFormLayout(channelBalanceGroup_);
    addParameterRow(channelBalanceForm, tr("Balance:"), makeParameterField(channelBalanceGroup_, "channelBalance"));
    root->addWidget(channelBalanceGroup_);

    invertGroup_ = new QGroupBox(tr("Invert"), container);
    invertGroup_->setObjectName(QStringLiteral("invertGroup"));
    auto* invertLayout = new QVBoxLayout(invertGroup_);
    auto* invertLabel = new QLabel(
        tr("Inverts loudness: quiet becomes loud, loud becomes quiet. No parameters."), invertGroup_);
    invertLabel->setWordWrap(true);
    invertLayout->addWidget(invertLabel);
    root->addWidget(invertGroup_);

    convolveGroup_ = new QGroupBox(tr("Convolve"), container);
    convolveGroup_->setObjectName(QStringLiteral("convolveGroup"));
    auto* convolveLayout = new QVBoxLayout(convolveGroup_);

    auto* convolveTopForm = new QFormLayout();
    convolvePresetCombo_ = new QComboBox(convolveGroup_);
    convolvePresetCombo_->setObjectName(QStringLiteral("convolvePresetCombo"));
    convolvePresetCombo_->addItem(tr("-- Presets --"));
    for (const auto& preset : kConvolvePresets) {
        convolvePresetCombo_->addItem(tr(preset.name));
    }
    connect(convolvePresetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index <= 0) {
            return;
        }
        const auto& preset = kConvolvePresets[static_cast<std::size_t>(index - 1)];
        config_.setConvolveKernelSize(3);
        config_.setConvolveKernel(std::vector<float>(preset.kernel.begin(), preset.kernel.end()));
        config_.setConvolveNormalize(preset.normalize);
        const QSignalBlocker sizeBlocker(convolveKernelSizeSpinBox_);
        convolveKernelSizeSpinBox_->setValue(3);
        rebuildConvolveKernelGrid(3);
        const QSignalBlocker normalizeBlocker(convolveNormalizeCheckBox_);
        convolveNormalizeCheckBox_->setChecked(preset.normalize);
        emitConfigChanged();
        // A one-shot trigger, not a sticky selection - see this class's
        // own docs.
        const QSignalBlocker comboBlocker(convolvePresetCombo_);
        convolvePresetCombo_->setCurrentIndex(0);
    });
    convolveTopForm->addRow(tr("Preset:"), convolvePresetCombo_);

    convolveKernelSizeSpinBox_ = new IntegerField(convolveGroup_);
    convolveKernelSizeSpinBox_->setObjectName(QStringLiteral("convolveKernelSizeSpinBox"));
    convolveKernelSizeSpinBox_->setRange(3, 21);
    convolveKernelSizeSpinBox_->setSingleStep(2);
    convolveKernelSizeSpinBox_->setToolTip(
        tr("Always odd - an even value is rounded up. Changing this replaces the current kernel with a fresh "
           "identity kernel of the new size."));
    connect(convolveKernelSizeSpinBox_, &IntegerField::valueChanged, this, [this](int value) {
        const int size = std::max(3, value | 1);
        if (size != value) {
            const QSignalBlocker blocker(convolveKernelSizeSpinBox_);
            convolveKernelSizeSpinBox_->setValue(size);
        }
        std::vector<float> identity(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), 0.0f);
        identity[static_cast<std::size_t>(size / 2) * static_cast<std::size_t>(size) +
                  static_cast<std::size_t>(size / 2)] = 1.0f;
        config_.setConvolveKernelSize(size);
        config_.setConvolveKernel(identity);
        rebuildConvolveKernelGrid(size);
        emitConfigChanged();
    });
    convolveTopForm->addRow(tr("Kernel Size:"), convolveKernelSizeSpinBox_);
    convolveLayout->addLayout(convolveTopForm);

    convolveKernelGridContainer_ = new QWidget(convolveGroup_);
    convolveKernelGridContainer_->setObjectName(QStringLiteral("convolveKernelGridContainer"));
    convolveKernelGridLayout_ = new QGridLayout(convolveKernelGridContainer_);
    convolveLayout->addWidget(convolveKernelGridContainer_);

    convolveNormalizeCheckBox_ = new QCheckBox(tr("Normalize"), convolveGroup_);
    convolveNormalizeCheckBox_->setObjectName(QStringLiteral("convolveNormalizeCheckBox"));
    convolveNormalizeCheckBox_->setToolTip(
        tr("Divides the kernel by the sum of its own positive coefficients first, so a pure-positive kernel "
           "(a blur) doesn't brighten or darken the whole image."));
    connect(convolveNormalizeCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        config_.setConvolveNormalize(checked);
        emitConfigChanged();
    });
    convolveLayout->addWidget(convolveNormalizeCheckBox_);

    auto* convolveBottomForm = new QFormLayout();
    addParameterRow(convolveBottomForm, tr("Amount:"), makeParameterField(convolveGroup_, "convolveAmount"));
    convolveLayout->addLayout(convolveBottomForm);

    auto* convolveLibraryRow = new QHBoxLayout();
    convolveSaveKernelButton_ = new QPushButton(tr("Save As New Kernel"), convolveGroup_);
    convolveSaveKernelButton_->setObjectName(QStringLiteral("convolveSaveKernelButton"));
    connect(convolveSaveKernelButton_, &QPushButton::clicked, this, [this]() {
        emit saveConvolutionKernelRequested(config_.convolveKernelSize(), config_.convolveKernel(),
                                              config_.convolveNormalize());
    });
    convolveLibraryRow->addWidget(convolveSaveKernelButton_);
    convolveLoadKernelCombo_ = new QComboBox(convolveGroup_);
    convolveLoadKernelCombo_->setObjectName(QStringLiteral("convolveLoadKernelCombo"));
    convolveLoadKernelCombo_->addItem(tr("-- Load Saved Kernel --"), QVariant::fromValue(qulonglong{0}));
    connect(convolveLoadKernelCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index <= 0) {
            return;
        }
        const auto id = static_cast<ConvolutionKernelId>(convolveLoadKernelCombo_->itemData(index).toULongLong());
        const auto it = std::find_if(
            availableConvolutionKernels_.begin(), availableConvolutionKernels_.end(),
            [id](const sound_mind::core::NamedConvolutionKernel& entry) { return entry.id == id; });
        if (it != availableConvolutionKernels_.end()) {
            config_.setConvolveKernelSize(it->size);
            config_.setConvolveKernel(it->coefficients);
            config_.setConvolveNormalize(it->normalize);
            const QSignalBlocker sizeBlocker(convolveKernelSizeSpinBox_);
            convolveKernelSizeSpinBox_->setValue(it->size);
            rebuildConvolveKernelGrid(it->size);
            const QSignalBlocker normalizeBlocker(convolveNormalizeCheckBox_);
            convolveNormalizeCheckBox_->setChecked(it->normalize);
            emitConfigChanged();
        }
        const QSignalBlocker comboBlocker(convolveLoadKernelCombo_);
        convolveLoadKernelCombo_->setCurrentIndex(0);
    });
    convolveLibraryRow->addWidget(convolveLoadKernelCombo_);
    convolveLayout->addLayout(convolveLibraryRow);

    rebuildConvolveKernelGrid(config_.convolveKernelSize());
    root->addWidget(convolveGroup_);

    spectralReverbGroup_ = new QGroupBox(tr("Spectral Reverb"), container);
    spectralReverbGroup_->setObjectName(QStringLiteral("spectralReverbGroup"));
    auto* reverbForm = new QFormLayout(spectralReverbGroup_);
    addParameterRow(reverbForm, tr("Pre-Delay (frames):"), makeParameterField(spectralReverbGroup_, "reverbPreDelay"));
    addParameterRow(reverbForm, tr("Decay (frames):"), makeParameterField(spectralReverbGroup_, "reverbDecay"));
    addParameterRow(reverbForm, tr("Room Size:"), makeParameterField(spectralReverbGroup_, "reverbRoomSize"));
    addParameterRow(reverbForm, tr("Diffusion:"), makeParameterField(spectralReverbGroup_, "reverbDiffusion"));
    addParameterRow(reverbForm, tr("Absorption:"), makeParameterField(spectralReverbGroup_, "reverbAbsorption"));
    addParameterRow(reverbForm, tr("Mix:"), makeParameterField(spectralReverbGroup_, "reverbMix"));
    root->addWidget(spectralReverbGroup_);

    downsampleGroup_ = new QGroupBox(tr("Downsample"), container);
    downsampleGroup_->setObjectName(QStringLiteral("downsampleGroup"));
    auto* downsampleForm = new QFormLayout(downsampleGroup_);
    downsampleModeCombo_ = new QComboBox(downsampleGroup_);
    downsampleModeCombo_->setObjectName(QStringLiteral("downsampleModeCombo"));
    for (const auto& [mode, name] : kDownsampleModes) {
        downsampleModeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
    }
    connect(downsampleModeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        config_.setDownsampleMode(static_cast<DownsampleMode>(downsampleModeCombo_->itemData(index).toInt()));
        emitConfigChanged();
    });
    downsampleForm->addRow(tr("Mode:"), downsampleModeCombo_);
    addParameterRow(downsampleForm, tr("Block Size:"), makeParameterField(downsampleGroup_, "downsampleBlockSize"));
    root->addWidget(downsampleGroup_);

    root->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);

    rebuildMindWaveCombos();
    updateVisibleGroup();
}

void FilterConfigurationPanel::emitConfigChanged() {
    presetIndicator_->markModified();
    emit filterConfigurationChanged(config_);
}

void FilterConfigurationPanel::setPreviewPlaying(bool playing) {
    const QSignalBlocker blocker(previewButton_);
    previewButton_->setChecked(playing);
}

void FilterConfigurationPanel::setActivePresetName(const QString& name) { presetIndicator_->setActivePreset(name); }

void FilterConfigurationPanel::updateVisibleGroup() {
    filterTypeCombo_->setVisible(!isEqualizerMode_);
    if (filterTypeLabel_ != nullptr) {
        filterTypeLabel_->setVisible(!isEqualizerMode_);
    }
    frequencyGradientEditor_->setCutMode(isEqualizerMode_);
    equalizerCurveWidget_->setVisible(isEqualizerMode_);
    equalizerPreviewCheckBox_->setVisible(isEqualizerMode_);

    // In Equalizer mode, frequencyAxisGradientSection_ (now in Cut mode)
    // is the only one shown - config_.type() is always
    // FrequencyAxisGradient for the Equalizer anyway (it's never switched
    // away, since the combo above is hidden), but every per-type group
    // stays hidden regardless of type() while this mode is active.
    const FilterType type = config_.type();
    frequencyAxisGradientSection_->setVisible(isEqualizerMode_ || type == FilterType::FrequencyAxisGradient);
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
    displaceGroup_->setVisible(!isEqualizerMode_ && type == FilterType::Displace);
    channelCycleGroup_->setVisible(!isEqualizerMode_ && type == FilterType::ChannelCycle);
    spectralReverbGroup_->setVisible(!isEqualizerMode_ && type == FilterType::SpectralReverb);
    toneCurveGroup_->setVisible(!isEqualizerMode_ && type == FilterType::ToneCurve);
    channelBalanceGroup_->setVisible(!isEqualizerMode_ && type == FilterType::ChannelBalance);
    invertGroup_->setVisible(!isEqualizerMode_ && type == FilterType::Invert);
    convolveGroup_->setVisible(!isEqualizerMode_ && type == FilterType::Convolve);
    downsampleGroup_->setVisible(!isEqualizerMode_ && type == FilterType::Downsample);
}

void FilterConfigurationPanel::setEqualizerMode(bool isEqualizer) {
    isEqualizerMode_ = isEqualizer;
    updateVisibleGroup();
}

void FilterConfigurationPanel::setProjectSettings(std::optional<sound_mind::core::ProjectSettings> settings) {
    equalizerCurveWidget_->setProjectSettings(std::move(settings));
}

void FilterConfigurationPanel::setEqualizerPreviewChecked(bool checked) {
    const QSignalBlocker blocker(equalizerPreviewCheckBox_);
    equalizerPreviewCheckBox_->setChecked(checked);
}

void FilterConfigurationPanel::setFilterConfiguration(const sound_mind::core::FilterConfiguration& config) {
    config_ = config;
    presetIndicator_->clearActivePreset();

    // QSignalBlocker on every control - see ToolConfigurationPanel::
    // setToolConfiguration()'s own docs for why: only the *display*
    // should change here, not re-trigger each control's own change
    // handler (which would both redundantly re-set config_ to the value
    // it already has and spuriously emit filterConfigurationChanged()).
    const QSignalBlocker filterTypeBlocker(filterTypeCombo_);
    const QSignalBlocker convolveKernelSizeBlocker(convolveKernelSizeSpinBox_);
    const QSignalBlocker convolveNormalizeBlocker(convolveNormalizeCheckBox_);
    const QSignalBlocker downsampleModeBlocker(downsampleModeCombo_);

    // Every FilterType is selectable now - findData() only ever falls
    // back to index 0 here for a corrupted/out-of-range stored value
    // (hand-edited project JSON), the same defensive fallback
    // ToolConfigurationPanel's own combos already establish, without
    // mutating config_.type() itself.
    const int typeIndex = filterTypeCombo_->findData(QVariant::fromValue(static_cast<int>(config_.type())));
    filterTypeCombo_->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);

    // GradientEditorWidget::setGradient() doesn't emit on its own (see its
    // own docs) - no separate QSignalBlocker needed here, matching
    // rebuildMindWaveCombos()'s own reasoning for the same thing.
    frequencyGradientEditor_->setGradient(config_.frequencyGradient());
    equalizerCurveWidget_->setGradient(config_.frequencyGradient());

    // Every numeric field, from the parameter table (v0.1.48.10) - shared
    // parameters (SpeckleAdd/DynamicSpeckle) show the same value in each.
    refreshParameterFields();
    convolveKernelSizeSpinBox_->setValue(config_.convolveKernelSize());
    convolveNormalizeCheckBox_->setChecked(config_.convolveNormalize());
    const int downsampleModeIndex =
        downsampleModeCombo_->findData(QVariant::fromValue(static_cast<int>(config_.downsampleMode())));
    downsampleModeCombo_->setCurrentIndex(downsampleModeIndex >= 0 ? downsampleModeIndex : 0);
    // rebuildConvolveKernelGrid() reads the freshly-loaded config_.convolveKernel()
    // itself when its own length already matches the new size - see its
    // own docs - so this always ends up showing the loaded kernel's own
    // actual values, not a reset-to-identity.
    rebuildConvolveKernelGrid(config_.convolveKernelSize());
    // The Preset/Load Kernel combos have no "current selection" to
    // restore - see this class's own docs on why both are one-shot
    // triggers, already reset to their own placeholder after every use.
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

void FilterConfigurationPanel::setCreateMindWaveCallback(
    std::function<std::pair<MindWaveId, QString>()> callback) {
    createMindWaveCallback_ = std::move(callback);
}

QComboBox* FilterConfigurationPanel::makeMindWaveCombo(QWidget* parent, const QString& objectName) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName(objectName);
    combo->setToolTip(tr("Bind this parameter to a MindWave"));
    connect(combo, &QComboBox::currentIndexChanged, this,
            [this, combo](int index) { resolveCreateMindWaveSentinel(combo, index); });
    return combo;
}

void FilterConfigurationPanel::resolveCreateMindWaveSentinel(QComboBox* combo, int index) {
    if (combo->itemData(index).toULongLong() != kCreateNewMindWaveSentinel) {
        return;
    }
    if (!createMindWaveCallback_) {
        return;
    }
    const auto [newId, newName] = createMindWaveCallback_();
    if (newId == MindWaveId{0}) {
        return;  // No project set - see MindWaveController::addMindWave()'s own docs.
    }
    // The callback normally triggers setAvailableMindWaves(), which has
    // already rebuilt this combo around the new entry (and reset it to the
    // config's still-unbound "None") - so look the new entry up and select
    // it rather than assuming the sentinel item is still current. Only when
    // no rebuild happened is the sentinel itself rewritten in place.
    const QSignalBlocker blocker(combo);
    int target = combo->findData(QVariant::fromValue(static_cast<qulonglong>(newId)));
    if (target < 0) {
        combo->setItemData(index, QVariant::fromValue(static_cast<qulonglong>(newId)));
        combo->setItemText(index, newName);
        target = index;
    }
    combo->setCurrentIndex(target);
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
        // Always last, after every real entry - v0.Y.62.1 Installment G.
        combo->addItem(tr("Create New MindWave..."), QVariant::fromValue(kCreateNewMindWaveSentinel));
        combo->setCurrentIndex(selectedIndex);
    };
    for (const ParameterField& field : parameterFields_) {
        if (field.mindWaveCombo != nullptr) {
            populate(field.mindWaveCombo, config_.parameterMindWave(field.parameter));
        }
    }
}

QStringList FilterConfigurationPanel::parameterFieldStems(FilterParameter parameter) {
    QStringList stems;
    for (const ParameterFieldLook& look : kParameterFieldLooks) {
        if (look.parameter == parameter) {
            stems.append(QString::fromLatin1(look.stem));
        }
    }
    return stems;
}

FilterConfigurationPanel::ParameterField FilterConfigurationPanel::makeParameterField(QWidget* group,
                                                                                      std::string_view stem) {
    const ParameterFieldLook& look = parameterFieldLook(stem);
    const sound_mind::core::FilterParameterSpec& spec = sound_mind::core::filterParameterSpec(look.parameter);
    const QString objectStem = QString::fromLatin1(stem.data(), static_cast<qsizetype>(stem.size()));
    ParameterField field{look.parameter, nullptr, nullptr, nullptr};
    QWidget* widget = nullptr;
    if (spec.integer) {
        field.whole = new IntegerField(group);
        field.whole->setRange(static_cast<int>(spec.minimum), static_cast<int>(spec.maximum));
        if (look.step > 0.0) {
            field.whole->setSingleStep(static_cast<int>(look.step));
        }
        connect(field.whole, &IntegerField::valueChanged, this, [this, parameter = look.parameter,
                                                                 source = field.whole](int value) {
            parameterEdited(parameter, static_cast<double>(value), source);
        });
        widget = field.whole;
    } else {
        field.real = new NumericField(group);
        field.real->setRange(spec.minimum, spec.maximum);
        if (look.step > 0.0) {
            field.real->setSingleStep(look.step);
        }
        if (look.decimals >= 0) {
            field.real->setDecimals(look.decimals);
        }
        if (look.suffix != nullptr) {
            field.real->setSuffix(QString::fromUtf8(look.suffix));
        }
        connect(field.real, &NumericField::valueChanged, this,
                [this, parameter = look.parameter, source = field.real](double value) {
                    parameterEdited(parameter, value, source);
                });
        widget = field.real;
    }
    widget->setObjectName(objectStem + QStringLiteral("SpinBox"));
    if (look.toolTip != nullptr) {
        widget->setToolTip(tr(look.toolTip));
    }
    if (spec.mindWaveKey != nullptr) {
        field.mindWaveCombo = makeMindWaveCombo(group, objectStem + QStringLiteral("MindWaveCombo"));
        connect(field.mindWaveCombo, &QComboBox::currentIndexChanged, this,
                [this, parameter = look.parameter, combo = field.mindWaveCombo](int index) {
                    parameterMindWaveChosen(parameter, combo, index);
                });
    }
    parameterFields_.push_back(field);
    return field;
}

void FilterConfigurationPanel::addParameterRow(QFormLayout* form, const QString& label, const ParameterField& field) {
    QWidget* widget = field.real != nullptr ? static_cast<QWidget*>(field.real) : field.whole;
    if (field.mindWaveCombo != nullptr) {
        form->addRow(label, makeBoundFieldRow(widget, field.mindWaveCombo));
    } else {
        form->addRow(label, widget);
    }
}

void FilterConfigurationPanel::parameterEdited(FilterParameter parameter, double value, QWidget* source) {
    config_.setParameter(parameter, value);
    // Another field editing the same shared parameter (SpeckleAdd's and
    // DynamicSpeckle's) shows the new value too.
    for (const ParameterField& field : parameterFields_) {
        if (field.parameter != parameter || field.real == source || field.whole == source) {
            continue;
        }
        if (field.real != nullptr) {
            const QSignalBlocker blocker(field.real);
            field.real->setValue(value);
        } else {
            const QSignalBlocker blocker(field.whole);
            field.whole->setValue(static_cast<int>(value));
        }
    }
    emitConfigChanged();
}

void FilterConfigurationPanel::parameterMindWaveChosen(FilterParameter parameter, QComboBox* source, int index) {
    const auto rawId = source->itemData(index).toULongLong();
    config_.setParameterMindWave(parameter,
                                 rawId == 0 ? std::nullopt : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
    for (const ParameterField& field : parameterFields_) {
        if (field.parameter == parameter && field.mindWaveCombo != nullptr && field.mindWaveCombo != source) {
            const QSignalBlocker blocker(field.mindWaveCombo);
            const int siblingIndex = field.mindWaveCombo->findData(QVariant::fromValue(rawId));
            field.mindWaveCombo->setCurrentIndex(siblingIndex >= 0 ? siblingIndex : 0);
        }
    }
    emitConfigChanged();
}

void FilterConfigurationPanel::refreshParameterFields() {
    for (const ParameterField& field : parameterFields_) {
        const double value = config_.parameter(field.parameter);
        if (field.real != nullptr) {
            const QSignalBlocker blocker(field.real);
            field.real->setValue(value);
        } else {
            const QSignalBlocker blocker(field.whole);
            field.whole->setValue(static_cast<int>(value));
        }
    }
}

void FilterConfigurationPanel::setAvailableConvolutionKernels(
    const std::vector<sound_mind::core::NamedConvolutionKernel>& kernels) {
    availableConvolutionKernels_ = kernels;
    const QSignalBlocker blocker(convolveLoadKernelCombo_);
    convolveLoadKernelCombo_->clear();
    convolveLoadKernelCombo_->addItem(tr("-- Load Saved Kernel --"), QVariant::fromValue(qulonglong{0}));
    for (const auto& kernel : availableConvolutionKernels_) {
        convolveLoadKernelCombo_->addItem(QString::fromStdString(kernel.name),
                                           QVariant::fromValue(static_cast<qulonglong>(kernel.id)));
    }
    convolveLoadKernelCombo_->setCurrentIndex(0);
}

void FilterConfigurationPanel::setAvailableFilterPresets(
    const std::vector<sound_mind::core::NamedFilterPreset>& presets) {
    availableFilterPresets_ = presets;
    if (presetIndicator_->hasActivePreset() &&
        std::none_of(presets.begin(), presets.end(), [this](const auto& preset) {
            return QString::fromStdString(preset.name) == presetIndicator_->activePresetName();
        })) {
        presetIndicator_->clearActivePreset();
    }
    const QSignalBlocker blocker(filterPresetCombo_);
    filterPresetCombo_->clear();
    filterPresetCombo_->addItem(tr("Load Preset..."));
    for (const auto& preset : availableFilterPresets_) {
        filterPresetCombo_->addItem(QString::fromStdString(preset.name));
    }
    filterPresetCombo_->setCurrentIndex(0);
}

void FilterConfigurationPanel::rebuildConvolveKernelGrid(int size) {
    // delete, not deleteLater() - a deferred delete would leave the old
    // widgets alive (with the same object names as their replacements)
    // until the next event loop iteration, so an immediate findChild()
    // lookup (as every test here does) would find the stale one instead.
    for (NumericField* spinBox : convolveKernelSpinBoxes_) {
        convolveKernelGridLayout_->removeWidget(spinBox);
        delete spinBox;
    }
    convolveKernelSpinBoxes_.clear();

    const auto& currentKernel = config_.convolveKernel();
    const bool reuseCurrentValues =
        currentKernel.size() == static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
    const int center = size / 2;
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const std::size_t cell = static_cast<std::size_t>(row) * static_cast<std::size_t>(size) +
                                       static_cast<std::size_t>(col);
            auto* spinBox = new NumericField(convolveKernelGridContainer_);
            spinBox->setObjectName(QStringLiteral("convolveKernelSpinBox_%1_%2").arg(row).arg(col));
            spinBox->setRange(-20.0, 20.0);
            spinBox->setSingleStep(0.1);
            spinBox->setDecimals(2);
            const QSignalBlocker blocker(spinBox);
            spinBox->setValue(reuseCurrentValues ? currentKernel[cell] : (row == center && col == center ? 1.0 : 0.0));
            connect(spinBox, &NumericField::valueChanged, this,
                    [this](double) { applyConvolveKernelFromGrid(); });
            convolveKernelGridLayout_->addWidget(spinBox, row, col);
            convolveKernelSpinBoxes_.push_back(spinBox);
        }
    }
}

void FilterConfigurationPanel::applyConvolveKernelFromGrid() {
    std::vector<float> kernel;
    kernel.reserve(convolveKernelSpinBoxes_.size());
    for (NumericField* spinBox : convolveKernelSpinBoxes_) {
        kernel.push_back(static_cast<float>(spinBox->value()));
    }
    config_.setConvolveKernel(std::move(kernel));
    emitConfigChanged();
}

}  // namespace sound_mind::studio
