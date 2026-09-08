#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/stream_incremental_encoder.h"

using sound_mind::codec::encode;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::codec::StreamIncrementalEncoder;

namespace {

/// @brief A stereo sine tone, as separate left/right sample vectors -
/// StreamIncrementalEncoder::pushSamples() takes raw pointers (matching
/// what an audio callback hands over), not an AudioBuffer.
void makeSineTone(float frequencyHz, std::size_t sampleCount, std::uint32_t sampleRateHz, std::vector<float>& left,
                   std::vector<float>& right) {
    left.resize(sampleCount);
    right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample =
            std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
        left[i] = sample;
        right[i] = sample;
    }
}

}  // namespace

TEST_CASE("StreamIncrementalEncoder starts with no frames or samples", "[stream_incremental_encoder]") {
    StreamIncrementalEncoder encoder{StreamCodecConfig{}};
    CHECK(encoder.sampleCount() == 0);
    CHECK(encoder.frameCount() == 0);

    const StreamImage image = encoder.snapshot();
    CHECK(image.frameCount == 0);
    CHECK(image.sampleCount == 0);
}

TEST_CASE("StreamIncrementalEncoder only finalizes frames once their whole window has real audio",
          "[stream_incremental_encoder]") {
    StreamCodecConfig config;
    const std::uint32_t fftSize = config.hopLength * 4;  // matches fftSizeFor()'s own 4x hop ratio.

    std::vector<float> left;
    std::vector<float> right;
    makeSineTone(440.0f, fftSize - 1, config.sampleRateHz, left, right);

    StreamIncrementalEncoder encoder{config};
    encoder.pushSamples(left.data(), right.data(), left.size());

    // One sample short of frame 0's full window - nothing should finalize yet.
    CHECK(encoder.frameCount() == 0);
    CHECK(encoder.sampleCount() == fftSize - 1);

    const float lastSample = 0.0f;
    encoder.pushSamples(&lastSample, &lastSample, 1);
    CHECK(encoder.frameCount() == 1);
}

TEST_CASE("StreamIncrementalEncoder produces the same data as encode() for the frames both agree on",
          "[stream_incremental_encoder]") {
    StreamCodecConfig config;
    std::vector<float> left;
    std::vector<float> right;
    makeSineTone(1000.0f, 44100, config.sampleRateHz, left, right);

    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    audio.left = left;
    audio.right = right;
    const StreamImage wholeBuffer = encode(audio, config);

    StreamIncrementalEncoder encoder{config};
    encoder.pushSamples(left.data(), right.data(), left.size());
    const StreamImage incremental = encoder.snapshot();

    // The incremental encoder never zero-pads a frame's window with fake
    // future audio (see the class's own docs) - it should report no more
    // frames than the whole-buffer encode, and typically a few fewer.
    REQUIRE(incremental.frameCount > 0);
    REQUIRE(incremental.frameCount <= wholeBuffer.frameCount);

    for (std::uint32_t frame = 0; frame < incremental.frameCount; ++frame) {
        for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
            const std::size_t cell = std::size_t{bin} * incremental.frameCount + frame;
            const std::size_t wholeCell = std::size_t{bin} * wholeBuffer.frameCount + frame;
            CHECK(incremental.leftMagnitudeDb[cell] == wholeBuffer.leftMagnitudeDb[wholeCell]);
            CHECK(incremental.rightMagnitudeDb[cell] == wholeBuffer.rightMagnitudeDb[wholeCell]);
            CHECK(incremental.sharedPhaseRadians[cell] == wholeBuffer.sharedPhaseRadians[wholeCell]);
        }
    }
}

TEST_CASE("StreamIncrementalEncoder gives the same result regardless of how audio is chunked",
          "[stream_incremental_encoder]") {
    StreamCodecConfig config;
    std::vector<float> left;
    std::vector<float> right;
    makeSineTone(1000.0f, 44100, config.sampleRateHz, left, right);

    StreamIncrementalEncoder wholeChunkEncoder{config};
    wholeChunkEncoder.pushSamples(left.data(), right.data(), left.size());

    // Simulate real-time arrival: small, irregularly-sized blocks, matching
    // how an audio device callback would actually hand over samples.
    StreamIncrementalEncoder smallChunkEncoder{config};
    constexpr std::size_t kChunkSize = 127;
    for (std::size_t offset = 0; offset < left.size(); offset += kChunkSize) {
        const std::size_t n = std::min(kChunkSize, left.size() - offset);
        smallChunkEncoder.pushSamples(left.data() + offset, right.data() + offset, n);
    }

    REQUIRE(smallChunkEncoder.frameCount() == wholeChunkEncoder.frameCount());
    const StreamImage a = wholeChunkEncoder.snapshot();
    const StreamImage b = smallChunkEncoder.snapshot();
    CHECK(a.leftMagnitudeDb == b.leftMagnitudeDb);
    CHECK(a.rightMagnitudeDb == b.rightMagnitudeDb);
    CHECK(a.sharedPhaseRadians == b.sharedPhaseRadians);
}
