#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/gpu_compute_availability.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::applyFilter;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterParameterMindWaves;
using sound_mind::core::FilterType;
using sound_mind::core::MindWave;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::ProjectSettings;
using sound_mind::core::setGpuComputeForcedOffForTesting;

namespace {

/// @brief Forces the CPU fallback path (`setGpuComputeForcedOffForTesting(true)`)
/// for its own scope, restoring normal GPU-when-available behavior when it
/// goes out of scope - even if the test fails partway through, so this
/// override never leaks into whichever test Catch2 runs next.
struct GpuComputeForcedOffGuard {
    GpuComputeForcedOffGuard() { setGpuComputeForcedOffForTesting(true); }
    ~GpuComputeForcedOffGuard() { setGpuComputeForcedOffForTesting(false); }
};

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

// ---------------------------------------------------------------------------
// ToneCurve - Installment C.
// ---------------------------------------------------------------------------

TEST_CASE("applyFilter's ToneCurve leaves dB values unchanged for the default identity curve",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ToneCurve);  // Default toneCurvePoints(): {0,0},{1,1} - the identity curve.

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-20.0f));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(-10.0f));
}

TEST_CASE("applyFilter's ToneCurve remaps each channel's own dB value independently through the curve",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ToneCurve);
    config.setToneCurvePoints({{0.0f, 0.0f}, {0.5f, 0.2f}, {1.0f, 1.0f}});

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    // Reference values from this filter's own documented remap (dB ->
    // normalized [0,1] -> evaluateToneCurve() -> dB), computed
    // independently ahead of this test. Left is -20 dB, right is -10 dB
    // (makeComposite()'s own values) - each channel's own value drives
    // its own point on the curve.
    for (const float db : filtered.leftMagnitudeDb) {
        CHECK(db == Catch::Approx(-34.91666667f).margin(0.001));
    }
    for (const float db : filtered.rightMagnitudeDb) {
        CHECK(db == Catch::Approx(-16.98958333f).margin(0.001));
    }
}

TEST_CASE("applyFilter's ToneCurve leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ToneCurve);
    config.setToneCurvePoints({{0.0f, 0.0f}, {0.5f, 0.9f}, {1.0f, 1.0f}});

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
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

// --- v0.Y.31.1 Installment D: filter-parameter MindWave bindings ------
//
// A Time-axis Square wave (see MindWaveTest's own precedent in
// test_mind_wave.cpp) with a huge period stays in its own "high" half
// (field 1.0) for any t this file's own small composites ever reach - the
// ceiling value everywhere. Shifting phase by pi starts it in the "low"
// half instead (field 0.0) - the baseline everywhere. A period of exactly
// two columns' own time (10 ms apart, per makeSingleRowComposite()'s own
// default StreamCodecConfig - sampleRateHz=44100, hopLength=441)
// alternates high/low every column, for a genuine per-cell-varying check.

MindWave alwaysCeilingWave() {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Square);
    wave.setPeriod(1'000'000.0);
    return wave;
}

MindWave alwaysBaselineWave() {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Square);
    wave.setPeriod(1'000'000.0);
    wave.setPhaseRadians(std::numbers::pi_v<double>);
    return wave;
}

MindWave alternatingColumnsWave() {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Square);
    wave.setPeriod(0.02);  // Two columns' own time, at the default 10 ms/column.
    return wave;
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

