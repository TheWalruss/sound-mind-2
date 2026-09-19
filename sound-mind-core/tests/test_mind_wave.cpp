#include <algorithm>
#include <cmath>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/path.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::EnvelopeShape;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::ReduceMode;
using sound_mind::core::reduceMindWaveToSignal;
using sound_mind::core::SpatialPattern;
using sound_mind::core::SteppedNoiseShape;
using sound_mind::core::SuperpositionBlendMode;
using sound_mind::core::TimeFrequencyPoint;

namespace {

/// @brief A straight-edged (`PathNodeType::Corner`, no handles) Path
/// through `points`, in order - geometrically a straight-line polyline
/// (see `evaluateCubicBezier()`'s own formula: collapsed handles still
/// trace a perfectly straight line between two Corner anchors, just via a
/// smoothstep-eased, not linear-in-`t`, parametrization - the relationship
/// *between* the two coordinates along one edge stays exactly linear
/// either way, which is all these tests' own hand-derived expectations
/// depend on).
Path cornerPath(std::vector<TimeFrequencyPoint> points) {
    Path path;
    for (const auto& point : points) {
        PathNode node;
        node.anchor = point;
        node.type = PathNodeType::Corner;
        path.addNode(node);
    }
    return path;
}

StreamCodecConfig testConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;
    config.binCount = 100;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

}  // namespace

TEST_CASE("A fresh MindWave is a real, audible-if-bound sine, not a degenerate placeholder",
          "[core][mind_wave]") {
    const MindWave wave;
    REQUIRE(wave.type() == GeneratorType::Periodic);
    REQUIRE(wave.periodicWaveform() == PeriodicWaveform::Sine);
    REQUIRE(wave.axis() == MindWaveAxis::Time);
    REQUIRE(wave.period() == 1.0);
    REQUIRE(wave.phaseRadians() == 0.0);
}

TEST_CASE("Setters change what the getters report", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Periodic);
    wave.setPeriodicWaveform(PeriodicWaveform::Sine);
    wave.setAxis(MindWaveAxis::Frequency);
    wave.setPeriod(2.5);
    wave.setPhaseRadians(1.25);

    REQUIRE(wave.axis() == MindWaveAxis::Frequency);
    REQUIRE(wave.period() == 2.5);
    REQUIRE(wave.phaseRadians() == 1.25);
}

TEST_CASE("A time-axis sine evaluates to 0.5 at t=0 with zero phase", "[core][mind_wave]") {
    const MindWave wave;  // period=1.0, phase=0.0, Time axis.
    const float value = wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, testConfig());
    REQUIRE(value == Catch::Approx(0.5f));
}

TEST_CASE("A time-axis sine peaks a quarter-period in and troughs three-quarters in",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setPeriod(4.0);  // quarter-period lands on a round number.

    const float atQuarter = wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, testConfig());
    const float atThreeQuarters = wave.evaluate(TimeFrequencyPoint{3.0, 1000.0}, testConfig());

    REQUIRE(atQuarter == Catch::Approx(1.0f));
    REQUIRE(atThreeQuarters == Catch::Approx(0.0f));
}

TEST_CASE("A time-axis sine repeats exactly every period", "[core][mind_wave]") {
    MindWave wave;
    wave.setPeriod(2.0);

    const float first = wave.evaluate(TimeFrequencyPoint{0.3, 1000.0}, testConfig());
    const float oneCycleLater = wave.evaluate(TimeFrequencyPoint{2.3, 1000.0}, testConfig());

    REQUIRE(first == Catch::Approx(oneCycleLater));
}

TEST_CASE("phaseRadians shifts the wave along its own axis", "[core][mind_wave]") {
    MindWave shifted;
    shifted.setPeriod(4.0);
    shifted.setPhaseRadians(std::numbers::pi_v<double> / 2.0);  // a quarter-period shift.

    // Shifted by a quarter period at t=0 should read the same as an
    // unshifted wave already a quarter-period in.
    MindWave unshifted;
    unshifted.setPeriod(4.0);

    const float shiftedAtZero = shifted.evaluate(TimeFrequencyPoint{0.0, 1000.0}, testConfig());
    const float unshiftedAtQuarter = unshifted.evaluate(TimeFrequencyPoint{1.0, 1000.0}, testConfig());
    REQUIRE(shiftedAtZero == Catch::Approx(unshiftedAtQuarter));
}

TEST_CASE("A frequency-axis wave's own period is in bins, not Hz", "[core][mind_wave]") {
    MindWave wave;
    wave.setAxis(MindWaveAxis::Frequency);
    wave.setPeriod(10.0);  // 10 bins per cycle.
    const auto config = testConfig();

    // Two frequencies exactly one full bin-space cycle apart should read
    // the same value, regardless of how many raw Hz separate them (the
    // frequency axis is log-scaled - see evaluate()'s own docs).
    constexpr float kLowFrequencyHz = 100.0f;
    const float lowBin = frequencyToBinIndex(kLowFrequencyHz, config);
    const float highFrequencyHz = [&] {
        // Binary-search-free: walk forward in bin space directly via the
        // inverse mapping already exposed for exactly this.
        return sound_mind::core::binIndexToFrequency(lowBin + 10.0f, config);
    }();

    const float atLow = wave.evaluate(TimeFrequencyPoint{0.0, kLowFrequencyHz}, config);
    const float atHigh = wave.evaluate(TimeFrequencyPoint{0.0, highFrequencyHz}, config);
    REQUIRE(atLow == Catch::Approx(atHigh).margin(0.001));
}

