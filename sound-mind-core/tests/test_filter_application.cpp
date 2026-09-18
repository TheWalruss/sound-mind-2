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

TEST_CASE("applyFilter's MindWave-bound blurSigma agrees with itself via the CPU fallback as via the GPU "
          "(GPU wiring - Installment D2)",
          "[core][filter_application][mind_wave][gpu]") {
    GpuComputeForcedOffGuard forceCpu;

    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(2.0f);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.blurSigma = &wave});

    // Same scenario, same expected shape, as the GPU-preferred test above -
    // run through the CPU fallback branch instead (Decision #4's own
    // testing strategy: the GPU path and the CPU fallback must agree).
    CHECK(bound.leftMagnitudeDb[5] == Catch::Approx(impulse[5]).margin(0.5));
    CHECK(bound.leftMagnitudeDb[4] != Catch::Approx(impulse[4]));
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

TEST_CASE("applyFilter's MindWave-bound medianSize agrees with itself via the CPU fallback as via the GPU "
          "(GPU wiring - Installment D2)",
          "[core][filter_application][mind_wave][gpu]") {
    GpuComputeForcedOffGuard forceCpu;

    FilterConfiguration config;
    config.setType(FilterType::EdgePreservingBlur);
    config.setMedianSize(5);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound = applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.medianSize = &wave});

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

TEST_CASE("applyFilter's MindWave-bound directionalBlurLength agrees with itself via the CPU fallback as via "
          "the GPU (GPU wiring - Installment D2)",
          "[core][filter_application][mind_wave][gpu]") {
    GpuComputeForcedOffGuard forceCpu;

    FilterConfiguration config;
    config.setType(FilterType::DirectionalBlur);
    config.setDirectionalBlurLength(3);
    std::vector<float> impulse(9, -96.0f);
    impulse[4] = 0.0f;
    const auto composite = makeSingleRowComposite(impulse);

    const auto wave = alternatingColumnsWave();
    const auto bound =
        applyFilter(composite, config, ProjectSettings{}, FilterParameterMindWaves{.directionalBlurLength = &wave});

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

// ---------------------------------------------------------------------------
// Noise & distortion - v0.Y.36.1 Installment A.
// ---------------------------------------------------------------------------

namespace {

/// @brief A uniform `binCount` x `frameCount` composite - large enough for
/// the statistical (density-based) Noise & distortion cases below to stay
/// robust rather than flaky.
StreamImage makeUniformGridComposite(std::uint32_t binCount, std::uint32_t frameCount, float leftDb, float rightDb) {
    StreamImage composite;
    composite.config.binCount = binCount;
    composite.frameCount = frameCount;
    const std::size_t cellCount = std::size_t{binCount} * frameCount;
    composite.leftMagnitudeDb.assign(cellCount, leftDb);
    composite.rightMagnitudeDb.assign(cellCount, rightDb);
    composite.sharedPhaseRadians.assign(cellCount, 0.5f);
    return composite;
}

}  // namespace

TEST_CASE("applyFilter's SpeckleAdd is a no-op at zero density", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpeckleAdd);
    config.setSpeckleDensity(0.0f);
    config.setSpeckleIntensity(1.0f);
    const auto composite = makeUniformGridComposite(10, 10, -40.0f, -30.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
}

TEST_CASE("applyFilter's SpeckleAdd is a no-op at zero intensity", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpeckleAdd);
    config.setSpeckleDensity(1.0f);
    config.setSpeckleIntensity(0.0f);
    const auto composite = makeUniformGridComposite(10, 10, -40.0f, -30.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
}

TEST_CASE("applyFilter's SpeckleAdd hits roughly its own configured density of cells, deterministically",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpeckleAdd);
    config.setNoiseSeed(7u);
    config.setSpeckleDensity(0.5f);
    config.setSpeckleIntensity(1.0f);  // full jump to 0dB, easy to detect.
    const auto composite = makeUniformGridComposite(20, 20, -40.0f, -40.0f);

    const auto first = applyFilter(composite, config, ProjectSettings{});
    const auto second = applyFilter(composite, config, ProjectSettings{});

    // Deterministic - the same configuration applied twice agrees exactly.
    REQUIRE(first.leftMagnitudeDb == second.leftMagnitudeDb);

    const auto hitCount = std::count(first.leftMagnitudeDb.begin(), first.leftMagnitudeDb.end(), 0.0f);
    const auto total = static_cast<std::ptrdiff_t>(first.leftMagnitudeDb.size());
    // A generous band around the configured 50% density - 400 independent
    // cells at p=0.5 landing outside [30%, 70%] is astronomically unlikely.
    CHECK(hitCount > total * 3 / 10);
    CHECK(hitCount < total * 7 / 10);
}

