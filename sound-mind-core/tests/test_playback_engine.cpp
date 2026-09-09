#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "sound_mind/core/playback_engine.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::core::AudioDeviceMode;
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

// Every engine below is constructed with AudioDeviceMode::None - these
// tests call renderBlock() directly, and a real device (when one exists)
// would also be calling it concurrently from its own background callback
// thread, racing against these direct calls on the same shared atomics.
// See AudioDeviceMode's docs for why that's a real, not just theoretical,
// source of flakiness (found via exactly this: a rare failure in "stop()
// rewinds to the beginning" when run alongside other tests, never in
// isolation - a classic race-condition signature).

TEST_CASE("A fresh PlaybackEngine is not playing", "[core][playback_engine]") {
    const PlaybackEngine engine(AudioDeviceMode::None);
    CHECK_FALSE(engine.isPlaying());
}

TEST_CASE("play() and pause() toggle the playing state", "[core][playback_engine]") {
    PlaybackEngine engine(AudioDeviceMode::None);
    engine.loadAudio(makeTestAudio());

    engine.play();
    CHECK(engine.isPlaying());

    engine.pause();
    CHECK_FALSE(engine.isPlaying());
}

TEST_CASE("renderBlock outputs silence when not playing", "[core][playback_engine]") {
    PlaybackEngine engine(AudioDeviceMode::None);
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
    PlaybackEngine engine(AudioDeviceMode::None);
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
    PlaybackEngine engine(AudioDeviceMode::None);
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
    PlaybackEngine engine(AudioDeviceMode::None);
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
    PlaybackEngine engine(AudioDeviceMode::None);
    engine.loadAudio(makeTestAudio());
    engine.play();

    std::vector<float> mono(2, 0.0f);
    float* channels[] = {mono.data()};

    engine.renderBlock(channels, 1, 2);

    CHECK(mono[0] == 0.1f);
    CHECK(mono[1] == 0.2f);
}

TEST_CASE("AudioDeviceMode::None never reports a device as available", "[core][playback_engine]") {
    const PlaybackEngine engine(AudioDeviceMode::None);
    CHECK_FALSE(engine.isDeviceAvailable());
}

TEST_CASE("A fresh PlaybackEngine has unity volume", "[core][playback_engine]") {
    const PlaybackEngine engine(AudioDeviceMode::None);
    CHECK(engine.volume() == 1.0f);
}

TEST_CASE("setVolume clamps to [0, kMaxVolume]", "[core][playback_engine]") {
    PlaybackEngine engine(AudioDeviceMode::None);

    engine.setVolume(1.5f);
    CHECK(engine.volume() == 1.5f);

    engine.setVolume(-1.0f);
    CHECK(engine.volume() == 0.0f);

    engine.setVolume(PlaybackEngine::kMaxVolume + 10.0f);
    CHECK(engine.volume() == PlaybackEngine::kMaxVolume);
}

TEST_CASE("renderBlock applies volume as a real gain multiplier, including above unity",
          "[core][playback_engine]") {
    PlaybackEngine engine(AudioDeviceMode::None);
    engine.loadAudio(makeTestAudio());
    engine.play();
    engine.setVolume(2.0f);

    std::vector<float> left(2, 0.0f);
    std::vector<float> right(2, 0.0f);
    float* channels[] = {left.data(), right.data()};
    engine.renderBlock(channels, 2, 2);

    CHECK(left[0] == 0.2f);   // 0.1 * 2.0
    CHECK(left[1] == 0.4f);   // 0.2 * 2.0
    CHECK(right[0] == -0.2f);  // -0.1 * 2.0
}

TEST_CASE("AudioDeviceMode::None reports no current output device", "[core][playback_engine]") {
    const PlaybackEngine engine(AudioDeviceMode::None);
    CHECK(engine.currentOutputDeviceName().empty());
}

TEST_CASE("setPreferredOutputDevice fails gracefully for an unknown device name", "[core][playback_engine]") {
    PlaybackEngine engine(AudioDeviceMode::None);
    // Populate the device manager's device types first - JUCE's own
    // setAudioDeviceSetup() has nothing to validate a device name against
    // (and so trivially "succeeds") until some device type has actually
    // been queried at least once, the same way availableOutputDeviceNames()
    // (or a real start()) would in practice.
    (void)engine.availableOutputDeviceNames();
    CHECK_FALSE(engine.setPreferredOutputDevice("definitely not a real device name"));
}

TEST_CASE("availableOutputDeviceNames is callable without crashing", "[core][playback_engine]") {
    PlaybackEngine engine(AudioDeviceMode::None);
    const auto names = engine.availableOutputDeviceNames();
    // No real device is guaranteed in a CI/test environment - just confirm
    // the call is well-formed.
    CHECK(names == engine.availableOutputDeviceNames());
}
