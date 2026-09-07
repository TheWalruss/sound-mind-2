#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "sound_mind/core/playback_engine.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::core::PlaybackEngine;

namespace {

AudioBuffer makeTestAudio() {
    AudioBuffer audio;
    audio.sampleRateHz = 44100;
    audio.left = {0.1f, 0.2f, 0.3f, 0.4f};
    audio.right = {-0.1f, -0.2f, -0.3f, -0.4f};
    return audio;
}

}  // namespace

// No test here depends on a real audio device actually being present -
// PlaybackEngine's constructor is defensive about that (see
// isDeviceAvailable()'s docs), and renderBlock() - the one method actually
// exercised below - is called directly rather than through a real device
// callback, so these tests are safe to run on a CI runner with no audio
// hardware.

TEST_CASE("A fresh PlaybackEngine is not playing", "[core][playback_engine]") {
    const PlaybackEngine engine;
    CHECK_FALSE(engine.isPlaying());
}

TEST_CASE("play() and pause() toggle the playing state", "[core][playback_engine]") {
    PlaybackEngine engine;
    engine.loadAudio(makeTestAudio());

    engine.play();
    CHECK(engine.isPlaying());

    engine.pause();
    CHECK_FALSE(engine.isPlaying());
}

TEST_CASE("renderBlock outputs silence when not playing", "[core][playback_engine]") {
    PlaybackEngine engine;
    engine.loadAudio(makeTestAudio());

    std::vector<float> left(4, 999.0f);
    std::vector<float> right(4, 999.0f);
    float* channels[] = {left.data(), right.data()};

    engine.renderBlock(channels, 2, 4);

    for (const float sample : left) {
        CHECK(sample == 0.0f);
    }
    for (const float sample : right) {
        CHECK(sample == 0.0f);
    }
}

TEST_CASE("renderBlock fills output with the loaded audio's samples in order", "[core][playback_engine]") {
    PlaybackEngine engine;
    engine.loadAudio(makeTestAudio());
    engine.play();

    std::vector<float> left(2, 0.0f);
    std::vector<float> right(2, 0.0f);
    float* channels[] = {left.data(), right.data()};

    engine.renderBlock(channels, 2, 2);

    CHECK(left[0] == 0.1f);
    CHECK(left[1] == 0.2f);
    CHECK(right[0] == -0.1f);
    CHECK(right[1] == -0.2f);
    CHECK(engine.isPlaying());

    // A second block should continue from where the first left off, not
    // restart from the beginning.
    engine.renderBlock(channels, 2, 2);
    CHECK(left[0] == 0.3f);
    CHECK(left[1] == 0.4f);
}

TEST_CASE("renderBlock silences and stops once the loaded audio ends", "[core][playback_engine]") {
    PlaybackEngine engine;
    engine.loadAudio(makeTestAudio());
    engine.play();

    std::vector<float> left(6, -1.0f);
    std::vector<float> right(6, -1.0f);
    float* channels[] = {left.data(), right.data()};

    engine.renderBlock(channels, 2, 6);  // only 4 samples loaded

    CHECK(left[0] == 0.1f);
    CHECK(left[3] == 0.4f);
    CHECK(left[4] == 0.0f);
    CHECK(left[5] == 0.0f);
    CHECK_FALSE(engine.isPlaying());
}

TEST_CASE("stop() rewinds to the beginning", "[core][playback_engine]") {
    PlaybackEngine engine;
    engine.loadAudio(makeTestAudio());
    engine.play();

    std::vector<float> left(2, 0.0f);
    std::vector<float> right(2, 0.0f);
    float* channels[] = {left.data(), right.data()};
    engine.renderBlock(channels, 2, 2);  // advance partway through

    engine.stop();
    CHECK_FALSE(engine.isPlaying());

    engine.play();
    engine.renderBlock(channels, 2, 2);
    CHECK(left[0] == 0.1f);  // back at the start, not continuing from sample 2
}

TEST_CASE("renderBlock mixes to a single output channel using just the left channel", "[core][playback_engine]") {
    PlaybackEngine engine;
    engine.loadAudio(makeTestAudio());
    engine.play();

    std::vector<float> mono(2, 0.0f);
    float* channels[] = {mono.data()};

    engine.renderBlock(channels, 1, 2);

    CHECK(mono[0] == 0.1f);
    CHECK(mono[1] == 0.2f);
}
