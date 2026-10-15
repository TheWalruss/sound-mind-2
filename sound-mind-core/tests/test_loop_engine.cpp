#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "sound_mind/core/loop_engine.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::AudioDeviceMode;
using sound_mind::core::LoopEngine;

namespace {

/// @brief Drives LoopEngine::processBlock() with plain vectors, the way a
/// real audio callback would - stereo out, mono or stereo in.
struct Block {
    std::vector<float> inLeft;
    std::vector<float> inRight;
    std::vector<float> outLeft;
    std::vector<float> outRight;

    explicit Block(std::size_t samples, float inputValue = 0.0f)
        : inLeft(samples, inputValue), inRight(samples, inputValue), outLeft(samples, 0.0f), outRight(samples, 0.0f) {}

    void run(LoopEngine& engine) {
        const float* inputs[2] = {inLeft.data(), inRight.data()};
        float* outputs[2] = {outLeft.data(), outRight.data()};
        engine.processBlock(inputs, 2, outputs, 2, static_cast<int>(outLeft.size()));
    }
};

AudioBuffer constantAudio(std::size_t samples, float left, float right, std::uint32_t sampleRateHz = 44100) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    audio.left.assign(samples, left);
    audio.right.assign(samples, right);
    return audio;
}

std::vector<float> makeSineTone(float frequencyHz, std::size_t sampleCount, std::uint32_t sampleRateHz) {
    std::vector<float> samples(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        samples[i] = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) /
                                     static_cast<float>(sampleRateHz));
    }
    return samples;
}

}  // namespace

TEST_CASE("A fresh LoopEngine is idle, unfrozen, silent and at position 0", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 1024, AudioDeviceMode::None);
    CHECK_FALSE(engine.isRunning());
    CHECK_FALSE(engine.isDeviceAvailable());
    CHECK_FALSE(engine.frozen());
    CHECK(engine.takesCompleted() == 0);
    CHECK(engine.playbackPositionSamples() == 0);
    CHECK(engine.loopLengthSamples() == 1024);
    CHECK(engine.currentImage().frameCount == 0);
    CHECK(engine.currentPreviewImage().frameCount == 0);
    CHECK(engine.inputGain() == 1.0f);
    CHECK(engine.currentInputLevel() == 0.0f);
}

TEST_CASE("LoopEngine::setInputGain clamps to [0, kMaxGain]", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 1024, AudioDeviceMode::None);
    engine.setInputGain(-1.0f);
    CHECK(engine.inputGain() == 0.0f);
    engine.setInputGain(LoopEngine::kMaxGain + 1.0f);
    CHECK(engine.inputGain() == LoopEngine::kMaxGain);
}

TEST_CASE("Device preferences are stored for the next start()", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 1024, AudioDeviceMode::None);
    CHECK(engine.preferredInputDevice().empty());
    engine.setPreferredInputDevice("Mic");
    engine.setPreferredOutputDevice("Speakers");
    CHECK(engine.preferredInputDevice() == "Mic");
    CHECK(engine.preferredOutputDevice() == "Speakers");
    static_cast<void>(engine.availableInputDeviceNames());
    static_cast<void>(engine.availableOutputDeviceNames());
}

TEST_CASE("start() with AudioDeviceMode::None runs without opening a device", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 1024, AudioDeviceMode::None);
    engine.start();
    CHECK(engine.isRunning());
    CHECK_FALSE(engine.isDeviceAvailable());
    engine.stop();
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("emptyImage is a silent image as long as one loop", "[loop_engine]") {
    const StreamCodecConfig config;
    LoopEngine engine(config, 8192, AudioDeviceMode::None);
    const auto image = engine.emptyImage();
    CHECK(image.frameCount > 0);
    CHECK(image.sampleCount == 8192);
}

