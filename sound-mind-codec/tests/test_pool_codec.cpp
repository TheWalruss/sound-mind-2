#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "sound_mind/codec/pool_codec.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::poolDecode;
using sound_mind::codec::poolEncode;
using sound_mind::codec::StreamCodecConfig;

namespace {

/// @brief A stereo sine tone, `rightGain`/`rightPhaseOffset` making the
/// right channel genuinely different from the left.
AudioBuffer makeSineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz, float rightGain = 1.0f,
                          float rightPhaseOffset = 0.0f) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto sampleCount = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    audio.left.resize(sampleCount);
    audio.right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float phase = 2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz);
        audio.left[i] = std::sin(phase);
        audio.right[i] = std::sin(phase + rightPhaseOffset) * rightGain;
    }
    return audio;
}

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

TEST_CASE("A mono-in-stereo tone round-trips through the Pool codec with very high fidelity", "[pool_codec]") {
    const AudioBuffer original = makeSineTone(1000.0f, 1.0f, 44100);

    const StreamCodecConfig config;
    const auto image = poolEncode(original, config);
    const AudioBuffer decoded = poolDecode(image);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    REQUIRE(decoded.frameCount() == original.frameCount());
    CHECK(correlation(decoded.left, original.left) > 0.999f);
    CHECK(correlation(decoded.right, original.right) > 0.999f);
}

TEST_CASE("A genuinely stereo tone still round-trips with very high fidelity", "[pool_codec]") {
    // Unlike the Stream codec (shared phase across channels), Pool keeps
    // independent left/right phase - so a genuinely different right
    // channel (different gain AND phase offset here) should round-trip
    // just as well as the mono-in-stereo case above, not degrade the way
    // Stream's does for the same input.
    const AudioBuffer original = makeSineTone(1000.0f, 1.0f, 44100, 0.5f, 1.2f);

    const StreamCodecConfig config;
    const auto image = poolEncode(original, config);
    const AudioBuffer decoded = poolDecode(image);

    CHECK(correlation(decoded.left, original.left) > 0.999f);
    CHECK(correlation(decoded.right, original.right) > 0.999f);
}

TEST_CASE("PoolImage stores independent left and right phase", "[pool_codec]") {
    // A sanity check on the format itself, not just the round trip: with
    // genuinely different L/R phase content, the stored phase planes
    // should actually differ from each other - confirming this isn't
    // accidentally collapsing to a shared value the way StreamImage does.
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100, 1.0f, 1.5f);
    const auto image = poolEncode(original, StreamCodecConfig{});

    bool anyDifferent = false;
    for (std::size_t i = 0; i < image.leftPhaseRadians.size(); ++i) {
        if (image.leftPhaseRadians[i] != image.rightPhaseRadians[i]) {
            anyDifferent = true;
            break;
        }
    }
    CHECK(anyDifferent);
}

TEST_CASE("Encoding carries the codec config and sample count needed to decode", "[pool_codec]") {
    const AudioBuffer original = makeSineTone(2000.0f, 0.5f, 48000);
    StreamCodecConfig config;
    config.binCount = 128;
    config.hopLength = 480;

    const auto image = poolEncode(original, config);

    CHECK(image.config.sampleRateHz == 48000);
    CHECK(image.config.binCount == 128);
    CHECK(image.config.hopLength == 480);
    CHECK(image.sampleCount == original.frameCount());
    CHECK(image.leftMagnitudeDb.size() == static_cast<std::size_t>(image.config.binCount) * image.frameCount);
}

TEST_CASE("Energy outside the encoded frequency range is silently dropped, not corrupting the rest", "[pool_codec]") {
    // A tone well above maxFrequencyHz should decode to near-silence, not
    // garbage - confirming the "outside the range is an accepted loss, not
    // a bug" behavior documented on poolEncode().
    StreamCodecConfig config;
    config.maxFrequencyHz = 8000.0f;
    const AudioBuffer original = makeSineTone(18000.0f, 0.25f, 44100);

    const auto image = poolEncode(original, config);
    const AudioBuffer decoded = poolDecode(image);

    double sumSquares = 0.0;
    for (const float sample : decoded.left) {
        sumSquares += static_cast<double>(sample) * sample;
    }
    const double rms = std::sqrt(sumSquares / static_cast<double>(decoded.left.size()));
    CHECK(rms < 0.05);
}

TEST_CASE("A quiet signal is normalized up toward 0.95 peak internally, but decodes back at its own original peak",
          "[pool_codec]") {
    AudioBuffer quiet = makeSineTone(1000.0f, 0.5f, 44100);
    for (float& sample : quiet.left) sample *= 0.1f;
    for (float& sample : quiet.right) sample *= 0.1f;

    const auto image = poolEncode(quiet, StreamCodecConfig{});

    CHECK(image.inputNormalizationScale > 1.0f);  // boosted, since peak (0.1) is well below the 0.95 target.

    const AudioBuffer decoded = poolDecode(image);
    const float decodedPeak = *std::max_element(decoded.left.begin(), decoded.left.end());
    CHECK(decodedPeak == Catch::Approx(0.1f).margin(0.01f));
}

TEST_CASE("A loud signal is normalized down toward 0.95 peak internally, but decodes back at its own original peak",
          "[pool_codec]") {
    const AudioBuffer loud = makeSineTone(1000.0f, 0.5f, 44100);  // unit amplitude - peak 1.0, above the 0.95 target.

    const auto image = poolEncode(loud, StreamCodecConfig{});

    CHECK(image.inputNormalizationScale < 1.0f);

    const AudioBuffer decoded = poolDecode(image);
    const float decodedPeak = *std::max_element(decoded.left.begin(), decoded.left.end());
    CHECK(decodedPeak == Catch::Approx(1.0f).margin(0.05f));
}

TEST_CASE("Silence normalizes to a scale of exactly 1.0 (nothing to measure a peak from)", "[pool_codec]") {
    AudioBuffer silence;
    silence.sampleRateHz = 44100;
    silence.left.assign(4410, 0.0f);
    silence.right.assign(4410, 0.0f);

    const auto image = poolEncode(silence, StreamCodecConfig{});
    CHECK(image.inputNormalizationScale == 1.0f);
}

TEST_CASE("A-weighting makes a bass tone store measurably quieter than an equal-amplitude 1 kHz tone", "[pool_codec]") {
    // Both at the same amplitude (so any raw-magnitude difference is purely
    // the A-weighting curve's own doing, not a difference in input level).
    const AudioBuffer bass = makeSineTone(250.0f, 0.5f, 44100);
    const AudioBuffer reference = makeSineTone(1000.0f, 0.5f, 44100);

    const auto bassImage = poolEncode(bass, StreamCodecConfig{});
    const auto referenceImage = poolEncode(reference, StreamCodecConfig{});

    const float bassPeakDb = *std::max_element(bassImage.leftMagnitudeDb.begin(), bassImage.leftMagnitudeDb.end());
    const float referencePeakDb =
        *std::max_element(referenceImage.leftMagnitudeDb.begin(), referenceImage.leftMagnitudeDb.end());

    // A-weighting attenuates 250 Hz relative to 1 kHz (per the standard
    // IEC 61672 curve) - confirms the weighting is actually being applied,
    // not just present-but-zero.
    CHECK(bassPeakDb < referencePeakDb);
}

TEST_CASE("A signal much shorter than a hop still encodes/decodes without crashing", "[pool_codec]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.01f, 44100);  // ~441 samples.
    const auto image = poolEncode(original, StreamCodecConfig{});
    const AudioBuffer decoded = poolDecode(image);

    CHECK(decoded.left.size() == original.frameCount());
}
