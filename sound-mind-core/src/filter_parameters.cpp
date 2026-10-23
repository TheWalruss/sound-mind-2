#include "sound_mind/core/filter_parameters.h"

namespace sound_mind::core {

namespace {

using P = FilterParameter;

// One row per FilterParameter, in enum order (a test checks it). Ranges are
// the Filter Configuration panel's own.
constexpr std::array<FilterParameterSpec, kFilterParameterCount> kSpecs{{
    {P::BlurSigma, "blurSigma", "blurSigmaMindWaveId", 0.1, 50.0, false},
    {P::MedianSize, "medianSize", "medianSizeMindWaveId", 3.0, 31.0, true},
    {P::DirectionalBlurLength, "directionalBlurLength", "directionalBlurLengthMindWaveId", 1.0, 200.0, true},
    {P::DirectionalBlurAngle, "directionalBlurAngleDegrees", "directionalBlurAngleMindWaveId", 0.0, 360.0, false},
    {P::SharpenAmount, "sharpenAmount", "sharpenAmountMindWaveId", 0.1, 5.0, false},
    {P::SpeckleDensity, "speckleDensity", "speckleDensityMindWaveId", 0.0, 1.0, false},
    {P::SpeckleIntensity, "speckleIntensity", "speckleIntensityMindWaveId", 0.0, 1.0, false},
    {P::SpeckleThreshold, "speckleThresholdDb", "speckleThresholdMindWaveId", 0.0, 96.0, false},
    {P::NoiseFloor, "noiseFloorDb", "noiseFloorMindWaveId", -96.0, 0.0, false},
    {P::Reduction, "reductionDb", "reductionMindWaveId", 0.0, 96.0, false},
    {P::CrushAmount, "crushAmount", "crushAmountMindWaveId", 0.0, 1.0, false},
    {P::GrainSize, "grainSize", nullptr, 1.0, 64.0, true},
    {P::GrainAmount, "grainAmountDb", "grainAmountMindWaveId", 0.0, 50.0, false},
    {P::FeedbackAmount, "feedbackAmount", "feedbackAmountMindWaveId", 0.0, 0.99, false},
    {P::FoldGain, "foldGain", "foldGainMindWaveId", 1.0, 20.0, false},
    {P::ChannelBalance, "channelBalance", "channelBalanceMindWaveId", 0.0, 1.0, false},
    {P::ConvolveKernelSize, "convolveKernelSize", nullptr, 3.0, 21.0, true},
    {P::ConvolveAmount, "convolveAmount", "convolveAmountMindWaveId", 0.0, 1.0, false},
    {P::DisplaceDistance, "displaceDistance", "displaceDistanceMindWaveId", 0.0, 500.0, false},
    {P::DisplaceAngle, "displaceAngleDegrees", "displaceAngleMindWaveId", 0.0, 360.0, false},
    {P::ChannelCycleAngle, "channelCycleAngleDegrees", "channelCycleAngleMindWaveId", 0.0, 360.0, false},
    {P::ReverbPreDelay, "reverbPreDelayFrames", nullptr, 0.0, 200.0, true},
    {P::ReverbDecay, "reverbDecayFrames", nullptr, 1.0, 1000.0, true},
    {P::ReverbRoomSize, "reverbRoomSize", nullptr, 0.0, 1.0, false},
    {P::ReverbDiffusion, "reverbDiffusion", nullptr, 0.0, 1.0, false},
    {P::ReverbAbsorption, "reverbAbsorption", nullptr, 0.0, 1.0, false},
    {P::ReverbMix, "reverbMix", "reverbMixMindWaveId", 0.0, 1.0, false},
    {P::DownsampleBlockSize, "downsampleBlockSize", "downsampleBlockSizeMindWaveId", 1.0, 64.0, true},
}};

}  // namespace

std::span<const FilterParameterSpec, kFilterParameterCount> filterParameterSpecs() noexcept { return kSpecs; }

const FilterParameterSpec& filterParameterSpec(FilterParameter parameter) noexcept {
    return kSpecs[static_cast<std::size_t>(parameter)];
}

}  // namespace sound_mind::core
