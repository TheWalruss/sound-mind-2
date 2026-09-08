#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

#include "sound_mind/core/live_engine.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::AudioDeviceMode;
using sound_mind::core::LiveEngine;

namespace {

/// @brief A mono sine tone, as a plain sample vector - LiveEngine::
/// processBlock() takes raw channel pointers, matching a real audio
/// callback's own shape.
std::vector<float> makeSineTone(float frequencyHz, std::size_t sampleCount, std::uint32_t sampleRateHz) {
    std::vector<float> samples(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        samples[i] =
            std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
    }
    return samples;
}

}  // namespace

TEST_CASE("LiveEngine starts with no image and is not running", "[live_engine]") {
    LiveEngine engine(StreamCodecConfig{}, AudioDeviceMode::None);
    CHECK_FALSE(engine.isRunning());
    CHECK_FALSE(engine.isDeviceAvailable());
    CHECK(engine.currentImage().frameCount == 0);
}

TEST_CASE("LiveEngine::start with AudioDeviceMode::None never opens a real device", "[live_engine]") {
    LiveEngine engine(StreamCodecConfig{}, AudioDeviceMode::None);
    engine.start();
    CHECK(engine.isRunning());
    CHECK_FALSE(engine.isDeviceAvailable());
    engine.stop();
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("processPendingAudio does nothing until a full analysis window has been captured", "[live_engine]") {
    const StreamCodecConfig config;
    const std::uint32_t fftSize = config.hopLength * 4;

    LiveEngine engine(config, AudioDeviceMode::None);

    const std::vector<float> input = makeSineTone(440.0f, fftSize - 1, config.sampleRateHz);
    const float* inputChannels[1] = {input.data()};
    // Output-pulling side of processBlock() isn't under test here - pass a
    // throwaway buffer.
    std::vector<float> scratchOut(input.size());
    float* outputChannels[1] = {scratchOut.data()};

    engine.processBlock(inputChannels, 1, outputChannels, 1, static_cast<int>(input.size()));
    engine.processPendingAudio();

    CHECK(engine.currentImage().frameCount == 0);
}

TEST_CASE("A full round trip: captured audio comes back out through the playback ring", "[live_engine]") {
    const StreamCodecConfig config;
    const std::uint32_t fftSize = config.hopLength * 4;  // one full analysis window.

    LiveEngine engine(config, AudioDeviceMode::None);

    const std::vector<float> input = makeSineTone(440.0f, fftSize, config.sampleRateHz);
    const float* inputChannels[1] = {input.data()};
    std::vector<float> ignoredOut(input.size());
    float* ignoredOutputChannels[1] = {ignoredOut.data()};

    // Feed exactly one window's worth of audio in, mono (channel 0 only) -
    // processBlock() should mirror it to both left and right internally.
    engine.processBlock(inputChannels, 1, ignoredOutputChannels, 1, static_cast<int>(input.size()));
    engine.processPendingAudio();

    REQUIRE(engine.currentImage().frameCount == 1);

    // One frame's worth of hopLength samples should now be stable and
    // published - pull exactly that many out.
    std::vector<float> outputLeft(config.hopLength, -1.0f);
    std::vector<float> outputRight(config.hopLength, -1.0f);
    float* outputChannels[2] = {outputLeft.data(), outputRight.data()};
    engine.processBlock(nullptr, 0, outputChannels, 2, static_cast<int>(config.hopLength));

    bool anyNonZero = false;
    for (float sample : outputLeft) {
        if (sample != 0.0f) {
            anyNonZero = true;
            break;
        }
    }
    CHECK(anyNonZero);
    // Left and right should match (mono input, mirrored to both channels).
    CHECK(outputLeft == outputRight);
}

TEST_CASE("Pushing audio in small chunks (simulating real callback timing) still produces frames",
          "[live_engine]") {
    const StreamCodecConfig config;
    const std::uint32_t fftSize = config.hopLength * 4;

    LiveEngine engine(config, AudioDeviceMode::None);
    const std::vector<float> input = makeSineTone(440.0f, fftSize * 3, config.sampleRateHz);

    constexpr std::size_t kBlockSize = 256;
    std::vector<float> scratchOut(kBlockSize, 0.0f);
    float* outputChannels[1] = {scratchOut.data()};

    for (std::size_t offset = 0; offset < input.size(); offset += kBlockSize) {
        const std::size_t n = std::min(kBlockSize, input.size() - offset);
        const float* inputChannels[1] = {input.data() + offset};
        engine.processBlock(inputChannels, 1, outputChannels, 1, static_cast<int>(n));
        engine.processPendingAudio();
    }

    CHECK(engine.currentImage().frameCount > 0);
}
