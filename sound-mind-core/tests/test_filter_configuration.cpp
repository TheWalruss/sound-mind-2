#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/filter_configuration.h"

using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::MindWaveId;

TEST_CASE("A fresh FilterConfiguration is FrequencyAxisGradient", "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.type() == FilterType::FrequencyAxisGradient);
}

TEST_CASE("A fresh FilterConfiguration has a fresh, fully transparent gradient", "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.frequencyGradient().stops().size() == 2);
    REQUIRE(config.frequencyGradient().stops().front().leftOpacity == 0.0f);
}

TEST_CASE("A fresh FilterConfiguration's tone curve is the identity", "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.toneCurvePoints().size() == 2);
    REQUIRE(config.toneCurvePoints().front() == std::array<float, 2>{0.0f, 0.0f});
    REQUIRE(config.toneCurvePoints().back() == std::array<float, 2>{1.0f, 1.0f});
}

TEST_CASE("A FilterConfiguration's type can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    REQUIRE(config.type() == FilterType::Sharpen);
}

TEST_CASE("A FilterConfiguration's blurSigma can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setBlurSigma(5.5f);
    REQUIRE(config.blurSigma() == 5.5f);
}

TEST_CASE("A FilterConfiguration's medianSize can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setMedianSize(7);
    REQUIRE(config.medianSize() == 7);
}

TEST_CASE("A FilterConfiguration's directional blur length/angle can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setDirectionalBlurLength(20);
    config.setDirectionalBlurAngleDegrees(45.0f);
    REQUIRE(config.directionalBlurLength() == 20);
    REQUIRE(config.directionalBlurAngleDegrees() == 45.0f);
}

TEST_CASE("A FilterConfiguration's sharpenAmount can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setSharpenAmount(2.5f);
    REQUIRE(config.sharpenAmount() == 2.5f);
}

TEST_CASE("A FilterConfiguration's tone curve points can be replaced", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setToneCurvePoints({{0.0f, 0.0f}, {0.5f, 0.8f}, {1.0f, 1.0f}});
    REQUIRE(config.toneCurvePoints().size() == 3);
    REQUIRE(config.toneCurvePoints()[1] == std::array<float, 2>{0.5f, 0.8f});
}

TEST_CASE("A FilterConfiguration's tone curve points can be mutated in place", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.toneCurvePoints().push_back({0.5f, 0.5f});
    REQUIRE(config.toneCurvePoints().size() == 3);
}

TEST_CASE("A FilterConfiguration's frequency gradient can be mutated in place", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.frequencyGradient().setLinkChannels(true);
    REQUIRE(config.frequencyGradient().linkChannels());
}

TEST_CASE("A FilterConfiguration round-trips through JSON", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setBlurSigma(3.0f);
    config.setMedianSize(9);
    config.setDirectionalBlurLength(15);
    config.setDirectionalBlurAngleDegrees(90.0f);
    config.setSharpenAmount(1.5f);
    config.setToneCurvePoints({{0.0f, 0.0f}, {0.5f, 0.9f}, {1.0f, 1.0f}});
    config.frequencyGradient().setLinkChannels(true);

    const nlohmann::json json = config;
    const FilterConfiguration roundTripped = json.get<FilterConfiguration>();

    REQUIRE(roundTripped.type() == FilterType::DirectionalBlur);
    REQUIRE(roundTripped.blurSigma() == 3.0f);
    REQUIRE(roundTripped.medianSize() == 9);
    REQUIRE(roundTripped.directionalBlurLength() == 15);
    REQUIRE(roundTripped.directionalBlurAngleDegrees() == 90.0f);
    REQUIRE(roundTripped.sharpenAmount() == 1.5f);
    REQUIRE(roundTripped.toneCurvePoints().size() == 3);
    REQUIRE(roundTripped.toneCurvePoints()[1] == std::array<float, 2>{0.5f, 0.9f});
    REQUIRE(roundTripped.frequencyGradient().linkChannels());
}

// --- v0.Y.31.1 Installment D: filter-parameter MindWave bindings -----