TEST_CASE("applyFilter's SpeckleAdd produces a different pattern for a different noiseSeed",
          "[core][filter_application]") {
    FilterConfiguration configA;
    configA.setType(FilterType::SpeckleAdd);
    configA.setNoiseSeed(1u);
    configA.setSpeckleDensity(0.5f);
    configA.setSpeckleIntensity(1.0f);
    FilterConfiguration configB = configA;
    configB.setNoiseSeed(2u);
    const auto composite = makeUniformGridComposite(20, 20, -40.0f, -40.0f);

    const auto filteredA = applyFilter(composite, configA, ProjectSettings{});
    const auto filteredB = applyFilter(composite, configB, ProjectSettings{});

    CHECK(filteredA.leftMagnitudeDb != filteredB.leftMagnitudeDb);
}

TEST_CASE("applyFilter's SpeckleAdd leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpeckleAdd);
    config.setSpeckleDensity(1.0f);
    config.setSpeckleIntensity(1.0f);

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

TEST_CASE("applyFilter's SpeckleRemove replaces an isolated outlier with its local median",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpeckleRemove);
    config.setSpeckleThresholdDb(10.0f);
    auto composite = makeUniformGridComposite(5, 5, -40.0f, -40.0f);
    const std::size_t spikeCell = 2 * 5 + 2;  // dead center - a full 3x3 neighborhood.
    composite.leftMagnitudeDb[spikeCell] = 0.0f;  // a 40dB outlier, well past the threshold.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[spikeCell] == Catch::Approx(-40.0f));
}

TEST_CASE("applyFilter's SpeckleRemove leaves a cell within threshold of its local median untouched",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpeckleRemove);
    config.setSpeckleThresholdDb(10.0f);
    auto composite = makeUniformGridComposite(5, 5, -40.0f, -40.0f);
    const std::size_t cell = 2 * 5 + 2;
    composite.leftMagnitudeDb[cell] = -35.0f;  // only 5dB off - within the 10dB threshold.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[cell] == Catch::Approx(-35.0f));
}

TEST_CASE("applyFilter's Denoise fully attenuates a cell well below the noise floor",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Denoise);
    config.setNoiseFloorDb(-60.0f);
    config.setReductionDb(24.0f);
    const auto composite = makeUniformGridComposite(3, 3, -80.0f, -80.0f);  // 20dB below the floor.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-104.0f));  // full 24dB reduction applied.
}

TEST_CASE("applyFilter's Denoise leaves a cell well above the noise floor untouched",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Denoise);
    config.setNoiseFloorDb(-60.0f);
    config.setReductionDb(24.0f);
    const auto composite = makeUniformGridComposite(3, 3, -20.0f, -20.0f);  // well above the floor.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-20.0f));
}

TEST_CASE("applyFilter's Denoise applies half its own reduction exactly at the noise floor",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Denoise);
    config.setNoiseFloorDb(-60.0f);
    config.setReductionDb(24.0f);
    const auto composite = makeUniformGridComposite(3, 3, -60.0f, -60.0f);  // exactly at the floor - the knee's center.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-72.0f));  // -60 - (0.5 * 24).
}

