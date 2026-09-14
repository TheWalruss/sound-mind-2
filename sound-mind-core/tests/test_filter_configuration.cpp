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