TEST_CASE("A fresh FilterConfiguration's five bindable parameters are all unbound", "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE_FALSE(config.blurSigmaMindWave().has_value());
    REQUIRE_FALSE(config.medianSizeMindWave().has_value());
    REQUIRE_FALSE(config.directionalBlurLengthMindWave().has_value());
    REQUIRE_FALSE(config.directionalBlurAngleMindWave().has_value());
    REQUIRE_FALSE(config.sharpenAmountMindWave().has_value());
}

TEST_CASE("Each of a FilterConfiguration's five parameters can be bound to (and unbound from) a MindWave",
          "[core][filter_configuration]") {
    FilterConfiguration config;

    config.setBlurSigmaMindWave(MindWaveId{1});
    config.setMedianSizeMindWave(MindWaveId{2});
    config.setDirectionalBlurLengthMindWave(MindWaveId{3});
    config.setDirectionalBlurAngleMindWave(MindWaveId{4});
    config.setSharpenAmountMindWave(MindWaveId{5});

    REQUIRE(config.blurSigmaMindWave() == MindWaveId{1});
    REQUIRE(config.medianSizeMindWave() == MindWaveId{2});
    REQUIRE(config.directionalBlurLengthMindWave() == MindWaveId{3});
    REQUIRE(config.directionalBlurAngleMindWave() == MindWaveId{4});
    REQUIRE(config.sharpenAmountMindWave() == MindWaveId{5});

    config.setBlurSigmaMindWave(std::nullopt);
    REQUIRE_FALSE(config.blurSigmaMindWave().has_value());
}

TEST_CASE("A FilterConfiguration's five parameter bindings round-trip through JSON", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setBlurSigmaMindWave(MindWaveId{1});
    config.setMedianSizeMindWave(MindWaveId{2});
    config.setDirectionalBlurLengthMindWave(MindWaveId{3});
    config.setDirectionalBlurAngleMindWave(MindWaveId{4});
    config.setSharpenAmountMindWave(MindWaveId{5});

    const nlohmann::json json = config;
    const FilterConfiguration roundTripped = json.get<FilterConfiguration>();

    REQUIRE(roundTripped.blurSigmaMindWave() == MindWaveId{1});
    REQUIRE(roundTripped.medianSizeMindWave() == MindWaveId{2});
    REQUIRE(roundTripped.directionalBlurLengthMindWave() == MindWaveId{3});
    REQUIRE(roundTripped.directionalBlurAngleMindWave() == MindWaveId{4});
    REQUIRE(roundTripped.sharpenAmountMindWave() == MindWaveId{5});
}

TEST_CASE("A FilterConfiguration loads from JSON missing the five MindWave binding keys "
          "(a configuration saved before v0.Y.31.1 Installment D) as fully unbound",
          "[core][filter_configuration]") {
    const nlohmann::json json{{"type", "uniformBlur"},
                               {"blurSigma", 2.0f},
                               {"medianSize", 3},
                               {"directionalBlurLength", 10},
                               {"directionalBlurAngleDegrees", 0.0f},
                               {"sharpenAmount", 1.0f},
                               {"toneCurvePoints", std::vector<std::array<float, 2>>{{0.0f, 0.0f}, {1.0f, 1.0f}}},
                               {"frequencyGradient", FilterConfiguration{}.frequencyGradient()}};

    const FilterConfiguration restored = json.get<FilterConfiguration>();

    REQUIRE_FALSE(restored.blurSigmaMindWave().has_value());
    REQUIRE_FALSE(restored.medianSizeMindWave().has_value());
    REQUIRE_FALSE(restored.directionalBlurLengthMindWave().has_value());
    REQUIRE_FALSE(restored.directionalBlurAngleMindWave().has_value());
    REQUIRE_FALSE(restored.sharpenAmountMindWave().has_value());
}

// --- v0.Y.36.1 Installment A: Noise & distortion ----------------------

TEST_CASE("A fresh FilterConfiguration has sensible Noise & distortion defaults", "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.speckleDensity() == 0.05f);
    REQUIRE(config.speckleIntensity() == 0.8f);
    REQUIRE(config.speckleThresholdDb() == 12.0f);
    REQUIRE(config.noiseFloorDb() == -60.0f);
    REQUIRE(config.reductionDb() == 24.0f);
    REQUIRE(config.crushAmount() == 0.5f);
    REQUIRE(config.grainSize() == 4);
    REQUIRE(config.grainAmountDb() == 6.0f);
    REQUIRE(config.feedbackAmount() == 0.5f);
    REQUIRE(config.foldGain() == 2.0f);
}

