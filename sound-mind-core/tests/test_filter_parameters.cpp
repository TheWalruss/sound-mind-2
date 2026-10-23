#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/filter_parameters.h"
#include "sound_mind/core/mind_wave.h"

using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterParameter;
using sound_mind::core::FilterParameterSpec;
using sound_mind::core::filterParameterSpec;
using sound_mind::core::filterParameterSpecs;
using sound_mind::core::isMindWaveBindable;
using sound_mind::core::kFilterParameterCount;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;

namespace {

/// A value inside `spec`'s range that differs from any default.
double probeValue(const FilterParameterSpec& spec) {
    const double value = spec.minimum + 0.37 * (spec.maximum - spec.minimum);
    return spec.integer ? static_cast<double>(static_cast<long>(value + 0.5)) : value;
}

}  // namespace

// v0.1.48.10 - one table describes every numeric filter parameter.
TEST_CASE("The filter parameter table has one well-formed row per parameter, in enum order",
          "[core][filter_parameters]") {
    std::set<std::string> keys;
    std::set<std::string> mindWaveKeys;
    for (std::size_t i = 0; i < kFilterParameterCount; ++i) {
        const FilterParameterSpec& spec = filterParameterSpecs()[i];
        CHECK(static_cast<std::size_t>(spec.parameter) == i);
        CHECK(&filterParameterSpec(spec.parameter) == &spec);
        CHECK(spec.minimum < spec.maximum);
        CHECK(keys.insert(spec.key).second);
        if (spec.mindWaveKey != nullptr) {
            const std::string key = spec.mindWaveKey;
            CHECK(key.ends_with("MindWaveId"));  // what mindWaveDependents() looks for.
            CHECK(mindWaveKeys.insert(key).second);
        }
    }
    CHECK(mindWaveKeys.size() == 21);
}

TEST_CASE("The generic accessors read and write the named ones", "[core][filter_parameters]") {
    FilterConfiguration config;
    config.setBlurSigma(7.5f);
    config.setMedianSize(9);
    config.setReverbDecayFrames(120);
    CHECK(config.parameter(FilterParameter::BlurSigma) == 7.5);
    CHECK(config.parameter(FilterParameter::MedianSize) == 9.0);
    CHECK(config.parameter(FilterParameter::ReverbDecay) == 120.0);

    config.setParameter(FilterParameter::SharpenAmount, 2.5);
    CHECK(config.sharpenAmount() == 2.5f);
    config.setParameter(FilterParameter::DownsampleBlockSize, 7.6);  // integer: rounded.
    CHECK(config.downsampleBlockSize() == 8);
}

TEST_CASE("Every parameter's value round-trips through the generic accessors", "[core][filter_parameters]") {
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        FilterConfiguration config;
        const double value = probeValue(spec);
        config.setParameter(spec.parameter, value);
        CHECK(std::abs(config.parameter(spec.parameter) - value) < 1e-4);
    }
}

TEST_CASE("Only bindable parameters keep a MindWave binding", "[core][filter_parameters]") {
    FilterConfiguration config;
    config.setBlurSigmaMindWave(MindWaveId{4});
    CHECK(config.parameterMindWave(FilterParameter::BlurSigma) == MindWaveId{4});
    config.setParameterMindWave(FilterParameter::ReverbMix, MindWaveId{5});
    CHECK(config.reverbMixMindWave() == MindWaveId{5});

    REQUIRE_FALSE(isMindWaveBindable(FilterParameter::GrainSize));
    config.setParameterMindWave(FilterParameter::GrainSize, MindWaveId{6});  // ignored.
    CHECK_FALSE(config.parameterMindWave(FilterParameter::GrainSize).has_value());
}

TEST_CASE("Filter JSON keeps its keys and round-trips every parameter and binding", "[core][filter_parameters]") {
    FilterConfiguration config;
    MindWaveId nextId = 100;
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        config.setParameter(spec.parameter, probeValue(spec));
        config.setParameterMindWave(spec.parameter, nextId++);
    }
    const nlohmann::json json = config;

    // The same keys as before the table: every value, every binding, and
    // the non-numeric fields.
    for (const char* key : {"type", "toneCurvePoints", "frequencyGradient", "noiseSeed", "convolveKernel",
                            "convolveNormalize", "downsampleMode", "blurSigma", "medianSize",
                            "directionalBlurLength", "directionalBlurAngleDegrees", "sharpenAmount", "speckleDensity",
                            "speckleIntensity", "speckleThresholdDb", "noiseFloorDb", "reductionDb", "crushAmount",
                            "grainSize", "grainAmountDb", "feedbackAmount", "foldGain", "channelBalance",
                            "convolveKernelSize", "convolveAmount", "displaceDistance", "displaceAngleDegrees",
                            "channelCycleAngleDegrees", "reverbPreDelayFrames", "reverbDecayFrames", "reverbRoomSize",
                            "reverbDiffusion", "reverbAbsorption", "reverbMix", "downsampleBlockSize",
                            "blurSigmaMindWaveId", "downsampleBlockSizeMindWaveId", "speckleThresholdMindWaveId"}) {
        CHECK(json.contains(key));
    }
    CHECK(json.at("medianSize").is_number_integer());
    CHECK(json.size() == 7 + kFilterParameterCount + 21);

    const auto restored = json.get<FilterConfiguration>();
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        CHECK(restored.parameter(spec.parameter) == config.parameter(spec.parameter));
        CHECK(restored.parameterMindWave(spec.parameter) == config.parameterMindWave(spec.parameter));
    }
}

TEST_CASE("A missing parameter in filter JSON keeps its default", "[core][filter_parameters]") {
    nlohmann::json json = FilterConfiguration{};
    json.erase("blurSigma");
    json.erase("reverbDecayFrames");
    const auto restored = json.get<FilterConfiguration>();
    CHECK(restored.blurSigma() == FilterConfiguration{}.blurSigma());
    CHECK(restored.reverbDecayFrames() == FilterConfiguration{}.reverbDecayFrames());
}

TEST_CASE("Resolving a filter's MindWaves follows the table", "[core][filter_parameters]") {
    FilterConfiguration config;
    std::map<MindWaveId, MindWave> waves;
    MindWaveId nextId = 1;
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        if (spec.mindWaveKey != nullptr) {
            waves.emplace(nextId, MindWave{});
            config.setParameterMindWave(spec.parameter, nextId++);
        }
    }
    const auto resolved = sound_mind::core::resolveFilterParameterMindWaves(config, [&waves](MindWaveId id) {
        const auto it = waves.find(id);
        return it != waves.end() ? &it->second : nullptr;
    });
    for (const FilterParameterSpec& spec : filterParameterSpecs()) {
        if (spec.mindWaveKey != nullptr) {
            CHECK(resolved.of(spec.parameter) == &waves.at(*config.parameterMindWave(spec.parameter)));
        } else {
            CHECK(resolved.of(spec.parameter) == nullptr);
        }
    }
    CHECK(resolved.blurSigma == resolved.of(FilterParameter::BlurSigma));  // the named fields too.
}