TEST_CASE("evaluate() never leaves [0, 1], including with an unusual period", "[core][mind_wave]") {
    const auto config = testConfig();
    for (double period : {0.0, -1.0, 0.001, 1000.0}) {
        MindWave wave;
        wave.setPeriod(period);
        for (double t : {0.0, 0.37, 5.5, 123.456}) {
            const float value = wave.evaluate(TimeFrequencyPoint{t, 1000.0}, config);
            REQUIRE(std::isfinite(value));
            REQUIRE(value >= 0.0f);
            REQUIRE(value <= 1.0f);
        }
    }
}

TEST_CASE("A MindWave round-trips through JSON unchanged", "[core][mind_wave]") {
    MindWave original;
    original.setType(GeneratorType::Periodic);
    original.setPeriodicWaveform(PeriodicWaveform::Sine);
    original.setAxis(MindWaveAxis::Frequency);
    original.setPeriod(3.25);
    original.setPhaseRadians(0.75);

    const nlohmann::json json = original;
    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.type() == original.type());
    REQUIRE(restored.periodicWaveform() == original.periodicWaveform());
    REQUIRE(restored.axis() == original.axis());
    REQUIRE(restored.period() == original.period());
    REQUIRE(restored.phaseRadians() == original.phaseRadians());
}

// --- Installment B: the rest of the v1 generator catalogue, plus
// superposition -----------------------------------------------------

TEST_CASE("Periodic Triangle/Square/Sawtooth/Pulse take their expected shape",
          "[core][mind_wave]") {
    const auto config = testConfig();

    MindWave triangle;
    triangle.setPeriodicWaveform(PeriodicWaveform::Triangle);
    triangle.setPeriod(4.0);
    // Aligned to Sine's own phase convention: 0.5 at t=0, peak at the
    // quarter-period, 0.5 at the half-period, trough at three-quarters.
    REQUIRE(triangle.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config) == Catch::Approx(0.5f));
    REQUIRE(triangle.evaluate(TimeFrequencyPoint{1.0, 1000.0}, config) == Catch::Approx(1.0f));
    REQUIRE(triangle.evaluate(TimeFrequencyPoint{2.0, 1000.0}, config) == Catch::Approx(0.5f));
    REQUIRE(triangle.evaluate(TimeFrequencyPoint{3.0, 1000.0}, config) == Catch::Approx(0.0f));

    MindWave square;
    square.setPeriodicWaveform(PeriodicWaveform::Square);
    square.setPeriod(4.0);
    REQUIRE(square.evaluate(TimeFrequencyPoint{0.5, 1000.0}, config) == Catch::Approx(1.0f));
    REQUIRE(square.evaluate(TimeFrequencyPoint{2.5, 1000.0}, config) == Catch::Approx(0.0f));

    MindWave sawtooth;
    sawtooth.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    sawtooth.setPeriod(4.0);
    REQUIRE(sawtooth.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config) == Catch::Approx(0.0f));
    REQUIRE(sawtooth.evaluate(TimeFrequencyPoint{1.0, 1000.0}, config) == Catch::Approx(0.25f));
    REQUIRE(sawtooth.evaluate(TimeFrequencyPoint{3.0, 1000.0}, config) == Catch::Approx(0.75f));

    MindWave pulse;
    pulse.setPeriodicWaveform(PeriodicWaveform::Pulse);
    pulse.setPeriod(4.0);
    pulse.setDutyCycle(0.25);
    REQUIRE(pulse.evaluate(TimeFrequencyPoint{0.5, 1000.0}, config) == Catch::Approx(1.0f));
    REQUIRE(pulse.evaluate(TimeFrequencyPoint{2.0, 1000.0}, config) == Catch::Approx(0.0f));
}

TEST_CASE("Pulse at the default 0.5 duty cycle matches Square", "[core][mind_wave]") {
    const auto config = testConfig();
    MindWave pulse;
    pulse.setPeriodicWaveform(PeriodicWaveform::Pulse);
    MindWave square;
    square.setPeriodicWaveform(PeriodicWaveform::Square);
    for (double t : {0.0, 0.2, 0.4, 0.6, 0.8}) {
        REQUIRE(pulse.evaluate(TimeFrequencyPoint{t, 1000.0}, config) ==
                square.evaluate(TimeFrequencyPoint{t, 1000.0}, config));
    }
}

TEST_CASE("Envelope ExponentialDecay plateaus before its center and decays after it",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Envelope);
    wave.setEnvelopeShape(EnvelopeShape::ExponentialDecay);
    wave.setEnvelopeCenter(1.0);
    wave.setDecayRate(2.0);
    const auto config = testConfig();

    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config) == Catch::Approx(1.0f));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, config) == Catch::Approx(1.0f));

    const float earlier = wave.evaluate(TimeFrequencyPoint{1.5, 1000.0}, config);
    const float later = wave.evaluate(TimeFrequencyPoint{3.0, 1000.0}, config);
    REQUIRE(earlier > later);
    REQUIRE(later >= 0.0f);
}

