#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

TEST_CASE("A quiet signal is normalized up toward 0.95 peak internally, but decodes back at its own original peak",
          "[stream_codec]") {
    AudioBuffer quiet = makeSineTone(1000.0f, 0.5f, 44100);
    for (float& sample : quiet.left) sample *= 0.1f;
    for (float& sample : quiet.right) sample *= 0.1f;

    const auto image = encode(quiet, StreamCodecConfig{});
    CHECK(image.inputNormalizationScale > 1.0f);  // boosted, since peak (0.1) is well below the 0.95 target.

    const AudioBuffer decoded = decode(image);
    // Measured away from the signal's own first/last ~2000 samples - the
    // overlap-add reconstruction's own window coverage is thinner right at
    // the very edges (a handful of samples see fewer overlapping analysis
    // frames than the steady-state interior does), a real, pre-existing
    // STFT edge characteristic unrelated to normalization - see
    // docs/sound-mind-architecture.md's Known Issues for the full account.
    // Checking the interior isolates normalization's own correctness from it.
    const float decodedPeak = *std::max_element(decoded.left.begin() + 2000, decoded.left.end() - 2000);
    CHECK(decodedPeak == Catch::Approx(0.1f).margin(0.01f));
}

TEST_CASE("A loud signal is normalized down toward 0.95 peak internally, but decodes back at its own original peak",
          "[stream_codec]") {
    const AudioBuffer loud = makeSineTone(1000.0f, 0.5f, 44100);  // unit amplitude - peak 1.0, above the 0.95 target.

    const auto image = encode(loud, StreamCodecConfig{});
    CHECK(image.inputNormalizationScale < 1.0f);

    const AudioBuffer decoded = decode(image);
    // Measured away from the edges - see the quiet-signal test's own docs.
    const float decodedPeak = *std::max_element(decoded.left.begin() + 2000, decoded.left.end() - 2000);
    CHECK(decodedPeak == Catch::Approx(1.0f).margin(0.05f));
}

TEST_CASE("Silence normalizes to a scale of exactly 1.0 (nothing to measure a peak from)", "[stream_codec]") {
    AudioBuffer silence;
    silence.sampleRateHz = 44100;
    silence.left.assign(4410, 0.0f);
    silence.right.assign(4410, 0.0f);

    const auto image = encode(silence, StreamCodecConfig{});
    CHECK(image.inputNormalizationScale == 1.0f);
}

TEST_CASE("A-weighting makes a bass tone store measurably quieter than an equal-amplitude 1 kHz tone", "[stream_codec]") {
    // Both at the same amplitude, so any raw-magnitude difference is purely
    // the A-weighting curve's own doing, not a difference in input level.
    const AudioBuffer bass = makeSineTone(250.0f, 0.5f, 44100);
    const AudioBuffer reference = makeSineTone(1000.0f, 0.5f, 44100);

    const auto bassImage = encode(bass, StreamCodecConfig{});
    const auto referenceImage = encode(reference, StreamCodecConfig{});

    const float bassPeakDb = *std::max_element(bassImage.leftMagnitudeDb.begin(), bassImage.leftMagnitudeDb.end());
    const float referencePeakDb =
        *std::max_element(referenceImage.leftMagnitudeDb.begin(), referenceImage.leftMagnitudeDb.end());

    CHECK(bassPeakDb < referencePeakDb);
}

TEST_CASE("Reflection padding leaves frameCount/sampleCount tied to the true, unpadded duration", "[stream_codec]") {
    const AudioBuffer original = makeSineTone(1000.0f, 1.0f, 44100);
    const StreamCodecConfig config;  // default minFrequencyHz = 20, so padding is genuinely active.

    const auto image = encode(original, config);
    CHECK(image.sampleCount == original.frameCount());
    CHECK(image.frameCount ==
          static_cast<std::uint32_t>((original.frameCount() + config.hopLength - 1) / config.hopLength));

    const AudioBuffer decoded = decode(image);
    CHECK(decoded.left.size() == original.frameCount());
}

TEST_CASE("A signal shorter than the reflection pad length still encodes/decodes without crashing", "[stream_codec]") {
    // sampleRate/minFrequencyHz (the nominal pad length) is far longer than
    // this clip - computePadSamples()'s own clamp to numSamples-1 should
    // keep this safe, not out-of-bounds.
    const AudioBuffer original = makeSineTone(1000.0f, 0.01f, 44100);  // ~441 samples.
    const StreamCodecConfig config;  // default minFrequencyHz = 20 -> a nominal pad of 2205 samples.

    const auto image = encode(original, config);
    const AudioBuffer decoded = decode(image);

    CHECK(decoded.left.size() == original.frameCount());
}

