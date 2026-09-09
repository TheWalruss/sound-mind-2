#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "sound_mind/core/loop_engine.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::AudioDeviceMode;
using sound_mind::core::LoopEngine;

namespace {

/// @brief A mono sine tone, as a plain sample vector - LoopEngine::
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

TEST_CASE("LoopEngine starts with no image, not running, and keepLooping off", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 1024, AudioDeviceMode::None);
    CHECK_FALSE(engine.isRunning());
    CHECK_FALSE(engine.isDeviceAvailable());
    CHECK_FALSE(engine.keepLooping());
    CHECK(engine.currentImage().frameCount == 0);
    CHECK(engine.loopsCaptured() == 0);
    CHECK(engine.loopsBehind() == 0);
}

TEST_CASE("LoopEngine::start with AudioDeviceMode::None never opens a real device", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 1024, AudioDeviceMode::None);
    engine.start();
    CHECK(engine.isRunning());
    CHECK_FALSE(engine.isDeviceAvailable());
    engine.stop();
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("processPendingAudio does nothing until a whole loop has been captured", "[loop_engine]") {
    const StreamCodecConfig config;
    constexpr std::size_t loopLen = 64;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);

    const std::vector<float> input = makeSineTone(440.0f, loopLen - 1, config.sampleRateHz);
    const float* inputChannels[1] = {input.data()};
    std::vector<float> scratchOut(input.size(), 0.0f);
    float* outputChannels[1] = {scratchOut.data()};

    engine.processBlock(inputChannels, 1, outputChannels, 1, static_cast<int>(input.size()));
    engine.processPendingAudio();

    CHECK(engine.currentImage().frameCount == 0);
    CHECK(engine.loopsCaptured() == 0);
}

TEST_CASE("A completed loop is encoded and decoded, updating currentImage()/loopsCaptured()", "[loop_engine]") {
    const StreamCodecConfig config;
    constexpr std::size_t loopLen = 64;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);

    const std::vector<float> input = makeSineTone(440.0f, loopLen, config.sampleRateHz);
    const float* inputChannels[1] = {input.data()};
    std::vector<float> scratchOut(input.size(), 0.0f);
    float* outputChannels[1] = {scratchOut.data()};

    engine.processBlock(inputChannels, 1, outputChannels, 1, static_cast<int>(input.size()));
    engine.processPendingAudio();

    CHECK(engine.currentImage().frameCount > 0);
    CHECK(engine.loopsCaptured() == 1);
    CHECK(engine.loopsBehind() == 0);
}

TEST_CASE(
    "Even with the worker keeping up perfectly, a captured loop is first heard two playback loops later - "
    "see LoopEngine's own docs for why this fixed baseline is structural, not a bug",
    "[loop_engine]") {
    const StreamCodecConfig config;
    constexpr std::size_t loopLen = 32;
    constexpr std::size_t chunk = 4;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);

    const std::vector<float> loop0Tone = makeSineTone(440.0f, loopLen, config.sampleRateHz);
    const std::vector<float> silence(loopLen, 0.0f);

    // Feed three whole loops in small chunks (simulating real callback
    // timing), draining the worker after every chunk so it comfortably
    // keeps up: loop 0 captures a real tone, loops 1 and 2 capture silence
    // (their content is irrelevant - only what's *played back* during them
    // is under test here).
    std::vector<float> output(loopLen * 3, -1.0f);
    auto feedLoop = [&](const std::vector<float>& captureInput, std::size_t outputOffset) {
        for (std::size_t offset = 0; offset < loopLen; offset += chunk) {
            const float* in[1] = {captureInput.data() + offset};
            float* out[1] = {output.data() + outputOffset + offset};
            engine.processBlock(in, 1, out, 1, static_cast<int>(chunk));
            engine.processPendingAudio();
        }
    };
    feedLoop(loop0Tone, 0);
    feedLoop(silence, loopLen);
    feedLoop(silence, loopLen * 2);

    CHECK(engine.loopsCaptured() == 3);
    CHECK(engine.loopsBehind() == 0);  // the worker never fell behind - see the fixed-baseline note above.

    const auto isZero = [](float sample) { return sample == 0.0f; };
    const bool playbackLoop0Silent = std::all_of(output.begin(), output.begin() + static_cast<long>(loopLen), isZero);
    const bool playbackLoop1Silent = std::all_of(output.begin() + static_cast<long>(loopLen),
                                                  output.begin() + static_cast<long>(loopLen * 2), isZero);
    const bool playbackLoop2AnyNonZero =
        std::any_of(output.begin() + static_cast<long>(loopLen * 2), output.end(), [](float s) { return s != 0.0f; });

    CHECK(playbackLoop0Silent);
    CHECK(playbackLoop1Silent);
    CHECK(playbackLoop2AnyNonZero);
}

TEST_CASE("keepLooping discards newly captured input instead of recording over the last take", "[loop_engine]") {
    const StreamCodecConfig config;
    constexpr std::size_t loopLen = 32;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);
    engine.setKeepLooping(true);
    CHECK(engine.keepLooping());

    const std::vector<float> input = makeSineTone(440.0f, loopLen * 2, config.sampleRateHz);
    const float* inputChannels[1] = {input.data()};
    std::vector<float> scratchOut(input.size(), 0.0f);
    float* outputChannels[1] = {scratchOut.data()};

    engine.processBlock(inputChannels, 1, outputChannels, 1, static_cast<int>(input.size()));
    engine.processPendingAudio();

    // Nothing was ever captured - see processBlock()'s docs.
    CHECK(engine.loopsCaptured() == 0);
    CHECK(engine.currentImage().frameCount == 0);
}

TEST_CASE("loopsBehind reflects loops captured but not yet processed by the worker", "[loop_engine]") {
    const StreamCodecConfig config;
    constexpr std::size_t loopLen = 32;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);

    const std::vector<float> input = makeSineTone(440.0f, loopLen * 3, config.sampleRateHz);
    const float* inputChannels[1] = {input.data()};
    std::vector<float> scratchOut(input.size(), 0.0f);
    float* outputChannels[1] = {scratchOut.data()};

    // Feed three whole loops in one go, without ever draining the worker in
    // between - simulates it having fallen behind by two whole loops (the
    // third/most recent loop is "the just-completed one", not itself a
    // backlog - see loopsBehind()'s docs).
    engine.processBlock(inputChannels, 1, outputChannels, 1, static_cast<int>(input.size()));

    CHECK(engine.loopsCaptured() == 3);
    CHECK(engine.loopsBehind() == 3);

    // One processPendingAudio() call drains everything queued, a whole loop
    // at a time.
    engine.processPendingAudio();

    CHECK(engine.loopsBehind() == 0);
    CHECK(engine.loopsCaptured() == 3);
}