TEST_CASE("applyFilter's UniformBlur produces the same result via its CPU fallback as via the GPU "
          "(GPU wiring - Installment C)",
          "[core][filter_application][gpu]") {
    GpuComputeForcedOffGuard forceCpu;

    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(1.0f);

    // Same impulse and same expected values as the GPU-preferred test
    // above - this is the same math, run through the CPU fallback branch
    // instead, confirmed identical (see docs/sound-mind-architecture.md's
    // Decision #4 testing strategy: the GPU path and the CPU fallback
    // must produce equivalent results).
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto filtered = applyFilter(makeSingleRowComposite(impulse), config, ProjectSettings{});

    const std::vector<float> expected = {-95.987152f, -95.574541f, -90.816852f, -72.770741f,
                                          -57.701427f, -72.770741f, -90.816852f, -95.574541f, -95.987152f};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(expected[i]).margin(0.001));
        CHECK(filtered.rightMagnitudeDb[i] == Catch::Approx(expected[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's UniformBlur agrees with itself whether or not the GPU is available "
          "(GPU wiring - Installment C)",
          "[core][filter_application][gpu]") {
    // A bigger, more varied grid than the hand-derived single-row cases
    // above - not checked against an independently-computed expected
    // value (sound-mind-gpu's own tests already do that, against a
    // from-scratch CPU reference), but cross-checked against *this*
    // production CPU implementation, GPU-preferred vs. forced-off,
    // confirming the wiring itself (not just the math in isolation)
    // agrees with its own fallback.
    constexpr std::uint32_t binCount = 6;
    constexpr std::uint32_t frameCount = 10;
    StreamImage composite;
    composite.config.binCount = binCount;
    composite.frameCount = frameCount;
    composite.leftMagnitudeDb.resize(std::size_t{binCount} * frameCount);
    composite.rightMagnitudeDb.resize(std::size_t{binCount} * frameCount);
    for (std::size_t i = 0; i < composite.leftMagnitudeDb.size(); ++i) {
        // A deterministic, non-uniform pattern (not literally random, so
        // the test stays reproducible) - varied enough to exercise every
        // part of the separable kernel, not just a flat or single-impulse
        // input.
        composite.leftMagnitudeDb[i] = -50.0f + 40.0f * std::sin(static_cast<float>(i) * 0.7f);
        composite.rightMagnitudeDb[i] = -30.0f + 20.0f * std::cos(static_cast<float>(i) * 1.3f);
    }
    composite.sharedPhaseRadians.assign(composite.leftMagnitudeDb.size(), 0.0f);

    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(2.5f);

    const auto gpuFiltered = applyFilter(composite, config, ProjectSettings{});
    std::vector<float> cpuLeft;
    std::vector<float> cpuRight;
    {
        GpuComputeForcedOffGuard forceCpu;
        const auto cpuFiltered = applyFilter(composite, config, ProjectSettings{});
        cpuLeft = cpuFiltered.leftMagnitudeDb;
        cpuRight = cpuFiltered.rightMagnitudeDb;
    }

    REQUIRE(gpuFiltered.leftMagnitudeDb.size() == cpuLeft.size());
    for (std::size_t i = 0; i < cpuLeft.size(); ++i) {
        CHECK(gpuFiltered.leftMagnitudeDb[i] == Catch::Approx(cpuLeft[i]).margin(0.01));
        CHECK(gpuFiltered.rightMagnitudeDb[i] == Catch::Approx(cpuRight[i]).margin(0.01));
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

TEST_CASE("applyFilter's UniformBlur, bound to a MindWave always at ceiling, matches the fixed sigma result",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(1.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto unbound = applyFilter(composite, config, ProjectSettings{});
    const auto ceilingWave = alwaysCeilingWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.blurSigma = &ceilingWave});

    for (std::size_t i = 0; i < unbound.leftMagnitudeDb.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(unbound.leftMagnitudeDb[i]).margin(0.01));
    }
}

TEST_CASE("applyFilter's UniformBlur, bound to a MindWave always at baseline, leaves the composite unchanged",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(3.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto baselineWave = alwaysBaselineWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.blurSigma = &baselineWave});

    for (std::size_t i = 0; i < impulse.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(impulse[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's UniformBlur, bound to an alternating MindWave, varies genuinely per column",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(2.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.blurSigma = &wave});
    const auto fullyBlurred = applyFilter(composite, config, ProjectSettings{});

    // Column 4 (the impulse's own column, high field - see
    // alternatingColumnsWave()'s own docs) should read close to the fully-
    // blurred result; column 5 (odd, low field) should stay close to the
    // untouched impulse floor - the two must differ from each other.
    CHECK(bound.leftMagnitudeDb[4] == Catch::Approx(fullyBlurred.leftMagnitudeDb[4]).margin(0.5));
    CHECK(bound.leftMagnitudeDb[5] == Catch::Approx(impulse[5]).margin(0.5));
    CHECK(bound.leftMagnitudeDb[4] != Catch::Approx(bound.leftMagnitudeDb[5]));
}

TEST_CASE("applyFilter's UniformBlur, bound to a MindWave, leaves phase untouched",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(2.0f);
    const auto composite = makeSingleRowComposite({-10.0f, -20.0f, -30.0f, -40.0f});

    const auto wave = alternatingColumnsWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.blurSigma = &wave});

    for (const float phase : bound.sharedPhaseRadians) {
        CHECK(phase == 0.75f);
    }
}

TEST_CASE("applyFilter's EdgePreservingBlur, bound to a MindWave always at ceiling, matches the fixed size result",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::EdgePreservingBlur);
    config.setMedianSize(5);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto unbound = applyFilter(composite, config, ProjectSettings{});
    const auto ceilingWave = alwaysCeilingWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.medianSize = &ceilingWave});

    for (std::size_t i = 0; i < unbound.leftMagnitudeDb.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(unbound.leftMagnitudeDb[i]).margin(0.01));
    }
}