TEST_CASE("Envelope DecayingOscillation decays toward its 0.5 baseline", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Envelope);
    wave.setEnvelopeShape(EnvelopeShape::DecayingOscillation);
    wave.setEnvelopeCenter(0.0);
    wave.setDecayRate(5.0);
    wave.setPeriod(1.0);
    const auto config = testConfig();

    const float early = wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config);
    const float muchLater = wave.evaluate(TimeFrequencyPoint{10.0, 1000.0}, config);
    REQUIRE(early == Catch::Approx(1.0f));
    REQUIRE(muchLater == Catch::Approx(0.5f).margin(0.01));
}

TEST_CASE("Envelope SCurve transitions from 0 to 1 through its own center", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Envelope);
    wave.setEnvelopeShape(EnvelopeShape::SCurve);
    wave.setEnvelopeCenter(2.0);
    wave.setEnvelopeSteepness(4.0);
    const auto config = testConfig();

    REQUIRE(wave.evaluate(TimeFrequencyPoint{-10.0, 1000.0}, config) == Catch::Approx(0.0f).margin(0.001));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{2.0, 1000.0}, config) == Catch::Approx(0.5f));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{20.0, 1000.0}, config) == Catch::Approx(1.0f).margin(0.001));
}

TEST_CASE("SteppedNoise Stepped only ever takes stepCount discrete levels", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::SteppedNoise);
    wave.setSteppedNoiseShape(SteppedNoiseShape::Stepped);
    wave.setPeriod(1.0);
    wave.setStepCount(4);
    const auto config = testConfig();

    for (double t : {0.0, 0.1, 0.24, 0.3, 0.5, 0.74, 0.9}) {
        const float value = wave.evaluate(TimeFrequencyPoint{t, 1000.0}, config);
        const float scaled = value * 3.0f;  // stepCount - 1
        REQUIRE(scaled == Catch::Approx(std::round(scaled)).margin(0.001));
    }
}

TEST_CASE("SteppedNoise GaussianNoise/FractalNoise are deterministic and stay in [0, 1]",
          "[core][mind_wave]") {
    const auto config = testConfig();
    for (auto shape : {SteppedNoiseShape::GaussianNoise, SteppedNoiseShape::FractalNoise}) {
        MindWave wave;
        wave.setType(GeneratorType::SteppedNoise);
        wave.setSteppedNoiseShape(shape);
        wave.setSeed(42);

        const float first = wave.evaluate(TimeFrequencyPoint{1.23, 1000.0}, config);
        const float again = wave.evaluate(TimeFrequencyPoint{1.23, 1000.0}, config);
        REQUIRE(first == again);
        REQUIRE(first >= 0.0f);
        REQUIRE(first <= 1.0f);

        MindWave differentSeed = wave;
        differentSeed.setSeed(99);
        REQUIRE(differentSeed.evaluate(TimeFrequencyPoint{1.23, 1000.0}, config) != first);
    }
}

TEST_CASE("Spatial Ripples is a distance field around its own center", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Spatial);
    wave.setSpatialPattern(SpatialPattern::Ripples);
    wave.setSpatialCenterX(0.0);
    wave.setSpatialCenterY(static_cast<double>(frequencyToBinIndex(1000.0f, testConfig())));
    wave.setPeriod(4.0);
    const auto config = testConfig();

    const float atCenter = wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config);
    REQUIRE(atCenter == Catch::Approx(0.5f));
}

TEST_CASE("Spatial Checkerboard alternates by integer cell parity", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Spatial);
    wave.setSpatialPattern(SpatialPattern::Checkerboard);
    wave.setPeriod(1.0);
    const auto config = testConfig();
    const float binZeroHz = binIndexToFrequency(0.0f, config);

    const float cell00 = wave.evaluate(TimeFrequencyPoint{0.5, binZeroHz}, config);
    const float cell10Hz = binIndexToFrequency(1.5f, config);
    const float cell10 = wave.evaluate(TimeFrequencyPoint{0.5, cell10Hz}, config);
    REQUIRE(cell00 != cell10);
    REQUIRE((cell00 == 0.0f || cell00 == 1.0f));
    REQUIRE((cell10 == 0.0f || cell10 == 1.0f));
}

TEST_CASE("Spatial Cellular/DomainWarpedNoise are deterministic and stay in [0, 1]",
          "[core][mind_wave]") {
    const auto config = testConfig();
    for (auto pattern : {SpatialPattern::Cellular, SpatialPattern::DomainWarpedNoise}) {
        MindWave wave;
        wave.setType(GeneratorType::Spatial);
        wave.setSpatialPattern(pattern);
        wave.setSeed(7);
        wave.setNoiseScale(2.0);

        const float first = wave.evaluate(TimeFrequencyPoint{3.5, 2000.0}, config);
        const float again = wave.evaluate(TimeFrequencyPoint{3.5, 2000.0}, config);
        REQUIRE(first == again);
        REQUIRE(first >= 0.0f);
        REQUIRE(first <= 1.0f);
    }
}

