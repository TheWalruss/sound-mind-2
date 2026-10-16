#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/stereo_analysis.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::core::measureStereo;

namespace {

AudioBuffer stereoSine(double rightGain, double seconds = 2.0) {
    AudioBuffer audio;
    audio.sampleRateHz = 48000;
    const auto count = static_cast<std::size_t>(seconds * 48000);
    for (std::size_t i = 0; i < count; ++i) {
        const auto value = static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * i / 48000.0));
        audio.left.push_back(value);
        audio.right.push_back(static_cast<float>(value * rightGain));
    }
    return audio;
}

}  // namespace

TEST_CASE("Identical channels are mono: correlation 1, width 0", "[core][stereo]") {
    const auto measurement = measureStereo(stereoSine(1.0));
    CHECK(measurement.correlation == Catch::Approx(1.0).margin(1e-6));
    CHECK(measurement.width == Catch::Approx(0.0).margin(1e-6));
    CHECK(measurement.correlationOverTime.size() == 20);
    CHECK(measurement.widthOverTime.back() == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("An inverted channel is out of phase: correlation -1, width 1", "[core][stereo]") {
    const auto measurement = measureStereo(stereoSine(-1.0));
    CHECK(measurement.correlation == Catch::Approx(-1.0).margin(1e-6));
    CHECK(measurement.width == Catch::Approx(1.0).margin(1e-6));
}

TEST_CASE("Unrelated channels read near zero correlation and middling width", "[core][stereo]") {
    AudioBuffer audio;
    audio.sampleRateHz = 48000;
    std::mt19937 rng(4);
    std::normal_distribution<float> noise(0.0f, 0.2f);
    for (int i = 0; i < 96000; ++i) {
        audio.left.push_back(noise(rng));
        audio.right.push_back(noise(rng));
    }
    const auto measurement = measureStereo(audio);
    CHECK(measurement.correlation == Catch::Approx(0.0).margin(0.02));
    CHECK(measurement.width == Catch::Approx(0.5).margin(0.02));
}

TEST_CASE("Band widths read stereo from level differences in the spectral data", "[core][stereo]") {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 40;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    auto mono = stereoSine(1.0, 1.0);
    mono.sampleRateHz = config.sampleRateHz;
    auto leftOnly = stereoSine(0.0, 1.0);
    leftOnly.sampleRateHz = config.sampleRateHz;

    const auto monoWidths = sound_mind::core::bandStereoWidths(sound_mind::codec::encode(mono, config));
    const auto leftWidths = sound_mind::core::bandStereoWidths(sound_mind::codec::encode(leftOnly, config));
    REQUIRE(monoWidths.size() == sound_mind::core::loudnessBands().size());
    // 440 Hz sits in the low-mid band (index 2).
    CHECK(monoWidths[2] == Catch::Approx(0.0).margin(0.01));
    CHECK(leftWidths[2] == Catch::Approx(0.5).margin(0.02));
}
