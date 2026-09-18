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
                           {"channelCycleAngleDegrees", config.channelCycleAngleDegrees_}};
    writeOptionalMindWaveId(json, "blurSigmaMindWaveId", config.blurSigmaMindWave_);
    writeOptionalMindWaveId(json, "medianSizeMindWaveId", config.medianSizeMindWave_);
    writeOptionalMindWaveId(json, "directionalBlurLengthMindWaveId", config.directionalBlurLengthMindWave_);
    writeOptionalMindWaveId(json, "directionalBlurAngleMindWaveId", config.directionalBlurAngleMindWave_);
    writeOptionalMindWaveId(json, "sharpenAmountMindWaveId", config.sharpenAmountMindWave_);
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
}

}  // namespace sound_mind::core
