#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/filter_configuration.h"

using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;

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