TEST_CASE("The output plays the staged project audio from the start, wrapping at the loop length", "[loop_engine]") {
    constexpr std::size_t loopLen = 8;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);
    AudioBuffer backing = constantAudio(loopLen, 0.0f, 0.0f);
    for (std::size_t i = 0; i < loopLen; ++i) {
        backing.left[i] = static_cast<float>(i);
        backing.right[i] = -static_cast<float>(i);
    }
    REQUIRE(engine.setBackingAudio(std::move(backing)));

    Block block(12);
    block.run(engine);

    for (std::size_t i = 0; i < 12; ++i) {
        CHECK(block.outLeft[i] == static_cast<float>(i % loopLen));
        CHECK(block.outRight[i] == -static_cast<float>(i % loopLen));
    }
    CHECK(engine.playbackPositionSamples() == 4);
}

TEST_CASE("setBackingAudio rejects audio that isn't exactly one loop long", "[loop_engine]") {
    LoopEngine engine(StreamCodecConfig{}, 8, AudioDeviceMode::None);
    CHECK_FALSE(engine.setBackingAudio(constantAudio(7, 0.0f, 0.0f)));
    CHECK(engine.setBackingAudio(constantAudio(8, 0.0f, 0.0f)));
}

TEST_CASE("Replacing the project audio while running takes effect at the next loop start, not mid-loop",
          "[loop_engine]") {
    constexpr std::size_t loopLen = 8;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);
    REQUIRE(engine.setBackingAudio(constantAudio(loopLen, 1.0f, 1.0f)));
    engine.start();  // while idle, new project audio applies immediately instead.
    Block first(4);
    first.run(engine);  // position 4 of 8.

    REQUIRE(engine.setBackingAudio(constantAudio(loopLen, 2.0f, 2.0f)));
    Block rest(8);
    rest.run(engine);

    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(rest.outLeft[i] == 1.0f);  // the rest of the current pass.
    }
    for (std::size_t i = 4; i < 8; ++i) {
        CHECK(rest.outLeft[i] == 2.0f);  // the next pass.
    }
    engine.stop();
}

TEST_CASE("A take recorded during one pass plays back during the next, mixed with the project",
          "[loop_engine]") {
    constexpr std::size_t loopLen = 8;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);
    REQUIRE(engine.setBackingAudio(constantAudio(loopLen, 0.25f, 0.25f)));

    Block firstPass(loopLen, 0.5f);  // the input during pass 1.
    firstPass.run(engine);
    for (const float sample : firstPass.outLeft) {
        CHECK(sample == 0.25f);  // no take yet - just the project.
    }
    CHECK(engine.takesCompleted() == 1);

    Block secondPass(loopLen, 0.0f);
    secondPass.run(engine);
    for (const float sample : secondPass.outLeft) {
        CHECK(sample == Catch::Approx(0.75f));  // project + the pass-1 take.
    }
}

TEST_CASE("The take's own left/right gains scale it in the mix", "[loop_engine]") {
    constexpr std::size_t loopLen = 4;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);
    engine.setTakeGains(0.5f, 0.0f);

    Block firstPass(loopLen, 1.0f);
    firstPass.run(engine);
    Block secondPass(loopLen, 0.0f);
    secondPass.run(engine);

    CHECK(secondPass.outLeft[0] == Catch::Approx(0.5f));
    CHECK(secondPass.outRight[0] == Catch::Approx(0.0f));
}