TEST_CASE("Two freshly-constructed FilterConfigurations get different noise seeds", "[core][filter_configuration]") {
    // Not a hard guarantee (std::random_device could theoretically repeat),
    // but astronomically unlikely for two back-to-back constructions - a
    // real, catchable regression if noiseSeed() were ever accidentally
    // hardcoded to a fixed default instead of freshly seeded.
    const FilterConfiguration first;
    const FilterConfiguration second;
    REQUIRE(first.noiseSeed() != second.noiseSeed());
}

TEST_CASE("A FilterConfiguration's noiseSeed can be set explicitly", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setNoiseSeed(42u);
    REQUIRE(config.noiseSeed() == 42u);
}

TEST_CASE("Each of a FilterConfiguration's Noise & distortion parameters can be changed",
          "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setSpeckleDensity(0.2f);
    config.setSpeckleIntensity(0.5f);
    config.setSpeckleThresholdDb(20.0f);
    config.setNoiseFloorDb(-40.0f);
    config.setReductionDb(12.0f);
    config.setCrushAmount(0.9f);
    config.setGrainSize(8);
    config.setGrainAmountDb(3.0f);
    config.setFeedbackAmount(0.75f);
    config.setFoldGain(4.0f);

    REQUIRE(config.speckleDensity() == 0.2f);
    REQUIRE(config.speckleIntensity() == 0.5f);
    REQUIRE(config.speckleThresholdDb() == 20.0f);
    REQUIRE(config.noiseFloorDb() == -40.0f);
    REQUIRE(config.reductionDb() == 12.0f);
    REQUIRE(config.crushAmount() == 0.9f);
    REQUIRE(config.grainSize() == 8);
    REQUIRE(config.grainAmountDb() == 3.0f);
    REQUIRE(config.feedbackAmount() == 0.75f);
    REQUIRE(config.foldGain() == 4.0f);
}

TEST_CASE("Every new Noise & distortion FilterType can be set", "[core][filter_configuration]") {
    FilterConfiguration config;
    for (const FilterType type : {FilterType::SpeckleAdd, FilterType::SpeckleRemove, FilterType::Denoise,
                                   FilterType::BitDepthCrush, FilterType::GranularNoise, FilterType::DynamicSpeckle,
                                   FilterType::FeedbackDistortion, FilterType::SpectralWavefold}) {
        config.setType(type);
        REQUIRE(config.type() == type);
    }
}

TEST_CASE("A FilterConfiguration's Noise & distortion parameters round-trip through JSON",
          "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::GranularNoise);
    config.setNoiseSeed(123u);
    config.setSpeckleDensity(0.2f);
    config.setSpeckleIntensity(0.5f);
    config.setSpeckleThresholdDb(20.0f);
    config.setNoiseFloorDb(-40.0f);
    config.setReductionDb(12.0f);
    config.setCrushAmount(0.9f);
    config.setGrainSize(8);
    config.setGrainAmountDb(3.0f);
    config.setFeedbackAmount(0.75f);
    config.setFoldGain(4.0f);

    const nlohmann::json json = config;
    const FilterConfiguration roundTripped = json.get<FilterConfiguration>();

    REQUIRE(roundTripped.type() == FilterType::GranularNoise);
    REQUIRE(roundTripped.noiseSeed() == 123u);
    REQUIRE(roundTripped.speckleDensity() == 0.2f);
    REQUIRE(roundTripped.speckleIntensity() == 0.5f);
    REQUIRE(roundTripped.speckleThresholdDb() == 20.0f);
    REQUIRE(roundTripped.noiseFloorDb() == -40.0f);
    REQUIRE(roundTripped.reductionDb() == 12.0f);
    REQUIRE(roundTripped.crushAmount() == 0.9f);
    REQUIRE(roundTripped.grainSize() == 8);
    REQUIRE(roundTripped.grainAmountDb() == 3.0f);
    REQUIRE(roundTripped.feedbackAmount() == 0.75f);
    REQUIRE(roundTripped.foldGain() == 4.0f);
}

