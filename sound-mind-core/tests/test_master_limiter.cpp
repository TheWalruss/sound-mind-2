#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "sound_mind/core/master_limiter.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::core::applyMasterLimiter;
using sound_mind::core::MasterLimiter;

namespace {

AudioBuffer sine(double amplitude, double seconds = 1.0) {
    AudioBuffer audio;
    audio.sampleRateHz = 48000;
    const auto count = static_cast<std::size_t>(seconds * 48000);
    for (std::size_t i = 0; i < count; ++i) {
        audio.left.push_back(static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * 220.0 * i / 48000.0)));
    }
    audio.right = audio.left;
    return audio;
}

float peakOf(const std::vector<float>& samples) {
    float peak = 0.0f;
    for (const float sample : samples) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

double rmsOf(const std::vector<float>& samples) {
    double sum = 0.0;
    for (const float sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

}  // namespace

TEST_CASE("The limiter keeps a mix that goes over full scale under the ceiling", "[core][limiter]") {
    AudioBuffer audio = sine(2.0);  // +6 dBFS - would hard-clip.
    applyMasterLimiter(audio, -1.0f);

    const float ceiling = std::pow(10.0f, -1.0f / 20.0f);
    CHECK(peakOf(audio.left) <= ceiling + 1e-6f);
    CHECK(peakOf(audio.right) <= ceiling + 1e-6f);
    CHECK(rmsOf(audio.left) > 0.5);  // turned down, not silenced.
    CHECK(audio.left.size() == 48000);
}

TEST_CASE("The limiter leaves audio under the ceiling untouched and aligned", "[core][limiter]") {
    const AudioBuffer original = sine(0.3);
    AudioBuffer audio = original;
    applyMasterLimiter(audio, -1.0f);
    for (std::size_t i = 0; i < audio.left.size(); i += 97) {
        REQUIRE(audio.left[i] == Catch::Approx(original.left[i]).margin(1e-6));
    }
}

TEST_CASE("Only the samples around a single peak are turned down", "[core][limiter]") {
    AudioBuffer audio = sine(0.3);
    audio.left[24000] = 1.8f;
    applyMasterLimiter(audio, -1.0f);
    CHECK(std::abs(audio.left[24000]) <= std::pow(10.0f, -1.0f / 20.0f) + 1e-6f);
    // Well before the peak (beyond the look-ahead), nothing changes.
    const AudioBuffer reference = sine(0.3);
    CHECK(audio.left[20000] == Catch::Approx(reference.left[20000]).margin(1e-6));
}

TEST_CASE("A disabled limiter passes audio through exactly", "[core][limiter]") {
    MasterLimiter limiter;
    limiter.setEnabled(false);
    AudioBuffer audio = sine(2.0, 0.1);
    const AudioBuffer original = audio;
    limiter.process(audio.left.data(), audio.right.data(), audio.left.size());
    CHECK(audio.left == original.left);
}

TEST_CASE("Processing in blocks gives the same result as all at once", "[core][limiter]") {
    AudioBuffer whole = sine(1.6, 0.2);
    AudioBuffer blocks = whole;
    MasterLimiter a;
    a.setSampleRate(48000.0);
    a.process(whole.left.data(), whole.right.data(), whole.left.size());
    MasterLimiter b;
    b.setSampleRate(48000.0);
    for (std::size_t start = 0; start < blocks.left.size(); start += 64) {
        const std::size_t count = std::min<std::size_t>(64, blocks.left.size() - start);
        b.process(blocks.left.data() + start, blocks.right.data() + start, count);
    }
    CHECK(whole.left == blocks.left);
    CHECK(a.latencySamples() == 72);  // 1.5 ms at 48 kHz.
}

TEST_CASE("The ceiling is clamped to a sensible range", "[core][limiter]") {
    MasterLimiter limiter;
    limiter.setCeilingDb(3.0f);
    CHECK(limiter.ceilingDb() == 0.0f);
    limiter.setCeilingDb(-40.0f);
    CHECK(limiter.ceilingDb() == -24.0f);
}

TEST_CASE("Without look-ahead the limiter adds no delay and still holds the ceiling", "[core][limiter]") {
    MasterLimiter limiter(MasterLimiter::LookAhead::Off);
    limiter.setSampleRate(48000.0);
    CHECK(limiter.latencySamples() == 0);
    std::vector<float> left{0.1f, 0.5f, 1.8f, -2.0f, 0.2f};
    std::vector<float> right{-0.1f, 0.4f, 0.3f, 0.0f, 0.2f};
    limiter.process(left.data(), right.data(), left.size());
    // Untouched (and undelayed) until the first peak.
    CHECK(left[0] == 0.1f);
    CHECK(left[1] == 0.5f);
    const float ceiling = std::pow(10.0f, -1.0f / 20.0f);
    for (std::size_t i = 0; i < left.size(); ++i) {
        CHECK(std::abs(left[i]) <= ceiling + 1e-6f);
    }
    // The gain recovers slowly after the peaks.
    CHECK(left[4] < 0.2f);
    CHECK(left[4] > 0.0f);
}

