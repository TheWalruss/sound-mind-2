#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "sound_mind/codec/stream_codec.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::decode;
using sound_mind::codec::encode;
using sound_mind::codec::StreamCodecConfig;

namespace {

/// @brief A stereo sine tone, `rightGain` scaling the right channel
/// relative to the left - 1.0 gives an identical (mono-in-stereo) signal on
/// both channels, anything else gives a genuinely different right channel.
AudioBuffer makeSineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz, float rightGain = 1.0f) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto sampleCount = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    audio.left.resize(sampleCount);
    audio.right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample =
            std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
        audio.left[i] = sample;
        audio.right[i] = sample * rightGain;
    }
    return audio;
}

/// @brief Pearson correlation coefficient between two signals, truncated to
/// the shorter one's length - a simple, well-understood proxy for "sounds
/// like the same signal" that doesn't require sample-exact equality.
float correlation(const std::vector<float>& a, const std::vector<float>& b) {
    const std::size_t n = std::min(a.size(), b.size());
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
        normA += static_cast<double>(a[i]) * static_cast<double>(a[i]);
        normB += static_cast<double>(b[i]) * static_cast<double>(b[i]);
    }
    if (normA <= 0.0 || normB <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(dot / std::sqrt(normA * normB));
}

}  // namespace

TEST_CASE("StreamCodecConfig has sensible defaults", "[stream_codec]") {
    const StreamCodecConfig config;

    CHECK(config.sampleRateHz == 44100);
    CHECK(config.hopLength == 441);
    CHECK(config.binCount == 512);
    CHECK(config.minFrequencyHz == 20.0f);
    CHECK(config.maxFrequencyHz == 16000.0f);
}

TEST_CASE("A mono-in-stereo tone round-trips through the Stream codec with high fidelity", "[stream_codec]") {
    // Left and right are identical here, so the shared-phase channel (see
    // StreamImage's docs) equals both channels' true phase exactly - this
    // isolates the STFT/log-binning/overlap-add pipeline's own correctness
    // from the shared-phase approximation's inherent lossiness.
    const AudioBuffer original = makeSineTone(440.0f, 1.0f, 44100);

    const StreamCodecConfig config;
    const auto image = encode(original, config);
    const AudioBuffer decoded = decode(image);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    REQUIRE(decoded.frameCount() == original.frameCount());
    CHECK(correlation(decoded.left, original.left) > 0.99f);
    CHECK(correlation(decoded.right, original.right) > 0.99f);
}

TEST_CASE("A genuinely stereo tone still round-trips recognizably", "[stream_codec]") {
    // Right channel at half gain - genuinely different from left, so the
    // shared-phase approximation is inexact here, unlike the mono-in-stereo
    // case above. Still expected to preserve the fundamental frequency and
    // stay well-correlated with the original, just with a looser tolerance.
    const AudioBuffer original = makeSineTone(440.0f, 1.0f, 44100, 0.5f);

    const StreamCodecConfig config;
    const auto image = encode(original, config);
    const AudioBuffer decoded = decode(image);

    CHECK(correlation(decoded.left, original.left) > 0.9f);
    CHECK(correlation(decoded.right, original.right) > 0.9f);
}

TEST_CASE("Encoding carries the codec config and sample count needed to decode", "[stream_codec]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 48000);
    StreamCodecConfig config;
    config.binCount = 256;
    config.hopLength = 480;

    const auto image = encode(original, config);

    CHECK(image.config.sampleRateHz == 48000);
    CHECK(image.config.binCount == 256);
    CHECK(image.config.hopLength == 480);
    CHECK(image.sampleCount == original.frameCount());
    CHECK(image.leftMagnitudeDb.size() == static_cast<std::size_t>(image.config.binCount) * image.frameCount);
}
