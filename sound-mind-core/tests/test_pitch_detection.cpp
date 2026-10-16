#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::detectPitch;
using sound_mind::core::noteFor;

namespace {

/// @brief A harmonic tone: `f0` and harmonics 2-5 at falling levels.
sound_mind::codec::StreamImage harmonicTone(double f0, double fundamentalGain = 1.0) {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 60;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const auto count = static_cast<std::size_t>(settings.canvasWidth) * config.hopLength;
    for (std::size_t i = 0; i < count; ++i) {
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 5; ++harmonic) {
            const double gain = (harmonic == 1 ? fundamentalGain : 1.0) * 0.3 / harmonic;
            value += gain * std::sin(2.0 * std::numbers::pi * f0 * harmonic * static_cast<double>(i) / config.sampleRateHz);
        }
        audio.left.push_back(static_cast<float>(value));
    }
    audio.right = audio.left;
    return sound_mind::codec::encode(audio, config);
}

}  // namespace

TEST_CASE("detectPitch finds a harmonic tone's fundamental", "[core][pitch]") {
    for (const double f0 : {110.0, 220.0, 440.0}) {
        const auto pitch = detectPitch(harmonicTone(f0));
        INFO("f0 " << f0);
        REQUIRE(pitch.size() == 60);
        CHECK(pitch[30] == Catch::Approx(f0).epsilon(0.02));
    }
}

TEST_CASE("detectPitch doesn't report a sub-octave for a strong fundamental", "[core][pitch]") {
    const auto pitch = detectPitch(harmonicTone(300.0));
    CHECK(pitch[30] == Catch::Approx(300.0).epsilon(0.02));
}

TEST_CASE("detectPitch leaves silence unvoiced", "[core][pitch]") {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 10;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    sound_mind::codec::AudioBuffer silence;
    silence.sampleRateHz = config.sampleRateHz;
    silence.left.assign(static_cast<std::size_t>(10) * config.hopLength, 0.0f);
    silence.right = silence.left;
    for (const float hz : detectPitch(sound_mind::codec::encode(silence, config))) {
        CHECK(hz == 0.0f);
    }
}

TEST_CASE("noteFor names notes and their tuning in cents", "[core][pitch]") {
    const auto a4 = noteFor(440.0);
    CHECK(a4.name == "A");
    CHECK(a4.octave == 4);
    CHECK(a4.cents == Catch::Approx(0.0).margin(1e-9));
    const auto middleC = noteFor(261.6256);
    CHECK(middleC.name == "C");
    CHECK(middleC.octave == 4);
    const auto sharp = noteFor(440.0 * std::pow(2.0, 0.2 / 12.0));
    CHECK(sharp.name == "A");
    CHECK(sharp.cents == Catch::Approx(20.0).margin(0.01));
    CHECK(noteFor(55.0).octave == 1);
}
