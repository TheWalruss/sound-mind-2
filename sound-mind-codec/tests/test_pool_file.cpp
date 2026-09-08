#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/pool_file.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::poolEncode;
using sound_mind::codec::PoolImage;
using sound_mind::codec::readPoolFile;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::writePoolFile;

namespace {

AudioBuffer makeTestAudio() {
    AudioBuffer audio;
    audio.sampleRateHz = 44100;
    audio.left.resize(4410);
    audio.right.resize(4410);
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        const float phase = 2.0f * std::numbers::pi_v<float> * 1000.0f * static_cast<float>(i) / 44100.0f;
        audio.left[i] = std::sin(phase);
        audio.right[i] = std::sin(phase + 0.7f) * 0.6f;
    }
    return audio;
}

}  // namespace

TEST_CASE("A PoolImage round-trips through a file on disk within 16-bit quantization tolerance", "[pool_file]") {
    const AudioBuffer audio = makeTestAudio();
    StreamCodecConfig config;
    config.binCount = 64;
    const PoolImage original = poolEncode(audio, config);

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool.smpool";
    writePoolFile(path, original);
    const PoolImage loaded = readPoolFile(path);
    std::filesystem::remove(path);

    CHECK(loaded.config.sampleRateHz == original.config.sampleRateHz);
    CHECK(loaded.config.hopLength == original.config.hopLength);
    CHECK(loaded.config.binCount == original.config.binCount);
    CHECK(loaded.config.minFrequencyHz == Catch::Approx(original.config.minFrequencyHz).margin(0.01));
    CHECK(loaded.config.maxFrequencyHz == Catch::Approx(original.config.maxFrequencyHz).margin(0.01));
    CHECK(loaded.frameCount == original.frameCount);
    CHECK(loaded.sampleCount == original.sampleCount);

    REQUIRE(loaded.leftMagnitudeDb.size() == original.leftMagnitudeDb.size());
    constexpr float kDbTolerance = 96.0f / 65535.0f;
    for (std::size_t i = 0; i < original.leftMagnitudeDb.size(); ++i) {
        const float clampedOriginal = std::clamp(original.leftMagnitudeDb[i], -96.0f, 0.0f);
        CHECK(loaded.leftMagnitudeDb[i] == Catch::Approx(clampedOriginal).margin(kDbTolerance));
    }
}

TEST_CASE("Reading a file that isn't a Pool file throws", "[pool_file]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-pool-bad.smpool";
    {
        std::ofstream junk(path, std::ios::binary);
        junk << "not a pool file";
    }

    CHECK_THROWS(readPoolFile(path));
    std::filesystem::remove(path);
}