TEST_CASE("Freezing keeps the current take and stops recording; unfreezing starts a fresh take at the next pass",
          "[loop_engine]") {
    constexpr std::size_t loopLen = 4;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);

    Block pass1(loopLen, 0.5f);
    pass1.run(engine);  // take = 0.5.
    engine.setFrozen(true);
    CHECK(engine.frozen());

    Block pass2(loopLen, 0.9f);  // ignored while frozen.
    pass2.run(engine);
    Block pass3(loopLen, 0.0f);
    pass3.run(engine);
    CHECK(pass2.outLeft[0] == Catch::Approx(0.5f));
    CHECK(pass3.outLeft[0] == Catch::Approx(0.5f));  // still the frozen take.
    CHECK(engine.takesCompleted() == 1);

    // Unfreezing mid-pass doesn't record a partial take over the frozen one.
    Block half(2, 0.2f);
    engine.setFrozen(false);
    half.run(engine);
    Block restOfPass(2, 0.2f);
    restOfPass.run(engine);
    CHECK(engine.takesCompleted() == 1);
    Block nextPass(loopLen, 0.3f);  // recorded.
    nextPass.run(engine);
    CHECK(engine.takesCompleted() == 2);
    Block after(loopLen, 0.0f);
    after.run(engine);
    CHECK(after.outLeft[0] == Catch::Approx(0.3f));
}

TEST_CASE("Captured input is scaled by the input gain and reported as the input level", "[loop_engine]") {
    constexpr std::size_t loopLen = 4;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);
    engine.setInputGain(0.5f);
    Block pass1(loopLen, 0.8f);
    pass1.run(engine);
    CHECK(engine.currentInputLevel() == Catch::Approx(0.4f));
    Block pass2(loopLen, 0.0f);
    pass2.run(engine);
    CHECK(pass2.outLeft[0] == Catch::Approx(0.4f));
}

TEST_CASE("processPendingAudio encodes each finished take into currentImage", "[loop_engine]") {
    const StreamCodecConfig config;
    const std::size_t loopLen = 8192;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);
    const auto tone = makeSineTone(1000.0f, loopLen, config.sampleRateHz);
    Block pass(loopLen);
    pass.inLeft = tone;
    pass.inRight = tone;
    pass.run(engine);

    engine.processPendingAudio();

    const auto image = engine.currentImage();
    REQUIRE(image.frameCount > 0);
    CHECK(image.sampleCount == loopLen);
    const float peakDb = *std::max_element(image.leftMagnitudeDb.begin(), image.leftMagnitudeDb.end());
    CHECK(peakDb > -12.0f);  // a 0.5-amplitude tone, about -6 dBFS.
}

TEST_CASE("currentPreviewImage grows during a pass and starts over at the next one", "[loop_engine]") {
    const StreamCodecConfig config;
    const std::size_t loopLen = 16384;
    LoopEngine engine(config, loopLen, AudioDeviceMode::None);
    const auto tone = makeSineTone(1000.0f, loopLen, config.sampleRateHz);

    Block firstHalf(loopLen / 2);
    firstHalf.inLeft.assign(tone.begin(), tone.begin() + static_cast<long>(loopLen / 2));
    firstHalf.inRight = firstHalf.inLeft;
    firstHalf.run(engine);
    engine.processPendingAudio();
    const auto halfway = engine.currentPreviewImage().frameCount;
    CHECK(halfway > 0);

    Block secondHalf(loopLen / 2);
    secondHalf.inLeft.assign(tone.begin() + static_cast<long>(loopLen / 2), tone.end());
    secondHalf.inRight = secondHalf.inLeft;
    secondHalf.run(engine);
    Block bitOfNext(1024);
    bitOfNext.run(engine);
    engine.processPendingAudio();

    CHECK(engine.currentPreviewImage().frameCount < halfway);
}

TEST_CASE("start() resets position, takes and images", "[loop_engine]") {
    constexpr std::size_t loopLen = 4;
    LoopEngine engine(StreamCodecConfig{}, loopLen, AudioDeviceMode::None);
    Block pass(loopLen + 1, 0.5f);
    pass.run(engine);
    REQUIRE(engine.takesCompleted() == 1);

    engine.start();
    CHECK(engine.playbackPositionSamples() == 0);
    CHECK(engine.takesCompleted() == 0);
    CHECK(engine.currentImage().frameCount == 0);
    engine.stop();

    Block silent(loopLen, 0.0f);
    silent.run(engine);
    CHECK(silent.outLeft[0] == 0.0f);  // the old take is gone.
}