TEST_CASE("applyFilter's BitDepthCrush is a no-op at crushAmount 0", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::BitDepthCrush);
    config.setCrushAmount(0.0f);
    const auto composite = makeUniformGridComposite(3, 3, -37.25f, -12.8f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
    CHECK(filtered.rightMagnitudeDb == composite.rightMagnitudeDb);
}

TEST_CASE("applyFilter's BitDepthCrush collapses nearby values onto the same quantized step at full amount",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::BitDepthCrush);
    config.setCrushAmount(1.0f);  // levels = 2 - the harshest setting.
    auto composite = makeUniformGridComposite(1, 2, 0.0f, 0.0f);
    composite.leftMagnitudeDb = {-10.0f, -20.0f};  // both well within the same half of the range.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(filtered.leftMagnitudeDb[1]));
}

TEST_CASE("applyFilter's BitDepthCrush leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::BitDepthCrush);
    config.setCrushAmount(1.0f);

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

TEST_CASE("applyFilter's GranularNoise is a no-op at zero amount", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::GranularNoise);
    config.setGrainAmountDb(0.0f);
    const auto composite = makeUniformGridComposite(10, 10, -40.0f, -30.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
}

TEST_CASE("applyFilter's GranularNoise applies the exact same offset to every cell within a block, deterministically",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::GranularNoise);
    config.setNoiseSeed(11u);
    config.setGrainSize(2);
    config.setGrainAmountDb(6.0f);
    const auto composite = makeUniformGridComposite(4, 4, -40.0f, -40.0f);

    const auto first = applyFilter(composite, config, ProjectSettings{});
    const auto second = applyFilter(composite, config, ProjectSettings{});

    REQUIRE(first.leftMagnitudeDb == second.leftMagnitudeDb);  // deterministic.
    // Top-left 2x2 block (bin 0-1, frame 0-1) - all four cells share one offset.
    const float offset = first.leftMagnitudeDb[0] - composite.leftMagnitudeDb[0];
    CHECK(first.leftMagnitudeDb[1] == Catch::Approx(composite.leftMagnitudeDb[1] + offset));
    CHECK(first.leftMagnitudeDb[4] == Catch::Approx(composite.leftMagnitudeDb[4] + offset));   // bin 1, frame 0.
    CHECK(first.leftMagnitudeDb[5] == Catch::Approx(composite.leftMagnitudeDb[5] + offset));   // bin 1, frame 1.
}

TEST_CASE("applyFilter's DynamicSpeckle sets every cell to full loudness at density 1 and intensity 1, "
          "regardless of its own live randomness",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::DynamicSpeckle);
    config.setSpeckleDensity(1.0f);
    config.setSpeckleIntensity(1.0f);
    const auto composite = makeUniformGridComposite(6, 6, -40.0f, -50.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    for (const float db : filtered.leftMagnitudeDb) {
        CHECK(db == Catch::Approx(0.0f));
    }
}

TEST_CASE("applyFilter's DynamicSpeckle is a no-op at zero density", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::DynamicSpeckle);
    config.setSpeckleDensity(0.0f);
    config.setSpeckleIntensity(1.0f);
    const auto composite = makeUniformGridComposite(10, 10, -40.0f, -30.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
}

TEST_CASE("applyFilter's DynamicSpeckle produces a genuinely different pattern from one call to the next",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::DynamicSpeckle);
    config.setSpeckleDensity(0.5f);
    config.setSpeckleIntensity(1.0f);
    const auto composite = makeUniformGridComposite(20, 20, -40.0f, -40.0f);

    const auto first = applyFilter(composite, config, ProjectSettings{});
    const auto second = applyFilter(composite, config, ProjectSettings{});

    // 100 independent 2x2 blocks at p=0.5 landing on the exact same pattern
    // twice in a row is astronomically unlikely - a real regression (e.g.
    // accidentally made hashCell()-deterministic) would fail this reliably.
    CHECK(first.leftMagnitudeDb != second.leftMagnitudeDb);
}

