#include <cstddef>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::applyFilter;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::ProjectSettings;

namespace {

/// @brief A 3-bin, 2-column StreamImage with uniform left/right dB and a
/// distinctive, easy-to-check phase - small enough to hand-verify every
/// cell of applyFilter()'s own output.
StreamImage makeComposite() {
    StreamImage composite;
    composite.config.binCount = 3;
    composite.frameCount = 2;
    composite.leftMagnitudeDb.assign(6, -20.0f);
    composite.rightMagnitudeDb.assign(6, -10.0f);
    composite.sharedPhaseRadians.assign(6, 0.5f);
    return composite;
}

}  // namespace

TEST_CASE("applyFilter's FrequencyAxisGradient blends each bin toward the gradient's own stop at that bin",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::FrequencyAxisGradient);
    auto& gradient = config.frequencyGradient();
    // t=0 (bin 0, lowest frequency): force left to 0 dB, leave right alone.
    gradient.setStopValues(0, {0.0f, 0.0f, -96.0f, 1.0f, 0.0f});
    // t=1 (bin 2, highest frequency): leave left alone, force right to 0 dB.
    gradient.setStopValues(1, {1.0f, -96.0f, 0.0f, 0.0f, 1.0f});

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    REQUIRE(filtered.config.binCount == 3);
    REQUIRE(filtered.frameCount == 2);

    // Bin 0 (t=0): left forced to 0 dB, right unchanged (-10 dB).
    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(0.0f));
    CHECK(filtered.leftMagnitudeDb[1] == Catch::Approx(0.0f));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(-10.0f));
    CHECK(filtered.rightMagnitudeDb[1] == Catch::Approx(-10.0f));

    // Bin 1 (t=0.5): halfway between the original value and the
    // interpolated stop (-48 dB intensity, 0.5 opacity on both channels).
    const std::size_t bin1 = 1 * 2;
    CHECK(filtered.leftMagnitudeDb[bin1] == Catch::Approx(-34.0f));
    CHECK(filtered.rightMagnitudeDb[bin1] == Catch::Approx(-29.0f));

    // Bin 2 (t=1): left unchanged (-20 dB), right forced to 0 dB.
    const std::size_t bin2 = 2 * 2;
    CHECK(filtered.leftMagnitudeDb[bin2] == Catch::Approx(-20.0f));
    CHECK(filtered.leftMagnitudeDb[bin2 + 1] == Catch::Approx(-20.0f));
    CHECK(filtered.rightMagnitudeDb[bin2] == Catch::Approx(0.0f));
    CHECK(filtered.rightMagnitudeDb[bin2 + 1] == Catch::Approx(0.0f));
}

TEST_CASE("applyFilter's FrequencyAxisGradient leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::FrequencyAxisGradient);
    config.frequencyGradient().setStopValues(0, {0.0f, 0.0f, 0.0f, 1.0f, 1.0f});

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

TEST_CASE("applyFilter is a harmless passthrough for a filter type not implemented yet",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ToneCurve);  // Installment C's own scope, not yet built.

    const auto composite = makeComposite();
    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
    CHECK(filtered.rightMagnitudeDb == composite.rightMagnitudeDb);
    CHECK(filtered.sharedPhaseRadians == composite.sharedPhaseRadians);
}

// ---------------------------------------------------------------------------
// UniformBlur (Gaussian blur) - Installment B.
// ---------------------------------------------------------------------------

/// @brief A 1-bin composite - collapses the 2D separable blur's own vertical
/// pass to a no-op (a single row blurred against itself, clamped), so these
/// cases exercise only the horizontal (frame-axis) pass and stay hand-checkable.
StreamImage makeSingleRowComposite(std::vector<float> leftDb) {
    StreamImage composite;
    composite.config.binCount = 1;
    composite.frameCount = static_cast<std::uint32_t>(leftDb.size());
    composite.rightMagnitudeDb = leftDb;  // Same values - both channels get checked.
    composite.leftMagnitudeDb = std::move(leftDb);
    composite.sharedPhaseRadians.assign(composite.frameCount, 0.75f);
    return composite;
}

TEST_CASE("applyFilter's UniformBlur leaves a uniform composite unchanged", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(2.0f);

    const auto filtered = applyFilter(makeSingleRowComposite({-40.0f, -40.0f, -40.0f, -40.0f, -40.0f}), config,
                                       ProjectSettings{});

    for (const float db : filtered.leftMagnitudeDb) {
        CHECK(db == Catch::Approx(-40.0f));
    }
}