TEST_CASE("Fractal is deterministic, seed-dependent, and repeats every period",
          "[core][mind_wave]") {
    const auto config = testConfig();
    MindWave wave;
    wave.setType(GeneratorType::Fractal);
    wave.setPeriod(2.0);
    wave.setSeed(5);
    wave.setFractalIterations(6);

    const float first = wave.evaluate(TimeFrequencyPoint{0.37, 1000.0}, config);
    const float again = wave.evaluate(TimeFrequencyPoint{0.37, 1000.0}, config);
    REQUIRE(first == again);
    REQUIRE(first >= 0.0f);
    REQUIRE(first <= 1.0f);

    const float oneCycleLater = wave.evaluate(TimeFrequencyPoint{2.37, 1000.0}, config);
    REQUIRE(first == Catch::Approx(oneCycleLater));

    MindWave differentSeed = wave;
    differentSeed.setSeed(6);
    REQUIRE(differentSeed.evaluate(TimeFrequencyPoint{0.37, 1000.0}, config) != first);
}

TEST_CASE("An empty superposition stack leaves evaluate() unchanged", "[core][mind_wave]") {
    const MindWave wave;  // Default sine, empty stack.
    REQUIRE(wave.superpositionStack().empty());
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.25, 1000.0}, testConfig()) ==
            Catch::Approx(1.0f));  // Unchanged from the plain-sine test's own expectation.
}

TEST_CASE("Superposition folds each stack member per the chosen blend mode", "[core][mind_wave]") {
    const auto config = testConfig();
    const TimeFrequencyPoint point{0.0, 1000.0};

    MindWave base;  // sine, t=0 -> 0.5
    MindWave member;
    member.setPeriodicWaveform(PeriodicWaveform::Square);  // t=0 -> 1.0
    member.setPeriod(4.0);

    const float baseValue = base.evaluate(point, config);
    const float memberValue = member.evaluate(point, config);
    REQUIRE(baseValue == Catch::Approx(0.5f));
    REQUIRE(memberValue == Catch::Approx(1.0f));

    auto withMode = [&](SuperpositionBlendMode mode) {
        MindWave combined = base;
        combined.setSuperpositionStack({member});
        combined.setSuperpositionBlendMode(mode);
        return combined.evaluate(point, config);
    };

    REQUIRE(withMode(SuperpositionBlendMode::Multiply) == Catch::Approx(baseValue * memberValue));
    REQUIRE(withMode(SuperpositionBlendMode::Add) == Catch::Approx(std::min(1.0f, baseValue + memberValue)));
    REQUIRE(withMode(SuperpositionBlendMode::Min) == Catch::Approx(std::min(baseValue, memberValue)));
    REQUIRE(withMode(SuperpositionBlendMode::Max) == Catch::Approx(std::max(baseValue, memberValue)));
    REQUIRE(withMode(SuperpositionBlendMode::Average) == Catch::Approx((baseValue + memberValue) / 2.0f));
}

TEST_CASE("Superposition supports nested stacks (a member with its own stack)", "[core][mind_wave]") {
    MindWave innermost;
    innermost.setPeriodicWaveform(PeriodicWaveform::Square);
    innermost.setPeriod(4.0);

    MindWave middle;
    middle.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    middle.setPeriod(4.0);
    middle.setSuperpositionStack({innermost});
    middle.setSuperpositionBlendMode(SuperpositionBlendMode::Add);

    MindWave outer;
    outer.setSuperpositionStack({middle});
    outer.setSuperpositionBlendMode(SuperpositionBlendMode::Multiply);

    const auto config = testConfig();
    const float value = outer.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config);
    REQUIRE(std::isfinite(value));
    REQUIRE(value >= 0.0f);
    REQUIRE(value <= 1.0f);
}