TEST_CASE("A FilterConfiguration loads from JSON missing every Noise & distortion key "
          "(a configuration saved before v0.Y.36.1 Installment A) using sensible defaults",
          "[core][filter_configuration]") {
    const nlohmann::json json{{"type", "uniformBlur"},
                               {"blurSigma", 2.0f},
                               {"medianSize", 3},
                               {"directionalBlurLength", 10},
                               {"directionalBlurAngleDegrees", 0.0f},
                               {"sharpenAmount", 1.0f},
                               {"toneCurvePoints", std::vector<std::array<float, 2>>{{0.0f, 0.0f}, {1.0f, 1.0f}}},
                               {"frequencyGradient", FilterConfiguration{}.frequencyGradient()}};

    const FilterConfiguration restored = json.get<FilterConfiguration>();

    // noiseSeed() isn't asserted to an exact value here - a configuration
    // missing this key keeps whatever fresh, real seed its own default
    // construction already generated (see from_json()'s own docs), not a
    // fixed fallback - only the other, plain-scalar defaults are checked.
    REQUIRE(restored.speckleDensity() == 0.05f);
    REQUIRE(restored.speckleIntensity() == 0.8f);
    REQUIRE(restored.speckleThresholdDb() == 12.0f);
    REQUIRE(restored.noiseFloorDb() == -60.0f);
    REQUIRE(restored.reductionDb() == 24.0f);
    REQUIRE(restored.crushAmount() == 0.5f);
    REQUIRE(restored.grainSize() == 4);
    REQUIRE(restored.grainAmountDb() == 6.0f);
    REQUIRE(restored.feedbackAmount() == 0.5f);
    REQUIRE(restored.foldGain() == 2.0f);
}

// --- v0.Y.36.1 Installment B: Tonal/Spectral shaping -------------------

