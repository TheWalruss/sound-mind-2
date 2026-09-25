#include "sound_mind/core/filter_configuration.h"

namespace sound_mind::core {

namespace {

/// @brief Writes `mindWaveId` under `key`, only if present - the same
/// "only write if present" convention `path.cpp`'s own handleIn/handleOut
/// already establish for an `std::optional` field, shared here rather
/// than repeated five times inline.
void writeOptionalMindWaveId(nlohmann::json& json, const char* key, std::optional<MindWaveId> mindWaveId) {
    if (mindWaveId.has_value()) {
        json[key] = *mindWaveId;
    }
}

/// @brief The inverse of writeOptionalMindWaveId() - `std::nullopt` if
/// `key` is absent (a configuration saved before `v0.Y.31.1` Installment D
/// was never bound anyway).
std::optional<MindWaveId> readOptionalMindWaveId(const nlohmann::json& json, const char* key) {
    return json.contains(key) ? std::optional(json.at(key).get<MindWaveId>()) : std::nullopt;
}

}  // namespace

void to_json(nlohmann::json& json, const FilterConfiguration& config) {
    json = nlohmann::json{{"type", config.type_},
                           {"blurSigma", config.blurSigma_},
                           {"medianSize", config.medianSize_},
                           {"directionalBlurLength", config.directionalBlurLength_},
                           {"directionalBlurAngleDegrees", config.directionalBlurAngleDegrees_},
                           {"sharpenAmount", config.sharpenAmount_},
                           {"toneCurvePoints", config.toneCurvePoints_},
                           {"frequencyGradient", config.frequencyGradient_},
                           {"noiseSeed", config.noiseSeed_},
                           {"speckleDensity", config.speckleDensity_},
                           {"speckleIntensity", config.speckleIntensity_},
                           {"speckleThresholdDb", config.speckleThresholdDb_},
                           {"noiseFloorDb", config.noiseFloorDb_},
                           {"reductionDb", config.reductionDb_},
                           {"crushAmount", config.crushAmount_},
                           {"grainSize", config.grainSize_},
                           {"grainAmountDb", config.grainAmountDb_},
                           {"feedbackAmount", config.feedbackAmount_},
                           {"foldGain", config.foldGain_},
                           {"channelBalance", config.channelBalance_},
                           {"convolveKernel", config.convolveKernel_},
                           {"convolveKernelSize", config.convolveKernelSize_},
                           {"convolveNormalize", config.convolveNormalize_},
                           {"convolveAmount", config.convolveAmount_},
                           {"displaceDistance", config.displaceDistance_},
                           {"displaceAngleDegrees", config.displaceAngleDegrees_},
                           {"channelCycleAngleDegrees", config.channelCycleAngleDegrees_},
                           {"reverbPreDelayFrames", config.reverbPreDelayFrames_},
                           {"reverbDecayFrames", config.reverbDecayFrames_},
                           {"reverbRoomSize", config.reverbRoomSize_},
                           {"reverbDiffusion", config.reverbDiffusion_},
                           {"reverbAbsorption", config.reverbAbsorption_},
                           {"reverbMix", config.reverbMix_},
                           {"downsampleMode", config.downsampleMode_},
                           {"downsampleBlockSize", config.downsampleBlockSize_}};
    writeOptionalMindWaveId(json, "blurSigmaMindWaveId", config.blurSigmaMindWave_);
    writeOptionalMindWaveId(json, "medianSizeMindWaveId", config.medianSizeMindWave_);
    writeOptionalMindWaveId(json, "directionalBlurLengthMindWaveId", config.directionalBlurLengthMindWave_);
    writeOptionalMindWaveId(json, "directionalBlurAngleMindWaveId", config.directionalBlurAngleMindWave_);
    writeOptionalMindWaveId(json, "sharpenAmountMindWaveId", config.sharpenAmountMindWave_);
    // v0.Y.38.1 (Filter Parameter Binding Completion).
    writeOptionalMindWaveId(json, "speckleDensityMindWaveId", config.speckleDensityMindWave_);
    writeOptionalMindWaveId(json, "speckleIntensityMindWaveId", config.speckleIntensityMindWave_);
    writeOptionalMindWaveId(json, "speckleThresholdMindWaveId", config.speckleThresholdMindWave_);
    writeOptionalMindWaveId(json, "noiseFloorMindWaveId", config.noiseFloorMindWave_);
    writeOptionalMindWaveId(json, "reductionMindWaveId", config.reductionMindWave_);
    writeOptionalMindWaveId(json, "crushAmountMindWaveId", config.crushAmountMindWave_);
    writeOptionalMindWaveId(json, "grainAmountMindWaveId", config.grainAmountMindWave_);
    writeOptionalMindWaveId(json, "feedbackAmountMindWaveId", config.feedbackAmountMindWave_);
    writeOptionalMindWaveId(json, "foldGainMindWaveId", config.foldGainMindWave_);
    writeOptionalMindWaveId(json, "channelBalanceMindWaveId", config.channelBalanceMindWave_);
    writeOptionalMindWaveId(json, "convolveAmountMindWaveId", config.convolveAmountMindWave_);
    writeOptionalMindWaveId(json, "displaceDistanceMindWaveId", config.displaceDistanceMindWave_);
    writeOptionalMindWaveId(json, "displaceAngleMindWaveId", config.displaceAngleMindWave_);
    writeOptionalMindWaveId(json, "channelCycleAngleMindWaveId", config.channelCycleAngleMindWave_);
    writeOptionalMindWaveId(json, "reverbMixMindWaveId", config.reverbMixMindWave_);
    writeOptionalMindWaveId(json, "downsampleBlockSizeMindWaveId", config.downsampleBlockSizeMindWave_);
}

void from_json(const nlohmann::json& json, FilterConfiguration& config) {
    json.at("type").get_to(config.type_);
    json.at("blurSigma").get_to(config.blurSigma_);
    json.at("medianSize").get_to(config.medianSize_);
    json.at("directionalBlurLength").get_to(config.directionalBlurLength_);
    json.at("directionalBlurAngleDegrees").get_to(config.directionalBlurAngleDegrees_);
    json.at("sharpenAmount").get_to(config.sharpenAmount_);
    json.at("toneCurvePoints").get_to(config.toneCurvePoints_);
    json.at("frequencyGradient").get_to(config.frequencyGradient_);
    // Lenient (defaults to unbound if absent) - didn't exist before
    // v0.Y.31.1 Installment D; a configuration saved before this
    // installment was never MindWave-bound anyway.
    config.blurSigmaMindWave_ = readOptionalMindWaveId(json, "blurSigmaMindWaveId");
    config.medianSizeMindWave_ = readOptionalMindWaveId(json, "medianSizeMindWaveId");
    config.directionalBlurLengthMindWave_ = readOptionalMindWaveId(json, "directionalBlurLengthMindWaveId");
    config.directionalBlurAngleMindWave_ = readOptionalMindWaveId(json, "directionalBlurAngleMindWaveId");
    config.sharpenAmountMindWave_ = readOptionalMindWaveId(json, "sharpenAmountMindWaveId");
    // Lenient (defaults to the already-freshly-seeded/constructed value if
    // absent) - didn't exist before v0.Y.36.1 Installment A; a project
    // saved before this installment never had any of these fields at all.
    config.noiseSeed_ = json.value("noiseSeed", config.noiseSeed_);
    config.speckleDensity_ = json.value("speckleDensity", config.speckleDensity_);
    config.speckleIntensity_ = json.value("speckleIntensity", config.speckleIntensity_);
    config.speckleThresholdDb_ = json.value("speckleThresholdDb", config.speckleThresholdDb_);
    config.noiseFloorDb_ = json.value("noiseFloorDb", config.noiseFloorDb_);
    config.reductionDb_ = json.value("reductionDb", config.reductionDb_);
    config.crushAmount_ = json.value("crushAmount", config.crushAmount_);
    config.grainSize_ = json.value("grainSize", config.grainSize_);
    config.grainAmountDb_ = json.value("grainAmountDb", config.grainAmountDb_);
    config.feedbackAmount_ = json.value("feedbackAmount", config.feedbackAmount_);
    config.foldGain_ = json.value("foldGain", config.foldGain_);
    // Lenient, same reasoning - didn't exist before v0.Y.36.1 Installment B.
    config.channelBalance_ = json.value("channelBalance", config.channelBalance_);
    config.convolveKernel_ = json.value("convolveKernel", config.convolveKernel_);
    config.convolveKernelSize_ = json.value("convolveKernelSize", config.convolveKernelSize_);
    config.convolveNormalize_ = json.value("convolveNormalize", config.convolveNormalize_);
    config.convolveAmount_ = json.value("convolveAmount", config.convolveAmount_);
    // Lenient, same reasoning - didn't exist before v0.Y.36.1 Installment C.
    config.displaceDistance_ = json.value("displaceDistance", config.displaceDistance_);
    config.displaceAngleDegrees_ = json.value("displaceAngleDegrees", config.displaceAngleDegrees_);
    config.channelCycleAngleDegrees_ = json.value("channelCycleAngleDegrees", config.channelCycleAngleDegrees_);
    // Lenient, same reasoning - didn't exist before v0.Y.36.1 Installment D.
    config.reverbPreDelayFrames_ = json.value("reverbPreDelayFrames", config.reverbPreDelayFrames_);
    config.reverbDecayFrames_ = json.value("reverbDecayFrames", config.reverbDecayFrames_);
    config.reverbRoomSize_ = json.value("reverbRoomSize", config.reverbRoomSize_);
    config.reverbDiffusion_ = json.value("reverbDiffusion", config.reverbDiffusion_);
    config.reverbAbsorption_ = json.value("reverbAbsorption", config.reverbAbsorption_);
    config.reverbMix_ = json.value("reverbMix", config.reverbMix_);
    // Lenient, same reasoning - didn't exist before finding #18's own
    // installment (real-world testing pass, 2026-09-20).
    config.downsampleMode_ = json.value("downsampleMode", config.downsampleMode_);
    config.downsampleBlockSize_ = json.value("downsampleBlockSize", config.downsampleBlockSize_);
    // Lenient (defaults to unbound if absent) - didn't exist before
    // v0.Y.38.1 (Filter Parameter Binding Completion); a configuration
    // saved before this milestone was never bound on any of these anyway.
    config.speckleDensityMindWave_ = readOptionalMindWaveId(json, "speckleDensityMindWaveId");
    config.speckleIntensityMindWave_ = readOptionalMindWaveId(json, "speckleIntensityMindWaveId");
    config.speckleThresholdMindWave_ = readOptionalMindWaveId(json, "speckleThresholdMindWaveId");
    config.noiseFloorMindWave_ = readOptionalMindWaveId(json, "noiseFloorMindWaveId");
    config.reductionMindWave_ = readOptionalMindWaveId(json, "reductionMindWaveId");
    config.crushAmountMindWave_ = readOptionalMindWaveId(json, "crushAmountMindWaveId");
    config.grainAmountMindWave_ = readOptionalMindWaveId(json, "grainAmountMindWaveId");
    config.feedbackAmountMindWave_ = readOptionalMindWaveId(json, "feedbackAmountMindWaveId");
    config.foldGainMindWave_ = readOptionalMindWaveId(json, "foldGainMindWaveId");
    config.channelBalanceMindWave_ = readOptionalMindWaveId(json, "channelBalanceMindWaveId");
    config.convolveAmountMindWave_ = readOptionalMindWaveId(json, "convolveAmountMindWaveId");
    config.displaceDistanceMindWave_ = readOptionalMindWaveId(json, "displaceDistanceMindWaveId");
    config.displaceAngleMindWave_ = readOptionalMindWaveId(json, "displaceAngleMindWaveId");
    config.channelCycleAngleMindWave_ = readOptionalMindWaveId(json, "channelCycleAngleMindWaveId");
    config.reverbMixMindWave_ = readOptionalMindWaveId(json, "reverbMixMindWaveId");
    config.downsampleBlockSizeMindWave_ = readOptionalMindWaveId(json, "downsampleBlockSizeMindWaveId");
}

}  // namespace sound_mind::core