TEST_CASE("applyFilter's FeedbackDistortion is a no-op at zero amount", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::FeedbackDistortion);
    config.setFeedbackAmount(0.0f);
    std::vector<float> profile = {-96.0f, -50.0f, 0.0f, -20.0f, -96.0f};
    const auto composite = makeSingleRowComposite(profile);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    for (std::size_t i = 0; i < profile.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(profile[i]));
    }
}

TEST_CASE("applyFilter's FeedbackDistortion produces a decaying smear following an impulse",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::FeedbackDistortion);
    config.setFeedbackAmount(0.5f);
    std::vector<float> impulse = {0.0f, -96.0f, -96.0f, -96.0f};
    const auto composite = makeSingleRowComposite(impulse);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    // Hand-derived: y0=0 (seeded to its own original value); y1 = 0.5*
    // (-96) + 0.5*0 = -48; y2 = 0.5*(-96) + 0.5*(-48) = -72; y3 = 0.5*
    // (-96) + 0.5*(-72) = -84 - decaying back toward the floor, never
    // discontinuous.
    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(0.0f));
    CHECK(filtered.leftMagnitudeDb[1] == Catch::Approx(-48.0f));
    CHECK(filtered.leftMagnitudeDb[2] == Catch::Approx(-72.0f));
    CHECK(filtered.leftMagnitudeDb[3] == Catch::Approx(-84.0f));
}

TEST_CASE("applyFilter's FeedbackDistortion clamps its own amount internally, staying stable past 1.0",
          "[core][filter_application]") {
    FilterConfiguration configHigh;
    configHigh.setType(FilterType::FeedbackDistortion);
    configHigh.setFeedbackAmount(10.0f);  // clamps to 0.99 internally.
    FilterConfiguration configClamped = configHigh;
    configClamped.setFeedbackAmount(0.99f);
    std::vector<float> impulse = {0.0f, -96.0f, -96.0f, -96.0f};
    const auto composite = makeSingleRowComposite(impulse);

    const auto filteredHigh = applyFilter(composite, configHigh, ProjectSettings{});
    const auto filteredClamped = applyFilter(composite, configClamped, ProjectSettings{});

    for (std::size_t i = 0; i < impulse.size(); ++i) {
        CHECK(filteredHigh.leftMagnitudeDb[i] == Catch::Approx(filteredClamped.leftMagnitudeDb[i]));
        CHECK(std::isfinite(filteredHigh.leftMagnitudeDb[i]));
    }
}

TEST_CASE("applyFilter's SpectralWavefold is a no-op at foldGain 1.0", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpectralWavefold);
    config.setFoldGain(1.0f);
    const auto composite = makeUniformGridComposite(3, 3, -24.0f, -60.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-24.0f).margin(0.01));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(-60.0f).margin(0.01));
}

TEST_CASE("applyFilter's SpectralWavefold folds a value exceeding the threshold back into range",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpectralWavefold);
    config.setFoldGain(2.0f);
    const auto composite = makeUniformGridComposite(1, 1, -24.0f, -24.0f);  // unit = 0.75.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    // unit*gain = 1.5 -> triangle-folds to 0.5 -> back to -48dB.
    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-48.0f).margin(0.01));
}

TEST_CASE("applyFilter's SpectralWavefold produces a second fold at a higher gain",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpectralWavefold);
    config.setFoldGain(4.0f);
    const auto composite = makeUniformGridComposite(1, 1, -24.0f, -24.0f);  // unit = 0.75.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    // unit*gain = 3.0 -> triangle-folds to 1.0 -> back to 0dB.
    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(0.0f).margin(0.01));
}

