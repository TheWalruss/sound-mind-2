#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "sound_mind/codec_eval/fidelity_metrics.h"

using sound_mind::codec_eval::BandSnrDb;
using sound_mind::codec_eval::bandSignalToNoiseRatioDb;
using sound_mind::codec_eval::correlation;
using sound_mind::codec_eval::kSnrCapDb;
using sound_mind::codec_eval::peakAbsolute;
using sound_mind::codec_eval::rootMeanSquare;
using sound_mind::codec_eval::signalToNoiseRatioDb;

namespace {

std::vector<float> sineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz, float amplitude = 1.0f) {
    const auto n = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    std::vector<float> signal(n);
    for (std::size_t i = 0; i < n; ++i) {
        signal[i] =
            amplitude * std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
    }
    return signal;
}

}  // namespace

TEST_CASE("correlation() is 1.0 for identical signals", "[fidelity_metrics]") {
    const std::vector<float> signal = sineTone(440.0f, 1.0f, 44100);
    CHECK(correlation(signal, signal) > 0.9999f);
}

TEST_CASE("correlation() is near zero for a sine against an unrelated, much higher frequency sine", "[fidelity_metrics]") {
    const std::vector<float> a = sineTone(100.0f, 1.0f, 44100);
    const std::vector<float> b = sineTone(11025.0f, 1.0f, 44100);
    CHECK(std::abs(correlation(a, b)) < 0.1f);
}

TEST_CASE("correlation() is 0.0 when either signal is silent", "[fidelity_metrics]") {
    const std::vector<float> silence(1000, 0.0f);
    const std::vector<float> tone = sineTone(440.0f, 1.0f, 44100);
    CHECK(correlation(silence, tone) == 0.0f);
    CHECK(correlation(silence, silence) == 0.0f);
}

TEST_CASE("correlation() truncates to the shorter signal's length", "[fidelity_metrics]") {
    const std::vector<float> full = sineTone(440.0f, 1.0f, 44100);
    const std::vector<float> truncated(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(full.size() / 2));
    CHECK(correlation(full, truncated) > 0.9999f);
}

TEST_CASE("rootMeanSquare() of a unit sine is 1/sqrt(2)", "[fidelity_metrics]") {
    const std::vector<float> signal = sineTone(1000.0f, 1.0f, 44100);
    CHECK(rootMeanSquare(signal) == Catch::Approx(1.0f / std::sqrt(2.0f)).margin(0.001));
}

TEST_CASE("rootMeanSquare() of an empty signal is 0", "[fidelity_metrics]") {
    CHECK(rootMeanSquare({}) == 0.0f);
}

TEST_CASE("peakAbsolute() finds the largest magnitude sample, positive or negative", "[fidelity_metrics]") {
    const std::vector<float> signal{0.1f, -0.9f, 0.5f, 0.3f};
    CHECK(peakAbsolute(signal) == Catch::Approx(0.9f));
}

TEST_CASE("peakAbsolute() of an empty signal is 0", "[fidelity_metrics]") {
    CHECK(peakAbsolute({}) == 0.0f);
}

TEST_CASE("signalToNoiseRatioDb() caps at kSnrCapDb for a sample-exact round trip", "[fidelity_metrics]") {
    const std::vector<float> signal = sineTone(440.0f, 1.0f, 44100);
    CHECK(signalToNoiseRatioDb(signal, signal) == Catch::Approx(kSnrCapDb));
}

TEST_CASE("signalToNoiseRatioDb() is finite and positive for a lightly-perturbed signal", "[fidelity_metrics]") {
    std::vector<float> original = sineTone(440.0f, 1.0f, 44100);
    std::vector<float> decoded = original;
    for (std::size_t i = 0; i < decoded.size(); ++i) {
        decoded[i] += (i % 2 == 0 ? 0.001f : -0.001f);
    }
    const float snr = signalToNoiseRatioDb(original, decoded);
    CHECK(snr > 0.0f);
    CHECK(snr < kSnrCapDb);
}

TEST_CASE("signalToNoiseRatioDb() is lower for a more heavily perturbed signal than a lightly perturbed one",
          "[fidelity_metrics]") {
    const std::vector<float> original = sineTone(440.0f, 1.0f, 44100);
    std::vector<float> lightlyPerturbed = original;
    std::vector<float> heavilyPerturbed = original;
    for (std::size_t i = 0; i < original.size(); ++i) {
        lightlyPerturbed[i] += 0.001f;
        heavilyPerturbed[i] += 0.1f;
    }
    CHECK(signalToNoiseRatioDb(original, lightlyPerturbed) > signalToNoiseRatioDb(original, heavilyPerturbed));
}

TEST_CASE("signalToNoiseRatioDb() is 0 when the original signal is silent", "[fidelity_metrics]") {
    const std::vector<float> silence(1000, 0.0f);
    CHECK(signalToNoiseRatioDb(silence, silence) == 0.0f);
}

TEST_CASE("bandSignalToNoiseRatioDb() caps every band at kSnrCapDb for a sample-exact round trip", "[fidelity_metrics]") {
    const std::vector<float> signal = sineTone(1000.0f, 1.0f, 44100);
    const BandSnrDb band = bandSignalToNoiseRatioDb(signal, signal, 44100);
    CHECK(band.lowDb == Catch::Approx(kSnrCapDb));
    CHECK(band.midDb == Catch::Approx(kSnrCapDb));
    CHECK(band.highDb == Catch::Approx(kSnrCapDb));
}

TEST_CASE("bandSignalToNoiseRatioDb() concentrates degradation in the band the error actually occupies",
          "[fidelity_metrics]") {
    // A 100 Hz tone (low band) whose decoded version has noise added only in
    // a high-frequency (>4000 Hz) component - the low band should read much
    // cleaner than the high band.
    const std::vector<float> original = sineTone(100.0f, 1.0f, 44100, 1.0f);
    const std::vector<float> highFrequencyNoise = sineTone(8000.0f, 1.0f, 44100, 0.5f);
    std::vector<float> decoded = original;
    for (std::size_t i = 0; i < decoded.size(); ++i) {
        decoded[i] += highFrequencyNoise[i];
    }
    const BandSnrDb band = bandSignalToNoiseRatioDb(original, decoded, 44100);
    CHECK(band.lowDb > band.highDb);
    CHECK(band.lowDb > 40.0f);
}

TEST_CASE("bandSignalToNoiseRatioDb() is all zero when the original signal is silent", "[fidelity_metrics]") {
    const std::vector<float> silence(1000, 0.0f);
    const BandSnrDb band = bandSignalToNoiseRatioDb(silence, silence, 44100);
    CHECK(band.lowDb == 0.0f);
    CHECK(band.midDb == 0.0f);
    CHECK(band.highDb == 0.0f);
}
