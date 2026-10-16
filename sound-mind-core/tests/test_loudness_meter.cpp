#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <tuple>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/parallel.h"
#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/core/stereo_analysis.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::core::kLoudnessFloorLufs;
using sound_mind::core::measureLoudness;

namespace {

AudioBuffer sine(double frequencyHz, double amplitude, double seconds, std::uint32_t rate = 48000,
                 double phase = 0.0) {
    AudioBuffer audio;
    audio.sampleRateHz = rate;
    const auto count = static_cast<std::size_t>(seconds * rate);
    audio.left.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        audio.left[i] = static_cast<float>(
            amplitude * std::sin(2.0 * std::numbers::pi * frequencyHz * static_cast<double>(i) / rate + phase));
    }
    audio.right = audio.left;
    return audio;
}

}  // namespace

TEST_CASE("A 1 kHz sine at -6 dBFS in both channels reads about -6 LUFS", "[core][loudness_meter]") {
    const auto measurement = measureLoudness(sine(1000.0, 0.5, 5.0));
    CHECK(measurement.integratedLufs == Catch::Approx(-6.03).margin(0.15));
    CHECK(measurement.maxShortTermLufs == Catch::Approx(-6.03).margin(0.15));
    CHECK(measurement.samplePeakDbfs == Catch::Approx(-6.02).margin(0.05));
    CHECK(measurement.clippedSamples == 0);
    // One reading per 100 ms.
    CHECK(measurement.momentaryLufs.size() == 50);
    CHECK(measurement.momentaryLufs.back() == Catch::Approx(-6.03).margin(0.2));
}

TEST_CASE("K-weighting is correct at other sample rates too", "[core][loudness_meter]") {
    const auto measurement = measureLoudness(sine(1000.0, 0.5, 5.0, 44100));
    CHECK(measurement.integratedLufs == Catch::Approx(-6.03).margin(0.15));
    // The bass is K-weighted down: a 40 Hz tone of the same level reads quieter.
    CHECK(measureLoudness(sine(40.0, 0.5, 5.0, 44100)).integratedLufs < measurement.integratedLufs - 1.0);
}

TEST_CASE("Gating keeps silence from dragging integrated loudness down", "[core][loudness_meter]") {
    AudioBuffer audio = sine(1000.0, 0.5, 4.0);
    audio.left.resize(audio.left.size() * 2, 0.0f);
    audio.right.resize(audio.right.size() * 2, 0.0f);
    CHECK(measureLoudness(audio).integratedLufs == Catch::Approx(-6.03).margin(0.2));
}

TEST_CASE("Silence reads the floor", "[core][loudness_meter]") {
    AudioBuffer silence;
    silence.sampleRateHz = 48000;
    silence.left.assign(48000, 0.0f);
    silence.right = silence.left;
    const auto measurement = measureLoudness(silence);
    CHECK(measurement.integratedLufs == kLoudnessFloorLufs);
    CHECK(measurement.truePeakDbtp == kLoudnessFloorLufs);
    CHECK(measureLoudness(AudioBuffer{}).momentaryLufs.empty());
}

TEST_CASE("Samples beyond full scale are counted as clipping", "[core][loudness_meter]") {
    AudioBuffer audio = sine(100.0, 0.5, 1.0);
    audio.left[100] = 1.2f;
    audio.right[200] = -1.5f;
    audio.right[300] = 1.0f;  // exactly full scale isn't a clip.
    const auto measurement = measureLoudness(audio);
    CHECK(measurement.clippedSamples == 2);
    CHECK(measurement.samplePeakDbfs == Catch::Approx(20.0 * std::log10(1.5)).margin(0.01));
}

TEST_CASE("True peak finds a peak that falls between samples", "[core][loudness_meter]") {
    // A quarter-sample-rate sine phased so every sample lands at 0.707 of the
    // waveform's peak: the sample peak under-reads by 3 dB.
    const auto audio = sine(12000.0, 0.8, 1.0, 48000, std::numbers::pi / 4.0);
    const auto measurement = measureLoudness(audio);
    CHECK(measurement.samplePeakDbfs == Catch::Approx(20.0 * std::log10(0.8 * std::sqrt(0.5))).margin(0.05));
    CHECK(measurement.truePeakDbtp == Catch::Approx(20.0 * std::log10(0.8)).margin(0.3));
}

TEST_CASE("Band levels place a tone in its own band", "[core][loudness_meter]") {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 40;
    settings.binCount = 128;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto content = sound_mind::codec::encode(sine(1000.0, 0.5, 1.0, config.sampleRateHz), config);
    const auto levels = sound_mind::core::bandLevelsOverTime(content);
    const auto& bands = sound_mind::core::loudnessBands();
    REQUIRE(levels.size() == bands.size() * content.frameCount);

    const std::uint32_t frame = content.frameCount / 2;
    std::size_t loudest = 0;
    for (std::size_t band = 1; band < bands.size(); ++band) {
        if (levels[band * content.frameCount + frame] > levels[loudest * content.frameCount + frame]) {
            loudest = band;
        }
    }
    CHECK(bands[loudest].lowHz <= 1000.0);
    CHECK(bands[loudest].highHz > 1000.0);
}

TEST_CASE("integratedLoudnessLufs matches the full measurement", "[core][loudness_meter]") {
    AudioBuffer audio = sine(1000.0, 0.25, 3.0);
    audio.left.resize(audio.left.size() + 48000, 0.0f);
    audio.right.resize(audio.right.size() + 48000, 0.0f);
    CHECK(sound_mind::core::integratedLoudnessLufs(audio) == Catch::Approx(measureLoudness(audio).integratedLufs));
    CHECK(sound_mind::core::integratedLoudnessLufs(AudioBuffer{}) == kLoudnessFloorLufs);
}

// v0.1.41.4 - the analysis stages run in parallel; results must not depend on it.
TEST_CASE("Analysis stages give identical results on one thread and on many", "[core][loudness_meter]") {
    AudioBuffer audio = sine(440.0, 0.4, 4.0);
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        audio.right[i] = 0.5f * audio.left[i] + 0.2f * std::sin(static_cast<float>(i) * 0.37f);
    }
    audio.left[1000] = 1.3f;  // a clip and a true peak to find.
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 300;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto content = sound_mind::codec::encode(sine(440.0, 0.4, 3.0, config.sampleRateHz), config);

    const auto run = [&]() {
        const auto loudness = measureLoudness(audio);
        return std::make_tuple(loudness.integratedLufs, loudness.truePeakDbtp, loudness.momentaryLufs,
                               sound_mind::core::bandLevelsOverTime(content),
                               sound_mind::core::bandStereoWidths(content),
                               sound_mind::core::detectPitches(content, 2));
    };
    sound_mind::codec::setMaxWorkerThreads(1);
    const auto serial = run();
    sound_mind::codec::setMaxWorkerThreads(8);
    const auto parallel = run();
    sound_mind::codec::setMaxWorkerThreads(0);
    CHECK(std::get<0>(parallel) == std::get<0>(serial));
    CHECK(std::get<1>(parallel) == std::get<1>(serial));
    CHECK(std::get<2>(parallel) == std::get<2>(serial));
    CHECK(std::get<3>(parallel) == std::get<3>(serial));
    CHECK(std::get<4>(parallel) == std::get<4>(serial));
    CHECK(std::get<5>(parallel) == std::get<5>(serial));
}