TEST_CASE("A MindWave with every new generator field and a superposition stack round-trips through JSON",
          "[core][mind_wave]") {
    MindWave member;
    member.setType(GeneratorType::Fractal);
    member.setSeed(123);
    member.setFractalRoughness(0.4);
    member.setFractalIterations(5);

    MindWave original;
    original.setType(GeneratorType::Spatial);
    original.setSpatialPattern(SpatialPattern::Cellular);
    original.setSpatialCenterX(1.5);
    original.setSpatialCenterY(2.5);
    original.setNoiseScale(3.0);
    original.setNoiseOctaves(6);
    original.setNoisePersistence(0.6);
    original.setSeed(77);
    original.setDomainWarpStrength(2.0);
    original.setDutyCycle(0.3);
    original.setEnvelopeShape(EnvelopeShape::SCurve);
    original.setEnvelopeCenter(1.0);
    original.setEnvelopeSteepness(2.0);
    original.setDecayRate(1.5);
    original.setSteppedNoiseShape(SteppedNoiseShape::FractalNoise);
    original.setStepCount(8);
    original.setFractalRoughness(0.6);
    original.setFractalIterations(9);
    original.setSuperpositionStack({member});
    original.setSuperpositionBlendMode(SuperpositionBlendMode::Max);

    const nlohmann::json json = original;
    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.type() == original.type());
    REQUIRE(restored.spatialPattern() == original.spatialPattern());
    REQUIRE(restored.spatialCenterX() == original.spatialCenterX());
    REQUIRE(restored.spatialCenterY() == original.spatialCenterY());
    REQUIRE(restored.noiseScale() == original.noiseScale());
    REQUIRE(restored.noiseOctaves() == original.noiseOctaves());
    REQUIRE(restored.noisePersistence() == original.noisePersistence());
    REQUIRE(restored.seed() == original.seed());
    REQUIRE(restored.domainWarpStrength() == original.domainWarpStrength());
    REQUIRE(restored.dutyCycle() == original.dutyCycle());
    REQUIRE(restored.envelopeShape() == original.envelopeShape());
    REQUIRE(restored.envelopeCenter() == original.envelopeCenter());
    REQUIRE(restored.envelopeSteepness() == original.envelopeSteepness());
    REQUIRE(restored.decayRate() == original.decayRate());
    REQUIRE(restored.steppedNoiseShape() == original.steppedNoiseShape());
    REQUIRE(restored.stepCount() == original.stepCount());
    REQUIRE(restored.fractalRoughness() == original.fractalRoughness());
    REQUIRE(restored.fractalIterations() == original.fractalIterations());
    REQUIRE(restored.superpositionBlendMode() == original.superpositionBlendMode());
    REQUIRE(restored.superpositionStack().size() == 1);
    REQUIRE(restored.superpositionStack()[0].type() == GeneratorType::Fractal);
    REQUIRE(restored.superpositionStack()[0].seed() == 123);
    REQUIRE(restored.superpositionStack()[0].fractalRoughness() == Catch::Approx(0.4));
    REQUIRE(restored.superpositionStack()[0].fractalIterations() == 5);
}

// --- v0.Y.39.1 Installment A: Warp/Reduce field operators -------------

TEST_CASE("A MindWave with no warp source evaluates unchanged", "[core][mind_wave]") {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    wave.setPeriod(4.0);
    REQUIRE_FALSE(wave.hasWarpSource());
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, testConfig()) == Catch::Approx(0.25f));
}

TEST_CASE("Warp displaces the sampling coordinate by its own source's evaluated value, scaled by warpStrength",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    wave.setPeriod(4.0);

    MindWave source;
    source.setPeriodicWaveform(PeriodicWaveform::Square);
    source.setPeriod(1'000'000.0);  // Always at its own ceiling (1.0) for any small t.
    wave.setWarpSource(source);
    wave.setWarpStrength(1.0);

    // displacement = 1.0 * (1*2-1) = 1.0; effective time = 1.0+1.0 = 2.0; p = 2.0/4.0 = 0.5.
    REQUIRE(wave.hasWarpSource());
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, testConfig()) == Catch::Approx(0.5f));
}

TEST_CASE("Warp's own strength scales the displacement linearly", "[core][mind_wave]") {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    wave.setPeriod(4.0);

    MindWave source;
    source.setPeriodicWaveform(PeriodicWaveform::Square);
    source.setPeriod(1'000'000.0);
    wave.setWarpSource(source);
    wave.setWarpStrength(0.5);

    // displacement = 0.5 * (1*2-1) = 0.5; effective time = 1.5; p = 1.5/4 = 0.375.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, testConfig()) == Catch::Approx(0.375f));
}

TEST_CASE("clearWarpSource() removes a previously-set warp source", "[core][mind_wave]") {
    MindWave wave;
    wave.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    wave.setPeriod(4.0);
    MindWave source;
    source.setPeriodicWaveform(PeriodicWaveform::Square);
    wave.setWarpSource(source);
    REQUIRE(wave.hasWarpSource());

    wave.clearWarpSource();

    REQUIRE_FALSE(wave.hasWarpSource());
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, testConfig()) == Catch::Approx(0.25f));
}

TEST_CASE("Warp also displaces the frequency axis, in bins", "[core][mind_wave]") {
    MindWave wave;
    wave.setAxis(MindWaveAxis::Frequency);
    wave.setPeriodicWaveform(PeriodicWaveform::Sawtooth);
    wave.setPeriod(4.0);  // 4 bins per cycle.

    MindWave source;
    source.setPeriodicWaveform(PeriodicWaveform::Square);
    source.setPeriod(1'000'000.0);
    wave.setWarpSource(source);
    wave.setWarpStrength(1.0);  // +1 bin displacement.

    const auto config = testConfig();
    const float bin1Hz = binIndexToFrequency(1.0f, config);
    // Unwarped: bin=1, p=1/4=0.25. Warped: bin=1+1=2, p=2/4=0.5.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.0, bin1Hz}, config) == Catch::Approx(0.5f).margin(0.01));
}

TEST_CASE("A MindWave with a warp source round-trips through JSON", "[core][mind_wave]") {
    MindWave source;
    source.setPeriodicWaveform(PeriodicWaveform::Square);
    source.setPeriod(2.0);

    MindWave original;
    original.setWarpSource(source);
    original.setWarpStrength(0.75);

    const nlohmann::json json = original;
    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.hasWarpSource());
    REQUIRE(restored.warpSource().periodicWaveform() == PeriodicWaveform::Square);
    REQUIRE(restored.warpSource().period() == Catch::Approx(2.0));
    REQUIRE(restored.warpStrength() == Catch::Approx(0.75));
}