TEST_CASE("applyFilter's SpectralWavefold leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::SpectralWavefold);
    config.setFoldGain(3.0f);

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

// ---------------------------------------------------------------------------
// ChannelBalance/Invert/Convolve - v0.Y.36.1 Installment B.
// ---------------------------------------------------------------------------

TEST_CASE("applyFilter's ChannelBalance reproduces an already-balanced signal at balance 0.5",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ChannelBalance);
    config.setChannelBalance(0.5f);
    const auto composite = makeUniformGridComposite(2, 2, -20.0f, -20.0f);  // left == right already.

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    for (std::size_t i = 0; i < filtered.leftMagnitudeDb.size(); ++i) {
        CHECK(filtered.leftMagnitudeDb[i] == Catch::Approx(-20.0f).margin(0.01));
        CHECK(filtered.rightMagnitudeDb[i] == Catch::Approx(-20.0f).margin(0.01));
    }
}

TEST_CASE("applyFilter's ChannelBalance sends all energy to the left channel at balance 0",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ChannelBalance);
    config.setChannelBalance(0.0f);
    const auto composite = makeUniformGridComposite(2, 2, -20.0f, -10.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    // total (linear) = 10^(-20/20) + 10^(-10/20) = 0.1 + 0.31623 = 0.41623;
    // left = total, right = 0 (floored to kMinLinearAmplitude's own -140dB).
    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(20.0f * std::log10(0.41623f)).margin(0.01));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(-140.0f).margin(0.5));
}

TEST_CASE("applyFilter's ChannelBalance sends all energy to the right channel at balance 1",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ChannelBalance);
    config.setChannelBalance(1.0f);
    const auto composite = makeUniformGridComposite(2, 2, -20.0f, -10.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-140.0f).margin(0.5));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(20.0f * std::log10(0.41623f)).margin(0.01));
}

TEST_CASE("applyFilter's ChannelBalance conserves total linear energy at an intermediate balance",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ChannelBalance);
    config.setChannelBalance(0.3f);
    const auto composite = makeUniformGridComposite(2, 2, -20.0f, -10.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    const auto toLinear = [](float db) { return std::pow(10.0f, db / 20.0f); };
    const float originalTotal = toLinear(-20.0f) + toLinear(-10.0f);
    const float filteredTotal = toLinear(filtered.leftMagnitudeDb[0]) + toLinear(filtered.rightMagnitudeDb[0]);
    CHECK(filteredTotal == Catch::Approx(originalTotal).margin(0.001));
}

TEST_CASE("applyFilter's ChannelBalance leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::ChannelBalance);
    config.setChannelBalance(0.2f);

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

TEST_CASE("applyFilter's Invert maps 0dB to the silence floor and the silence floor to 0dB",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Invert);
    const auto composite = makeUniformGridComposite(1, 1, 0.0f, -96.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-96.0f).margin(0.01));
    CHECK(filtered.rightMagnitudeDb[0] == Catch::Approx(0.0f).margin(0.01));
}

TEST_CASE("applyFilter's Invert is its own inverse at the midpoint", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Invert);
    const auto composite = makeUniformGridComposite(1, 1, -48.0f, -48.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb[0] == Catch::Approx(-48.0f).margin(0.01));
}

TEST_CASE("applyFilter's Invert leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Invert);

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}

TEST_CASE("applyFilter's Convolve is a no-op with the default identity kernel", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Convolve);  // default kernel/amount - identity, full wet.
    const auto composite = makeUniformGridComposite(4, 4, -37.25f, -12.8f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
    CHECK(filtered.rightMagnitudeDb == composite.rightMagnitudeDb);
}

TEST_CASE("applyFilter's Convolve is a no-op at amount 0 regardless of the kernel",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Convolve);
    config.setConvolveKernel({1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});  // a real box-blur kernel.
    config.setConvolveKernelSize(3);
    config.setConvolveAmount(0.0f);
    const auto composite = makeUniformGridComposite(4, 4, -37.25f, -12.8f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
}

