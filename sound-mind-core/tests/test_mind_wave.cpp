#include <cmath>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveAxis;
using sound_mind::core::PeriodicWaveform;
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
