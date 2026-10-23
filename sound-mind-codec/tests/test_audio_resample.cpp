#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/audio_resample.h"
#include "sound_mind/codec/stream_codec.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::resample;

namespace {

AudioBuffer tone(double frequencyHz, std::uint32_t sampleRateHz, double seconds, float amplitude = 0.5f) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto frames = static_cast<std::size_t>(std::llround(seconds * sampleRateHz));
    audio.left.resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        audio.left[i] = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequencyHz *
                                                                 static_cast<double>(i) / sampleRateHz));
    }
    audio.right = audio.left;
    return audio;
}

/// RMS of `samples` between `from` and `to` (a fraction of its length),
/// away from the edges where the filter has no neighbours.
double middleRms(const std::vector<float>& samples, double from = 0.1, double to = 0.9) {
    const auto begin = static_cast<std::size_t>(from * static_cast<double>(samples.size()));
    const auto end = static_cast<std::size_t>(to * static_cast<double>(samples.size()));
    double sum = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        sum += static_cast<double>(samples[i]) * samples[i];
    }
    return std::sqrt(sum / static_cast<double>(std::max<std::size_t>(end - begin, 1)));
}

/// The loudest bin of a frame in the middle of `image`.
std::uint32_t loudestBin(const sound_mind::codec::StreamImage& image) {
    const std::uint32_t frame = image.frameCount / 2;
    std::uint32_t best = 0;
    for (std::uint32_t bin = 1; bin < image.config.binCount; ++bin) {
        if (image.leftMagnitudeDb[bin * image.frameCount + frame] >
            image.leftMagnitudeDb[best * image.frameCount + frame]) {
            best = bin;
        }
    }
    return best;
}

}  // namespace

// v0.1.49.3 - converting audio to the project's own sample rate.
TEST_CASE("Resampling to the same rate copies the audio unchanged", "[codec][resample]") {
    const AudioBuffer audio = tone(440.0, 44100, 0.1);
    const AudioBuffer same = resample(audio, 44100);
    CHECK(same.sampleRateHz == 44100);
    CHECK(same.left == audio.left);
    CHECK(same.right == audio.right);
}

TEST_CASE("Resampling an empty buffer gives an empty buffer at the new rate", "[codec][resample]") {
    AudioBuffer empty;
    empty.sampleRateHz = 44100;
    const AudioBuffer result = resample(empty, 48000);
    CHECK(result.sampleRateHz == 48000);
    CHECK(result.frameCount() == 0);
    CHECK(result.right.empty());
}

TEST_CASE("Resampling keeps the duration", "[codec][resample]") {
    const AudioBuffer up = resample(tone(440.0, 44100, 1.0), 48000);
    CHECK(up.sampleRateHz == 48000);
    CHECK(up.frameCount() == 48000);
    CHECK(up.right.size() == up.left.size());
    const AudioBuffer down = resample(tone(440.0, 48000, 1.0), 44100);
    CHECK(down.frameCount() == 44100);
}

TEST_CASE("Resampling keeps a tone's pitch and level", "[codec][resample]") {
    // 1 kHz at 44.1 kHz, converted to 48 kHz, matches 1 kHz generated at
    // 48 kHz sample for sample (away from the edges).
    const AudioBuffer converted = resample(tone(1000.0, 44100, 0.5), 48000);
    const AudioBuffer ideal = tone(1000.0, 48000, 0.5);
    REQUIRE(converted.frameCount() == ideal.frameCount());
    double worst = 0.0;
    for (std::size_t i = converted.frameCount() / 10; i < converted.frameCount() * 9 / 10; ++i) {
        worst = std::max(worst, static_cast<double>(std::abs(converted.left[i] - ideal.left[i])));
    }
    CHECK(worst < 1e-3);  // about -66 dB below the 0.5 tone.
    CHECK(std::abs(middleRms(converted.left) - middleRms(ideal.left)) < 1e-3);
}

TEST_CASE("Resampling down removes what the new rate can't hold instead of aliasing it", "[codec][resample]") {
    // 23 kHz is above 44.1 kHz's 22.05 kHz limit: it must not fold back.
    const AudioBuffer high = resample(tone(23000.0, 48000, 0.5), 44100);
    CHECK(middleRms(high.left) < 0.5 * 0.01);  // below -40 dB of the original.
    // 10 kHz passes untouched.
    const AudioBuffer passing = resample(tone(10000.0, 48000, 0.5), 44100);
    CHECK(std::abs(middleRms(passing.left) - 0.5 / std::sqrt(2.0)) < 0.01);
}

TEST_CASE("Encoding audio at another rate converts it to the configuration's rate first", "[codec][resample]") {
    sound_mind::codec::StreamCodecConfig config;
    config.sampleRateHz = 48000;
    config.hopLength = 480;
    const auto fromOtherRate = sound_mind::codec::encode(tone(1000.0, 44100, 0.5), config);
    const auto native = sound_mind::codec::encode(tone(1000.0, 48000, 0.5), config);
    CHECK(fromOtherRate.config.sampleRateHz == 48000);
    CHECK(fromOtherRate.frameCount == native.frameCount);  // the same 0.5 s of columns.
    CHECK(loudestBin(fromOtherRate) == loudestBin(native));  // the same pitch.
}