TEST_CASE("applyFilter's Convolve applies a 3x3 box blur matching a hand-computed average",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Convolve);
    // A pre-normalized box blur (sums to 1 already) - normalize left off,
    // so the result should match a plain 3x3 average exactly.
    config.setConvolveKernel(std::vector<float>(9, 1.0f / 9.0f));
    config.setConvolveKernelSize(3);
    config.setConvolveNormalize(false);
    config.setConvolveAmount(1.0f);
    StreamImage composite;
    composite.config.binCount = 3;
    composite.frameCount = 3;
    composite.leftMagnitudeDb = {-10.0f, -20.0f, -10.0f, -20.0f, -90.0f, -20.0f, -10.0f, -20.0f, -10.0f};
    composite.rightMagnitudeDb.assign(9, -30.0f);
    composite.sharedPhaseRadians.assign(9, 0.0f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    // Center cell (row 1, col 1) - a full, unclamped 3x3 neighborhood.
    const float expectedCenter = (-10.0f - 20.0f - 10.0f - 20.0f - 90.0f - 20.0f - 10.0f - 20.0f - 10.0f) / 9.0f;
    CHECK(filtered.leftMagnitudeDb[4] == Catch::Approx(expectedCenter).margin(0.001));
    // A uniform field's own average is itself, regardless of the kernel.
    CHECK(filtered.rightMagnitudeDb[4] == Catch::Approx(-30.0f).margin(0.001));
}

TEST_CASE("applyFilter's Convolve normalizing a non-unit-sum kernel matches the same kernel pre-divided",
          "[core][filter_application]") {
    FilterConfiguration normalizedConfig;
    normalizedConfig.setType(FilterType::Convolve);
    normalizedConfig.setConvolveKernel(std::vector<float>(9, 1.0f));  // sums to 9, not 1.
    normalizedConfig.setConvolveKernelSize(3);
    normalizedConfig.setConvolveNormalize(true);
    normalizedConfig.setConvolveAmount(1.0f);

    FilterConfiguration preDividedConfig = normalizedConfig;
    preDividedConfig.setConvolveKernel(std::vector<float>(9, 1.0f / 9.0f));
    preDividedConfig.setConvolveNormalize(false);

    StreamImage composite;
    composite.config.binCount = 3;
    composite.frameCount = 3;
    composite.leftMagnitudeDb = {-10.0f, -20.0f, -10.0f, -20.0f, -90.0f, -20.0f, -10.0f, -20.0f, -10.0f};
    composite.rightMagnitudeDb.assign(9, -30.0f);
    composite.sharedPhaseRadians.assign(9, 0.0f);

    const auto normalized = applyFilter(composite, normalizedConfig, ProjectSettings{});
    const auto preDivided = applyFilter(composite, preDividedConfig, ProjectSettings{});

    for (std::size_t i = 0; i < normalized.leftMagnitudeDb.size(); ++i) {
        CHECK(normalized.leftMagnitudeDb[i] == Catch::Approx(preDivided.leftMagnitudeDb[i]).margin(0.001));
    }
}

TEST_CASE("applyFilter's Convolve is a no-op for a malformed kernel whose size doesn't match its own coefficient count",
          "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Convolve);
    config.setConvolveKernel({1.0f, 2.0f, 3.0f});  // 3 coefficients, but size claims 3x3=9.
    config.setConvolveKernelSize(3);
    const auto composite = makeUniformGridComposite(4, 4, -37.25f, -12.8f);

    const auto filtered = applyFilter(composite, config, ProjectSettings{});

    CHECK(filtered.leftMagnitudeDb == composite.leftMagnitudeDb);
}

TEST_CASE("applyFilter's Convolve leaves phase untouched", "[core][filter_application]") {
    FilterConfiguration config;
    config.setType(FilterType::Convolve);
    config.setConvolveKernel(std::vector<float>(9, 1.0f / 9.0f));
    config.setConvolveKernelSize(3);

    const auto filtered = applyFilter(makeComposite(), config, ProjectSettings{});

    for (const float phase : filtered.sharedPhaseRadians) {
        CHECK(phase == 0.5f);
    }
}
