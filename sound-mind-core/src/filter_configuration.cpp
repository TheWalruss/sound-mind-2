#include "sound_mind/core/filter_configuration.h"

#include <cmath>

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

FilterConfiguration::ParameterSlot FilterConfiguration::slot(FilterParameter parameter) noexcept {
    switch (parameter) {
        case FilterParameter::BlurSigma: return {&blurSigma_, nullptr, &blurSigmaMindWave_};
        case FilterParameter::MedianSize: return {nullptr, &medianSize_, &medianSizeMindWave_};
        case FilterParameter::DirectionalBlurLength:
            return {nullptr, &directionalBlurLength_, &directionalBlurLengthMindWave_};
        case FilterParameter::DirectionalBlurAngle:
            return {&directionalBlurAngleDegrees_, nullptr, &directionalBlurAngleMindWave_};
        case FilterParameter::SharpenAmount: return {&sharpenAmount_, nullptr, &sharpenAmountMindWave_};
        case FilterParameter::SpeckleDensity: return {&speckleDensity_, nullptr, &speckleDensityMindWave_};
        case FilterParameter::SpeckleIntensity: return {&speckleIntensity_, nullptr, &speckleIntensityMindWave_};
        case FilterParameter::SpeckleThreshold: return {&speckleThresholdDb_, nullptr, &speckleThresholdMindWave_};
        case FilterParameter::NoiseFloor: return {&noiseFloorDb_, nullptr, &noiseFloorMindWave_};
        case FilterParameter::Reduction: return {&reductionDb_, nullptr, &reductionMindWave_};
        case FilterParameter::CrushAmount: return {&crushAmount_, nullptr, &crushAmountMindWave_};
        case FilterParameter::GrainSize: return {nullptr, &grainSize_, nullptr};
        case FilterParameter::GrainAmount: return {&grainAmountDb_, nullptr, &grainAmountMindWave_};
        case FilterParameter::FeedbackAmount: return {&feedbackAmount_, nullptr, &feedbackAmountMindWave_};
        case FilterParameter::FoldGain: return {&foldGain_, nullptr, &foldGainMindWave_};
        case FilterParameter::ChannelBalance: return {&channelBalance_, nullptr, &channelBalanceMindWave_};
        case FilterParameter::ConvolveKernelSize: return {nullptr, &convolveKernelSize_, nullptr};
        case FilterParameter::ConvolveAmount: return {&convolveAmount_, nullptr, &convolveAmountMindWave_};
        case FilterParameter::DisplaceDistance: return {&displaceDistance_, nullptr, &displaceDistanceMindWave_};
        case FilterParameter::DisplaceAngle: return {&displaceAngleDegrees_, nullptr, &displaceAngleMindWave_};
        case FilterParameter::ChannelCycleAngle:
            return {&channelCycleAngleDegrees_, nullptr, &channelCycleAngleMindWave_};
        case FilterParameter::ReverbPreDelay: return {nullptr, &reverbPreDelayFrames_, nullptr};
        case FilterParameter::ReverbDecay: return {nullptr, &reverbDecayFrames_, nullptr};
        case FilterParameter::ReverbRoomSize: return {&reverbRoomSize_, nullptr, nullptr};
        case FilterParameter::ReverbDiffusion: return {&reverbDiffusion_, nullptr, nullptr};
        case FilterParameter::ReverbAbsorption: return {&reverbAbsorption_, nullptr, nullptr};
        case FilterParameter::ReverbMix: return {&reverbMix_, nullptr, &reverbMixMindWave_};
        case FilterParameter::DownsampleBlockSize:
            return {nullptr, &downsampleBlockSize_, &downsampleBlockSizeMindWave_};
    }
    return {};
}

double FilterConfiguration::parameter(FilterParameter parameter) const noexcept {
    const ParameterSlot where = const_cast<FilterConfiguration*>(this)->slot(parameter);
    if (where.real != nullptr) {
        return static_cast<double>(*where.real);
    }
    return where.whole != nullptr ? static_cast<double>(*where.whole) : 0.0;
}

void FilterConfiguration::setParameter(FilterParameter parameter, double value) noexcept {
    const ParameterSlot where = slot(parameter);
    if (where.real != nullptr) {
        *where.real = static_cast<float>(value);
    } else if (where.whole != nullptr) {
        *where.whole = static_cast<int>(std::lround(value));
    }
}

std::optional<MindWaveId> FilterConfiguration::parameterMindWave(FilterParameter parameter) const noexcept {
    const ParameterSlot where = const_cast<FilterConfiguration*>(this)->slot(parameter);
    return where.mindWave != nullptr ? *where.mindWave : std::nullopt;
}

void FilterConfiguration::setParameterMindWave(FilterParameter parameter,
                                               std::optional<MindWaveId> mindWaveId) noexcept {
    const ParameterSlot where = slot(parameter);
    if (where.mindWave != nullptr) {
        *where.mindWave = mindWaveId;
    }
}

void to_json(nlohmann::json& json, const FilterConfiguration& config) {
    json = nlohmann::json{{"type", config.type_},
                           {"toneCurvePoints", config.toneCurvePoints_},
                           {"frequencyGradient", config.frequencyGradient_},
                           {"noiseSeed", config.noiseSeed_},
                           {"convolveKernel", config.convolveKernel_},
                           {"convolveNormalize", config.convolveNormalize_},
                           {"downsampleMode", config.downsampleMode_}};
    // Every numeric parameter and its binding, from the one table
    // (v0.1.48.10) - the same keys and types as before it.
    auto& mutableConfig = const_cast<FilterConfiguration&>(config);
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        const auto where = mutableConfig.slot(spec.parameter);
        if (where.real != nullptr) {
            json[spec.key] = *where.real;
        } else if (where.whole != nullptr) {
            json[spec.key] = *where.whole;
        }
        if (spec.mindWaveKey != nullptr && where.mindWave != nullptr) {
            writeOptionalMindWaveId(json, spec.mindWaveKey, *where.mindWave);
        }
    }
}

void from_json(const nlohmann::json& json, FilterConfiguration& config) {
    json.at("type").get_to(config.type_);
    json.at("toneCurvePoints").get_to(config.toneCurvePoints_);
    json.at("frequencyGradient").get_to(config.frequencyGradient_);
    config.noiseSeed_ = json.value("noiseSeed", config.noiseSeed_);
    config.convolveKernel_ = json.value("convolveKernel", config.convolveKernel_);
    config.convolveNormalize_ = json.value("convolveNormalize", config.convolveNormalize_);
    config.downsampleMode_ = json.value("downsampleMode", config.downsampleMode_);
    // Every numeric parameter and its binding, from the one table
    // (v0.1.48.10). A missing value keeps its default; a missing binding
    // is unbound.
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        const auto where = config.slot(spec.parameter);
        if (where.real != nullptr) {
            *where.real = json.value(spec.key, *where.real);
        } else if (where.whole != nullptr) {
            *where.whole = json.value(spec.key, *where.whole);
        }
        if (spec.mindWaveKey != nullptr && where.mindWave != nullptr) {
            *where.mindWave = readOptionalMindWaveId(json, spec.mindWaveKey);
        }
    }
}

}  // namespace sound_mind::core
