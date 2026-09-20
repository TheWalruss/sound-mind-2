#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "sound_mind/core/device_test_tone_player.h"

using sound_mind::core::AudioDeviceMode;
using sound_mind::core::DeviceTestTonePlayer;

TEST_CASE("DeviceTestTonePlayer starts idle, not playing", "[device_test_tone_player]") {
    const DeviceTestTonePlayer player(AudioDeviceMode::None);
    CHECK_FALSE(player.isPlaying());
    CHECK_FALSE(player.isDeviceAvailable());
}

TEST_CASE("start with AudioDeviceMode::None never opens a real device but reports playing", "[device_test_tone_player]") {
    DeviceTestTonePlayer player(AudioDeviceMode::None);
    player.start("");
    CHECK(player.isPlaying());
    CHECK_FALSE(player.isDeviceAvailable());
    player.stop();
    CHECK_FALSE(player.isPlaying());
}

TEST_CASE("processBlock outputs silence before start() / after stop()", "[device_test_tone_player]") {
    DeviceTestTonePlayer player(AudioDeviceMode::None);
    std::vector<float> left(256, 1.0f);
    std::vector<float> right(256, 1.0f);
    float* outputChannels[2] = {left.data(), right.data()};

    player.processBlock(outputChannels, 2, static_cast<int>(left.size()));

    CHECK(std::all_of(left.begin(), left.end(), [](float s) { return s == 0.0f; }));
    CHECK(std::all_of(right.begin(), right.end(), [](float s) { return s == 0.0f; }));
}

TEST_CASE("processBlock outputs a non-silent, identical-on-both-channels tone once started",
          "[device_test_tone_player]") {
    DeviceTestTonePlayer player(AudioDeviceMode::None);
    player.start("");

    std::vector<float> left(2048, 0.0f);
    std::vector<float> right(2048, 0.0f);
    float* outputChannels[2] = {left.data(), right.data()};
    player.processBlock(outputChannels, 2, static_cast<int>(left.size()));

    CHECK(left == right);
    CHECK(std::any_of(left.begin(), left.end(), [](float s) { return s != 0.0f; }));
    // Never louder than the documented, deliberately-moderate amplitude.
    CHECK(std::all_of(left.begin(), left.end(),
                       [](float s) { return std::abs(s) <= DeviceTestTonePlayer::kToneAmplitude + 1e-5f; }));
}

TEST_CASE("stop() silences subsequent processBlock() calls", "[device_test_tone_player]") {
    DeviceTestTonePlayer player(AudioDeviceMode::None);
    player.start("");
    std::vector<float> scratch(256, 0.0f);
    float* outputChannels[1] = {scratch.data()};
    player.processBlock(outputChannels, 1, static_cast<int>(scratch.size()));
    REQUIRE(std::any_of(scratch.begin(), scratch.end(), [](float s) { return s != 0.0f; }));

    player.stop();
    std::fill(scratch.begin(), scratch.end(), 1.0f);
    player.processBlock(outputChannels, 1, static_cast<int>(scratch.size()));

    CHECK(std::all_of(scratch.begin(), scratch.end(), [](float s) { return s == 0.0f; }));
}

TEST_CASE("start() resets phase so consecutive test tones sound identical", "[device_test_tone_player]") {
    DeviceTestTonePlayer player(AudioDeviceMode::None);
    std::vector<float> first(512, 0.0f);
    std::vector<float> second(512, 0.0f);
    float* firstChannels[1] = {first.data()};
    float* secondChannels[1] = {second.data()};

    player.start("");
    player.processBlock(firstChannels, 1, static_cast<int>(first.size()));
    player.stop();

    player.start("");
    player.processBlock(secondChannels, 1, static_cast<int>(second.size()));
    player.stop();

    CHECK(first == second);
}