TEST_CASE("A MindWave loads from JSON missing warpSourceStack/warpStrength (saved before v0.Y.39.1) unwarped",
          "[core][mind_wave]") {
    MindWave config;
    nlohmann::json json = config;
    json.erase("warpSourceStack");
    json.erase("warpStrength");

    const MindWave restored = json.get<MindWave>();

    REQUIRE_FALSE(restored.hasWarpSource());
    REQUIRE(restored.warpStrength() == Catch::Approx(1.0));
}

TEST_CASE("reduceMindWaveToSignal's Integrate mode averages evaluate() across every bin at each time sample",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Spatial);
    wave.setSpatialPattern(SpatialPattern::Checkerboard);
    wave.setPeriod(3.0);
    const auto config = testConfig();

    const auto signal = reduceMindWaveToSignal(wave, config, 4, 2.0, ReduceMode::Integrate);

    REQUIRE(signal.size() == 4);
    for (std::size_t i = 0; i < signal.size(); ++i) {
        const double timeSeconds = (static_cast<double>(i) / 4.0) * 2.0;
        float sum = 0.0f;
        for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
            sum += wave.evaluate(TimeFrequencyPoint{timeSeconds, binIndexToFrequency(static_cast<float>(bin), config)},
                                  config);
        }
        const float expected = sum / static_cast<float>(config.binCount);
        REQUIRE(signal[i] == Catch::Approx(expected).margin(0.0001));
    }
}

TEST_CASE("reduceMindWaveToSignal's Slice mode reads a single representative bin at each time sample",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Spatial);
    wave.setSpatialPattern(SpatialPattern::Checkerboard);
    wave.setPeriod(3.0);
    const auto config = testConfig();

    const auto signal = reduceMindWaveToSignal(wave, config, 4, 2.0, ReduceMode::Slice);

    const float midHz = binIndexToFrequency(static_cast<float>(config.binCount) / 2.0f, config);
    REQUIRE(signal.size() == 4);
    for (std::size_t i = 0; i < signal.size(); ++i) {
        const double timeSeconds = (static_cast<double>(i) / 4.0) * 2.0;
        const float expected = wave.evaluate(TimeFrequencyPoint{timeSeconds, midHz}, config);
        REQUIRE(signal[i] == Catch::Approx(expected).margin(0.0001));
    }
}

TEST_CASE("reduceMindWaveToSignal produces exactly sampleCount values, floored at 1", "[core][mind_wave]") {
    const MindWave wave;
    const auto config = testConfig();
    REQUIRE(reduceMindWaveToSignal(wave, config, 7, 1.0, ReduceMode::Integrate).size() == 7);
    REQUIRE(reduceMindWaveToSignal(wave, config, 0, 1.0, ReduceMode::Slice).size() == 1);
}

TEST_CASE("reduceMindWaveToSignal's own values all fall within [0, 1]", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::SteppedNoise);
    wave.setSteppedNoiseShape(SteppedNoiseShape::FractalNoise);
    const auto config = testConfig();

    for (const float value : reduceMindWaveToSignal(wave, config, 20, 5.0, ReduceMode::Integrate)) {
        REQUIRE(value >= 0.0f);
        REQUIRE(value <= 1.0f);
    }
}

// --- v0.Y.39.1 Installment B: Drawn-shape generator --------------------

TEST_CASE("A Drawn MindWave with no captured path (fewer than two nodes) evaluates to neutral 0.5",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    REQUIRE(wave.drawnPath().nodes().empty());
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.5, 1000.0}, testConfig()) == Catch::Approx(0.5f));

    wave.setDrawnPath(cornerPath({TimeFrequencyPoint{0.0, 100.0}}));  // A single node still can't sample anything.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.5, 1000.0}, testConfig()) == Catch::Approx(0.5f));
}

TEST_CASE("A Drawn MindWave samples its own straight two-node path via first-crossing, normalized against its own "
          "bounding box",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    wave.setAxis(MindWaveAxis::Time);
    // Drawn from (t=0, 100 Hz) to (t=2, 300 Hz) - a 2-second recorded span.
    wave.setDrawnPath(cornerPath({TimeFrequencyPoint{0.0, 100.0}, TimeFrequencyPoint{2.0, 300.0}}));
    wave.setPeriod(2.0);  // Loop length matches the recorded span exactly.
    const auto config = testConfig();

    // t=0 -> the path's own start -> its own lowest frequency -> normalized 0.0.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config) == Catch::Approx(0.0f).margin(0.001));
    // t=1 -> halfway along the drawn span -> halfway between 100 Hz and 300 Hz -> normalized 0.5.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, config) == Catch::Approx(0.5f).margin(0.001));
    // t=2 -> exactly one full loop -> wraps back to the start -> normalized 0.0 again.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{2.0, 1000.0}, config) == Catch::Approx(0.0f).margin(0.001));
}

