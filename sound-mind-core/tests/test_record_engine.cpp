#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "sound_mind/core/record_engine.h"

using sound_mind::core::AudioDeviceMode;
using sound_mind::core::RecordEngine;

namespace {

std::vector<float> makeSineTone(float frequencyHz, std::size_t sampleCount, std::uint32_t sampleRateHz) {
    std::vector<float> samples(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        samples[i] =
            std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
    }
    return samples;
}

}  // namespace

TEST_CASE("RecordEngine starts with no captured audio and is not recording", "[record_engine]") {
    RecordEngine engine(44100, AudioDeviceMode::None);
    CHECK_FALSE(engine.isRecording());
    CHECK_FALSE(engine.isDeviceAvailable());
    CHECK(engine.capturedAudio().frameCount() == 0);
}

TEST_CASE("RecordEngine::start with AudioDeviceMode::None never opens a real device", "[record_engine]") {
    RecordEngine engine(44100, AudioDeviceMode::None);
    engine.start();
    CHECK(engine.isRecording());
    CHECK_FALSE(engine.isDeviceAvailable());
    engine.stop();
    CHECK_FALSE(engine.isRecording());
}

TEST_CASE("processBlock followed by drainAvailable accumulates captured audio", "[record_engine]") {
    RecordEngine engine(44100, AudioDeviceMode::None);
    engine.start();

    const std::vector<float> input = makeSineTone(440.0f, 512, 44100);
    const float* inputChannels[1] = {input.data()};

    engine.processBlock(inputChannels, 1, static_cast<int>(input.size()));
    engine.drainAvailable();

    const auto& captured = engine.capturedAudio();
    REQUIRE(captured.frameCount() == input.size());
    CHECK(captured.left == input);
    // Mono input mirrored to both channels.
    CHECK(captured.right == input);

    engine.stop();
}

TEST_CASE("Capturing across several blocks accumulates all of them in order", "[record_engine]") {
    RecordEngine engine(44100, AudioDeviceMode::None);
    engine.start();

    const std::vector<float> input = makeSineTone(440.0f, 2000, 44100);
    constexpr std::size_t kBlockSize = 256;
    for (std::size_t offset = 0; offset < input.size(); offset += kBlockSize) {
        const std::size_t n = std::min(kBlockSize, input.size() - offset);
        const float* inputChannels[1] = {input.data() + offset};
        engine.processBlock(inputChannels, 1, static_cast<int>(n));
        engine.drainAvailable();
    }

    engine.stop();

    REQUIRE(engine.capturedAudio().frameCount() == input.size());
    CHECK(engine.capturedAudio().left == input);
}

TEST_CASE("start() clears any previously captured audio", "[record_engine]") {
    RecordEngine engine(44100, AudioDeviceMode::None);
    engine.start();
    const std::vector<float> input = makeSineTone(440.0f, 128, 44100);
    const float* inputChannels[1] = {input.data()};
    engine.processBlock(inputChannels, 1, static_cast<int>(input.size()));
    engine.drainAvailable();
    engine.stop();
    REQUIRE(engine.capturedAudio().frameCount() == input.size());

    engine.start();
    CHECK(engine.capturedAudio().frameCount() == 0);
    engine.stop();
}

TEST_CASE("stop() drains any samples still sitting in the ring buffer", "[record_engine]") {
    RecordEngine engine(44100, AudioDeviceMode::None);
    engine.start();

    const std::vector<float> input = makeSineTone(440.0f, 300, 44100);
    const float* inputChannels[1] = {input.data()};
    engine.processBlock(inputChannels, 1, static_cast<int>(input.size()));
    // No drainAvailable() call here - stop() itself should still capture it.

    engine.stop();

    CHECK(engine.capturedAudio().frameCount() == input.size());
}