TEST_CASE("applyFilter's EdgePreservingBlur, bound to a MindWave always at baseline, leaves the composite unchanged",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::EdgePreservingBlur);
    config.setMedianSize(5);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto baselineWave = alwaysBaselineWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.medianSize = &baselineWave});

    for (std::size_t i = 0; i < impulse.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(impulse[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's EdgePreservingBlur, bound to an alternating MindWave, varies genuinely per column",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::EdgePreservingBlur);
    config.setMedianSize(5);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.medianSize = &wave});

    // Column 4 (high field) gets a real median window and removes the lone
    // impulse; column 5 (low field, baseline) stays exactly as it was.
    CHECK(bound.leftMagnitudeDb[4] == Catch::Approx(-96.0f).margin(0.001));
    CHECK(bound.leftMagnitudeDb[5] == Catch::Approx(impulse[5]).margin(0.001));
}

TEST_CASE("applyFilter's DirectionalBlur, bound to a MindWave always at ceiling, matches the fixed length result",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(3);
    config.setDirectionalBlurAngleDegrees(0.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto unbound = applyFilter(composite, config, ProjectSettings{});
    const auto ceilingWave = alwaysCeilingWave();
    const auto bound =
        applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.directionalBlurLength = &ceilingWave});

    for (std::size_t i = 0; i < unbound.leftMagnitudeDb.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(unbound.leftMagnitudeDb[i]).margin(0.01));
    }
}

TEST_CASE("applyFilter's DirectionalBlur, bound to a MindWave always at baseline, leaves the composite unchanged",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(5);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto baselineWave = alwaysBaselineWave();
    const auto bound =
        applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.directionalBlurLength = &baselineWave});

    for (std::size_t i = 0; i < impulse.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(impulse[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's DirectionalBlur, bound to an alternating MindWave, varies genuinely per column",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(3);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;  // Column 4 is even - alternatingColumnsWave()'s own high field.
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound =
        applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.directionalBlurLength = &wave});

    // Each output cell's own kernel length comes from *that* cell's own
    // field value, not its neighbors' - column 4's own high field blurs
    // its own output (pulling in the surrounding floor); columns 3/5's own
    // low field means their own output is untouched, regardless of the
    // impulse sitting right next to them.
    CHECK(bound.leftMagnitudeDb[4] != Catch::Approx(impulse[4]));
    CHECK(bound.leftMagnitudeDb[3] == Catch::Approx(impulse[3]).margin(0.001));
    CHECK(bound.leftMagnitudeDb[5] == Catch::Approx(impulse[5]).margin(0.001));
}

TEST_CASE("applyFilter's DirectionalBlur binds length and angle independently",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(3);
    config.setDirectionalBlurAngleDegrees(90.0f);  // Along the frequency axis - irrelevant on a single-row composite.
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    // Length bound (varies), angle left fixed at its own configured 90 -
    // a single-row composite has no frequency-axis neighbors, so a 90-
    // degree blur is a no-op regardless of length.
    const auto lengthWave = alwaysCeilingWave();
    const auto bound =
        applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.directionalBlurLength = &lengthWave});

    for (std::size_t i = 0; i < impulse.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(impulse[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's Sharpen, bound to a MindWave always at ceiling, matches the fixed amount result",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setSharpenAmount(1.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto unbound = applyFilter(composite, config, ProjectSettings{});
    const auto ceilingWave = alwaysCeilingWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.sharpenAmount = &ceilingWave});

    for (std::size_t i = 0; i < unbound.leftMagnitudeDb.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(unbound.leftMagnitudeDb[i]).margin(0.01));
    }
}

TEST_CASE("applyFilter's Sharpen, bound to a MindWave always at baseline, leaves the composite unchanged",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setSharpenAmount(2.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto baselineWave = alwaysBaselineWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.sharpenAmount = &baselineWave});

    for (std::size_t i = 0; i < impulse.size(); ++i) {
        CHECK(bound.leftMagnitudeDb[i] == Catch::Approx(impulse[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's Sharpen, bound to an alternating MindWave, varies genuinely per column",
          "[core][filter_application][mind_wave]") {
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setSharpenAmount(1.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.sharpenAmount = &wave});
    const auto fullySharpened = applyFilter(composite, config, ProjectSettings{});

    CHECK(bound.leftMagnitudeDb[4] == Catch::Approx(fullySharpened.leftMagnitudeDb[4]).margin(0.001));
    CHECK(bound.leftMagnitudeDb[5] == Catch::Approx(impulse[5]).margin(0.001));
}