TEST_CASE("A Drawn MindWave's own period() independently stretches/compresses its loop length, decoupled from the "
          "path's own recorded duration",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    wave.setAxis(MindWaveAxis::Time);
    wave.setDrawnPath(cornerPath({TimeFrequencyPoint{0.0, 100.0}, TimeFrequencyPoint{2.0, 300.0}}));
    wave.setPeriod(4.0);  // Twice the recorded 2-second span.
    const auto config = testConfig();

    // t=2 is now only halfway through the (now 4-second) loop, landing at the
    // same halfway point along the drawn shape that t=1 reached with period=2.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{2.0, 1000.0}, config) == Catch::Approx(0.5f).margin(0.001));
}

TEST_CASE("A Drawn MindWave's first-crossing rule picks the earliest crossing along a non-single-valued curve",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    wave.setAxis(MindWaveAxis::Time);
    // Time doubles back on itself (0 -> 2 -> 1): querying domain (time) 0.5
    // crosses twice - once early (segment 0, freq rising 0->100) and once
    // late (segment 1, freq rising 100->300). The recorded span (first node
    // to last node) is t=0 to t=1, matched here by period=1.
    wave.setDrawnPath(cornerPath({TimeFrequencyPoint{0.0, 0.0}, TimeFrequencyPoint{2.0, 100.0},
                                   TimeFrequencyPoint{1.0, 300.0}}));
    wave.setPeriod(1.0);
    const auto config = testConfig();

    // First crossing (segment 0, at local fraction 0.25 of its own 0->2
    // domain span): freq = 0 + 0.25*(100-0) = 25. Bounding box freq range
    // is [0, 300] (node0's 0, node2's 300) -> normalized 25/300.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.5, 1000.0}, config) == Catch::Approx(25.0f / 300.0f).margin(0.005));
}

TEST_CASE("A Drawn MindWave on the Frequency axis samples via bin-domain, normalized against the path's own time "
          "range",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    wave.setAxis(MindWaveAxis::Frequency);
    const auto config = testConfig();
    const float lowHz = binIndexToFrequency(0.0f, config);
    const float highHz = binIndexToFrequency(10.0f, config);
    // Drawn from (10s, lowest bin) to (20s, bin 10).
    wave.setDrawnPath(cornerPath({TimeFrequencyPoint{10.0, lowHz}, TimeFrequencyPoint{20.0, highHz}}));
    wave.setPeriod(10.0);  // Matches the recorded 10-bin domain span exactly.

    // The exact recorded start (bin 0) is unambiguous: p=0 maps directly to
    // the first node, normalized to 0.0. An exact query at the *other*
    // recorded end (bin 10) isn't - it lands exactly on period()'s own
    // wraparound boundary and loops back to the start instead (the same
    // "t == one full period wraps to 0" behavior the Time-axis looping test
    // above already exercises) - so this checks monotonic increase toward
    // the interior instead of a second exact endpoint. The frequency axis's
    // own log-to-bin conversion also means an interior "halfway in bins"
    // query does *not* land halfway along the curve's own parametrization
    // the way the Time-axis test above can (bins are a nonlinear function
    // of the Hz values the path itself linearly interpolates), so this
    // avoids hand-deriving any interior numeric value at all.
    const float atStart = wave.evaluate(TimeFrequencyPoint{0.0, static_cast<double>(lowHz)}, config);
    const float nearStart = wave.evaluate(TimeFrequencyPoint{0.0, static_cast<double>(binIndexToFrequency(1.0f, config))}, config);
    const float nearEnd = wave.evaluate(TimeFrequencyPoint{0.0, static_cast<double>(binIndexToFrequency(9.0f, config))}, config);
    REQUIRE(atStart == Catch::Approx(0.0f).margin(0.01));
    REQUIRE(nearStart < nearEnd);
    REQUIRE(nearEnd < 1.0f);
}

TEST_CASE("A Drawn MindWave with a zero-width recorded domain span evaluates to neutral 0.5", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::Drawn);
    wave.setAxis(MindWaveAxis::Time);
    // Both nodes at the same time - no time-axis span to loop across at all.
    wave.setDrawnPath(cornerPath({TimeFrequencyPoint{1.0, 100.0}, TimeFrequencyPoint{1.0, 300.0}}));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.5, 1000.0}, testConfig()) == Catch::Approx(0.5f));
}

TEST_CASE("A Drawn MindWave round-trips its own path through JSON", "[core][mind_wave]") {
    MindWave original;
    original.setType(GeneratorType::Drawn);
    original.setDrawnPath(cornerPath({TimeFrequencyPoint{0.0, 100.0}, TimeFrequencyPoint{2.0, 300.0}}));

    const nlohmann::json json = original;
    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.type() == GeneratorType::Drawn);
    REQUIRE(restored.drawnPath().nodes().size() == std::size_t{2});
    REQUIRE(restored.drawnPath().nodes()[1].anchor.frequencyHz == Catch::Approx(300.0));
}

TEST_CASE("A MindWave loads from JSON missing drawnPath (saved before v0.Y.39.1 Installment B) with an empty path",
          "[core][mind_wave]") {
    MindWave config;
    nlohmann::json json = config;
    json.erase("drawnPath");

    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.drawnPath().nodes().empty());
}