TEST_CASE("A fresh FilterConfiguration has sensible ChannelBalance/Convolve defaults",
          "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.channelBalance() == 0.5f);
    REQUIRE(config.convolveKernel() == std::vector<float>{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    REQUIRE(config.convolveKernelSize() == 3);
    REQUIRE_FALSE(config.convolveNormalize());
    REQUIRE(config.convolveAmount() == 1.0f);
}

TEST_CASE("Every new Tonal/Spectral-shaping FilterType can be set", "[core][filter_configuration]") {
    FilterConfiguration config;
    for (const FilterType type : {FilterType::ChannelBalance, FilterType::Invert, FilterType::Convolve}) {
        config.setType(type);
        REQUIRE(config.type() == type);
    }
}

TEST_CASE("A FilterConfiguration's ChannelBalance can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setChannelBalance(0.2f);
    REQUIRE(config.channelBalance() == 0.2f);
}

TEST_CASE("A FilterConfiguration's Convolve parameters can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    const std::vector<float> sharpen{0.0f, -1.0f, 0.0f, -1.0f, 5.0f, -1.0f, 0.0f, -1.0f, 0.0f};
    config.setConvolveKernel(sharpen);
    config.setConvolveKernelSize(3);
    config.setConvolveNormalize(true);
    config.setConvolveAmount(0.6f);

    REQUIRE(config.convolveKernel() == sharpen);
    REQUIRE(config.convolveKernelSize() == 3);
    REQUIRE(config.convolveNormalize());
    REQUIRE(config.convolveAmount() == 0.6f);
}

TEST_CASE("A FilterConfiguration's Convolve kernel can be mutated in place", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.convolveKernel()[4] = 2.0f;
    REQUIRE(config.convolveKernel()[4] == 2.0f);
}

TEST_CASE("A FilterConfiguration's ChannelBalance/Convolve parameters round-trip through JSON",
          "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::Convolve);
    config.setChannelBalance(0.3f);
    const std::vector<float> sharpen{0.0f, -1.0f, 0.0f, -1.0f, 5.0f, -1.0f, 0.0f, -1.0f, 0.0f};
    config.setConvolveKernel(sharpen);
    config.setConvolveKernelSize(3);
    config.setConvolveNormalize(true);
    config.setConvolveAmount(0.6f);

    const nlohmann::json json = config;
    const FilterConfiguration roundTripped = json.get<FilterConfiguration>();

    REQUIRE(roundTripped.type() == FilterType::Convolve);
    REQUIRE(roundTripped.channelBalance() == 0.3f);
    REQUIRE(roundTripped.convolveKernel() == sharpen);
    REQUIRE(roundTripped.convolveKernelSize() == 3);
    REQUIRE(roundTripped.convolveNormalize());
    REQUIRE(roundTripped.convolveAmount() == 0.6f);
}

TEST_CASE("A FilterConfiguration loads from JSON missing every ChannelBalance/Convolve key "
          "(a configuration saved before v0.Y.36.1 Installment B) using sensible defaults",
          "[core][filter_configuration]") {
    const nlohmann::json json{{"type", "uniformBlur"},
                               {"blurSigma", 2.0f},
                               {"medianSize", 3},
                               {"directionalBlurLength", 10},
                               {"directionalBlurAngleDegrees", 0.0f},
                               {"sharpenAmount", 1.0f},
                               {"toneCurvePoints", std::vector<std::array<float, 2>>{{0.0f, 0.0f}, {1.0f, 1.0f}}},
                               {"frequencyGradient", FilterConfiguration{}.frequencyGradient()}};

    const FilterConfiguration restored = json.get<FilterConfiguration>();

    REQUIRE(restored.channelBalance() == 0.5f);
    REQUIRE(restored.convolveKernel() == std::vector<float>{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    REQUIRE(restored.convolveKernelSize() == 3);
    REQUIRE_FALSE(restored.convolveNormalize());
    REQUIRE(restored.convolveAmount() == 1.0f);
}

// --- v0.Y.36.1 Installment C: Geometric --------------------------------

TEST_CASE("A fresh FilterConfiguration has sensible Displace/ChannelCycle defaults",
          "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.displaceDistance() == 10.0f);
    REQUIRE(config.displaceAngleDegrees() == 0.0f);
    REQUIRE(config.channelCycleAngleDegrees() == 0.0f);
}

TEST_CASE("Every new Geometric FilterType can be set", "[core][filter_configuration]") {
    FilterConfiguration config;
    for (const FilterType type : {FilterType::Displace, FilterType::ChannelCycle}) {
        config.setType(type);
        REQUIRE(config.type() == type);
    }
}

TEST_CASE("A FilterConfiguration's Displace/ChannelCycle parameters can be changed",
          "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setDisplaceDistance(25.0f);
    config.setDisplaceAngleDegrees(90.0f);
    config.setChannelCycleAngleDegrees(120.0f);

    REQUIRE(config.displaceDistance() == 25.0f);
    REQUIRE(config.displaceAngleDegrees() == 90.0f);
    REQUIRE(config.channelCycleAngleDegrees() == 120.0f);
}

TEST_CASE("A FilterConfiguration's Displace/ChannelCycle parameters round-trip through JSON",
          "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::Displace);
    config.setDisplaceDistance(25.0f);
    config.setDisplaceAngleDegrees(90.0f);
    config.setChannelCycleAngleDegrees(120.0f);

    const nlohmann::json json = config;
    const FilterConfiguration roundTripped = json.get<FilterConfiguration>();

    REQUIRE(roundTripped.type() == FilterType::Displace);
    REQUIRE(roundTripped.displaceDistance() == 25.0f);
    REQUIRE(roundTripped.displaceAngleDegrees() == 90.0f);
    REQUIRE(roundTripped.channelCycleAngleDegrees() == 120.0f);
}

TEST_CASE("A FilterConfiguration loads from JSON missing every Displace/ChannelCycle key "
          "(a configuration saved before v0.Y.36.1 Installment C) using sensible defaults",
          "[core][filter_configuration]") {
    const nlohmann::json json{{"type", "uniformBlur"},
                               {"blurSigma", 2.0f},
                               {"medianSize", 3},
                               {"directionalBlurLength", 10},
                               {"directionalBlurAngleDegrees", 0.0f},
                               {"sharpenAmount", 1.0f},
                               {"toneCurvePoints", std::vector<std::array<float, 2>>{{0.0f, 0.0f}, {1.0f, 1.0f}}},
                               {"frequencyGradient", FilterConfiguration{}.frequencyGradient()}};

    const FilterConfiguration restored = json.get<FilterConfiguration>();

    REQUIRE(restored.displaceDistance() == 10.0f);
    REQUIRE(restored.displaceAngleDegrees() == 0.0f);
    REQUIRE(restored.channelCycleAngleDegrees() == 0.0f);
}

// --- v0.Y.36.1 Installment D: Space -------------------------------------

TEST_CASE("A fresh FilterConfiguration has sensible SpectralReverb defaults", "[core][filter_configuration]") {
    const FilterConfiguration config;
    REQUIRE(config.reverbPreDelayFrames() == 2);
    REQUIRE(config.reverbDecayFrames() == 40);
    REQUIRE(config.reverbRoomSize() == 0.6f);
    REQUIRE(config.reverbDiffusion() == 0.5f);
    REQUIRE(config.reverbAbsorption() == 0.4f);
    REQUIRE(config.reverbMix() == 0.4f);
}

TEST_CASE("SpectralReverb FilterType can be set", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::SpectralReverb);
    REQUIRE(config.type() == FilterType::SpectralReverb);
}

TEST_CASE("A FilterConfiguration's SpectralReverb parameters can be changed", "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setReverbPreDelayFrames(5);
    config.setReverbDecayFrames(80);
    config.setReverbRoomSize(0.9f);
    config.setReverbDiffusion(0.2f);
    config.setReverbAbsorption(0.7f);
    config.setReverbMix(0.8f);

    REQUIRE(config.reverbPreDelayFrames() == 5);
    REQUIRE(config.reverbDecayFrames() == 80);
    REQUIRE(config.reverbRoomSize() == 0.9f);
    REQUIRE(config.reverbDiffusion() == 0.2f);
    REQUIRE(config.reverbAbsorption() == 0.7f);
    REQUIRE(config.reverbMix() == 0.8f);
}

TEST_CASE("A FilterConfiguration's SpectralReverb parameters round-trip through JSON",
          "[core][filter_configuration]") {
    FilterConfiguration config;
    config.setType(FilterType::SpectralReverb);
    config.setReverbPreDelayFrames(5);
    config.setReverbDecayFrames(80);
    config.setReverbRoomSize(0.9f);
    config.setReverbDiffusion(0.2f);
    config.setReverbAbsorption(0.7f);
    config.setReverbMix(0.8f);

    const nlohmann::json json = config;
    const FilterConfiguration roundTripped = json.get<FilterConfiguration>();

    REQUIRE(roundTripped.type() == FilterType::SpectralReverb);
    REQUIRE(roundTripped.reverbPreDelayFrames() == 5);
    REQUIRE(roundTripped.reverbDecayFrames() == 80);
    REQUIRE(roundTripped.reverbRoomSize() == 0.9f);
    REQUIRE(roundTripped.reverbDiffusion() == 0.2f);
    REQUIRE(roundTripped.reverbAbsorption() == 0.7f);
    REQUIRE(roundTripped.reverbMix() == 0.8f);
}

TEST_CASE("A FilterConfiguration loads from JSON missing every SpectralReverb key "
          "(a configuration saved before v0.Y.36.1 Installment D) using sensible defaults",
          "[core][filter_configuration]") {
    const nlohmann::json json{{"type", "uniformBlur"},
                               {"blurSigma", 2.0f},
                               {"medianSize", 3},
                               {"directionalBlurLength", 10},
                               {"directionalBlurAngleDegrees", 0.0f},
                               {"sharpenAmount", 1.0f},
                               {"toneCurvePoints", std::vector<std::array<float, 2>>{{0.0f, 0.0f}, {1.0f, 1.0f}}},
                               {"frequencyGradient", FilterConfiguration{}.frequencyGradient()}};

    const FilterConfiguration restored = json.get<FilterConfiguration>();

    REQUIRE(restored.reverbPreDelayFrames() == 2);
    REQUIRE(restored.reverbDecayFrames() == 40);
    REQUIRE(restored.reverbRoomSize() == 0.6f);
    REQUIRE(restored.reverbDiffusion() == 0.5f);
    REQUIRE(restored.reverbAbsorption() == 0.4f);
    REQUIRE(restored.reverbMix() == 0.4f);
}
