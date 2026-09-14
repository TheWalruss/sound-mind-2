#include <algorithm>
#include <cmath>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::EnvelopeShape;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::PeriodicWaveform;
using sound_mind::core::SpatialPattern;
using sound_mind::core::SteppedNoiseShape;
using sound_mind::core::SuperpositionBlendMode;
using sound_mind::core::TimeFrequencyPoint;

namespace {

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