// --- v0.Y.39.1 Installment C: Step-grid generator ----------------------

TEST_CASE("A fresh StepGrid MindWave has the default four-step rising staircase", "[core][mind_wave]") {
    const MindWave wave;
    REQUIRE(wave.stepGridValues() == std::vector<double>{0.25, 0.5, 0.75, 1.0});
}

TEST_CASE("A StepGrid MindWave returns each step's own value verbatim across one period", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::StepGrid);
    wave.setAxis(MindWaveAxis::Time);
    wave.setPeriod(4.0);
    wave.setStepGridValues({0.1, 0.4, 0.6, 0.9});
    const auto config = testConfig();

    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, config) == Catch::Approx(0.1f));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{1.0, 1000.0}, config) == Catch::Approx(0.4f));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{2.0, 1000.0}, config) == Catch::Approx(0.6f));
    REQUIRE(wave.evaluate(TimeFrequencyPoint{3.0, 1000.0}, config) == Catch::Approx(0.9f));
    // One full period wraps back to the first step.
    REQUIRE(wave.evaluate(TimeFrequencyPoint{4.0, 1000.0}, config) == Catch::Approx(0.1f));
}

TEST_CASE("A StepGrid MindWave's own phase offset shifts which step is currently active", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::StepGrid);
    wave.setAxis(MindWaveAxis::Time);
    wave.setPeriod(4.0);
    wave.setStepGridValues({0.1, 0.4, 0.6, 0.9});
    wave.setPhaseRadians(std::numbers::pi_v<double>);  // Half a cycle - shifts two steps ahead of four.

    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.0, 1000.0}, testConfig()) == Catch::Approx(0.6f));
}

TEST_CASE("An empty StepGrid evaluates to neutral 0.5 rather than indexing an empty vector", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::StepGrid);
    wave.setStepGridValues({});
    REQUIRE(wave.evaluate(TimeFrequencyPoint{0.5, 1000.0}, testConfig()) == Catch::Approx(0.5f));
}

TEST_CASE("A StepGrid MindWave round-trips its own values through JSON", "[core][mind_wave]") {
    MindWave original;
    original.setType(GeneratorType::StepGrid);
    original.setStepGridValues({0.2, 0.4, 0.8});

    const nlohmann::json json = original;
    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.type() == GeneratorType::StepGrid);
    REQUIRE(restored.stepGridValues() == std::vector<double>{0.2, 0.4, 0.8});
}

TEST_CASE("A MindWave loads from JSON missing stepGridValues (saved before v0.Y.39.1 Installment C) with the "
          "standard default staircase",
          "[core][mind_wave]") {
    MindWave config;
    nlohmann::json json = config;
    json.erase("stepGridValues");

    const MindWave restored = json.get<MindWave>();

    REQUIRE(restored.stepGridValues() == std::vector<double>{0.25, 0.5, 0.75, 1.0});
}

// --- Installment C1: NamedMindWave -----------------------------------

TEST_CASE("A NamedMindWave round-trips through JSON unchanged", "[core][mind_wave]") {
    sound_mind::core::NamedMindWave original;
    original.id = 42;
    original.name = "Slow Pulse";
    original.wave.setPeriod(3.0);
    original.wave.setPeriodicWaveform(PeriodicWaveform::Triangle);

    const nlohmann::json json = original;
    const auto restored = json.get<sound_mind::core::NamedMindWave>();

    REQUIRE(restored.id == original.id);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.wave.period() == original.wave.period());
    REQUIRE(restored.wave.periodicWaveform() == original.wave.periodicWaveform());
}

// --- evaluateMindWaveField() - the Studio's own MindWave Preview overlay ---

TEST_CASE("evaluateMindWaveField returns one value per bin/frame cell, matching evaluate() at each position",
          "[core][mind_wave]") {
    MindWave wave;
    wave.setAxis(MindWaveAxis::Time);
    wave.setPeriod(2.0);
    const auto config = testConfig();
    constexpr std::uint32_t canvasWidth = 4;

    const auto field = sound_mind::core::evaluateMindWaveField(wave, config, canvasWidth);

    REQUIRE(field.size() == std::size_t{config.binCount} * canvasWidth);
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        const float frequencyHz = binIndexToFrequency(static_cast<float>(bin), config);
        for (std::uint32_t frame = 0; frame < canvasWidth; ++frame) {
            const float expected =
                wave.evaluate(TimeFrequencyPoint{sound_mind::core::frameIndexToTime(frame, config), frequencyHz},
                              config);
            REQUIRE(field[sound_mind::core::cellIndex(bin, frame, canvasWidth)] == Catch::Approx(expected));
        }
    }
}

TEST_CASE("evaluateMindWaveField's own values all fall within [0, 1]", "[core][mind_wave]") {
    MindWave wave;
    wave.setType(GeneratorType::SteppedNoise);
    wave.setSteppedNoiseShape(SteppedNoiseShape::GaussianNoise);
    const auto config = testConfig();

    const auto field = sound_mind::core::evaluateMindWaveField(wave, config, 10);

    for (const float value : field) {
        REQUIRE(value >= 0.0f);
        REQUIRE(value <= 1.0f);
    }
}