TEST_CASE("applyFilter's UniformBlur spreads an impulse toward its neighbors, symmetrically",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(1.0f);

    // A single 0 dB impulse against a -96 dB floor, centered in a 9-column row.
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto filtered = applyFilter(makeSingleRowComposite(impulse), config, ProjectSettings{});

    // Reference values from this filter's own documented formula (a
    // sigma=1.0 Gaussian kernel truncated at 4 standard deviations,
    // clamp-to-edge boundary), computed independently ahead of this test.
    const std::vector<float> expected = {-95.987152f, -95.574541f, -90.816852f, -72.770741f,
                                          -57.701427f, -72.770741f, -90.816852f, -95.574541f, -95.987152f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(expected[i]).margin(0.001));
        CHECK(filtered.rightMagnitudeDb[i] == Catch::Approx(expected[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's UniformBlur leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(3.0f);

    const auto filtered =
        applyFilter(makeSingleRowComposite({-10.0f, -20.0f, -30.0f}), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.75f);
    }
}

// ---------------------------------------------------------------------------
// EdgePreservingBlur (median blur) - Installment B.
// ---------------------------------------------------------------------------

TEST_CASE("applyFilter's EdgePreservingBlur removes a lone spike surrounded by a flat floor",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::EdgePreservingBlur);
    config.setMedianSize(3);

    const auto filtered =
        applyFilter(makeSingleRowComposite({-96.0f, -96.0f, 0.0f, -96.0f, -96.0f}), config, ProjectSettings{});

    for (const float db : filtered.leftMagnitudeDb) {
        CHECK(db == Catch::Approx(-96.0f));
    }
}

TEST_CASE("applyFilter's EdgePreservingBlur preserves a real step edge instead of smoothing across it",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::EdgePreservingBlur);
    config.setMedianSize(3);

    const std::vector<float> step = {-96.0f, -96.0f, -96.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const auto filtered = applyFilter(makeSingleRowComposite(step), config, ProjectSettings{});

    // Median blur is edge-preserving: a clean step is already its own
    // median everywhere, so it comes back bit-for-bit unchanged - unlike a
    // Gaussian blur, which would smear an intermediate value across the
    // boundary column.
    for (std::size_t i = 0; i < step.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(step[i]));
    }
}

// ---------------------------------------------------------------------------
// DirectionalBlur (motion blur) - Installment B.
// ---------------------------------------------------------------------------

TEST_CASE("applyFilter's DirectionalBlur at 0 degrees blurs along the time axis (columns)",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(1);
    config.setDirectionalBlurAngleDegrees(0.0f);

    // A 3-tap box average (length=1 -> 3 samples: -1, 0, +1 columns), clamped
    // at the edges - deliberately a ramp whose thirds divide evenly.
    const auto filtered =
        applyFilter(makeSingleRowComposite({0.0f, -12.0f, -24.0f, -36.0f, -48.0f}), config, ProjectSettings{});

    const std::vector<float> expected = {-4.0f, -12.0f, -24.0f, -36.0f, -44.0f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(expected[i]));
    }
}

TEST_CASE("applyFilter's DirectionalBlur at 90 degrees blurs along the frequency axis (bins) instead",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(1);
    config.setDirectionalBlurAngleDegrees(90.0f);

    StreamImage composite;
    composite.config.binCount = 5;
    composite.frameCount = 1;
    composite.leftMagnitudeDb = {0.0f, -12.0f, -24.0f, -36.0f, -48.0f};
    composite.rightMagnitudeDb = composite.leftMagnitudeDb;
    composite.sharedPhaseRadians.assign(5, 0.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    // Same 3-tap box average as the 0-degree case, but now down the bin
    // axis instead of across the frame axis - confirms the angle convention
    // documented on FilterConfiguration::directionalBlurAngleDegrees().
    const std::vector<float> expected = {-4.0f, -12.0f, -24.0f, -36.0f, -44.0f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(expected[i]).margin(0.001));
    }
}

// ---------------------------------------------------------------------------
// Sharpen (unsharp mask) - Installment B.
// ---------------------------------------------------------------------------

TEST_CASE("applyFilter's Sharpen leaves a uniform composite unchanged", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setSharpenAmount(1.0f);

    const auto filtered =
        applyFilter(makeSingleRowComposite({-40.0f, -40.0f, -40.0f, -40.0f}), config, ProjectSettings{});

    for (const float db : filtered.leftMagnitudeDb) {
        CHECK(db == Catch::Approx(-40.0f));
    }
}

TEST_CASE("applyFilter's Sharpen exaggerates an impulse against its own (fixed-sigma) blurred version",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setSharpenAmount(1.0f);

    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto filtered = applyFilter(makeSingleRowComposite(impulse), config, ProjectSettings{});

    // Reference values: grid + amount*(grid - gaussianBlur(grid, sigma=1.0)),
    // using this same sigma=1.0 kernel as the UniformBlur test above,
    // computed independently ahead of this test.
    const std::vector<float> expected = {-96.012848f, -96.425459f, -101.183148f, -119.229259f,
                                          57.701427f,  -119.229259f, -101.183148f, -96.425459f, -96.012848f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(expected[i]).margin(0.001));
    }
}
