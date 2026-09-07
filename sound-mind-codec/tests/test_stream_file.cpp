#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/stream_file.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::encode;
using sound_mind::codec::readStreamFile;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::codec::writeStreamFile;

TEST_CASE("A StreamImage round-trips through a file on disk", "[stream_file]") {
    AudioBuffer audio;
    audio.sampleRateHz = 44100;
    audio.left.resize(4410);
    audio.right.resize(4410);
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        const float sample = std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float>(i) / 44100.0f);
        audio.left[i] = sample;
        audio.right[i] = sample;
    }

    const StreamImage original = encode(audio, StreamCodecConfig{});

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-stream.smstream";
    writeStreamFile(path, original);
    const StreamImage loaded = readStreamFile(path);
    std::filesystem::remove(path);

    CHECK(loaded.config.sampleRateHz == original.config.sampleRateHz);
    CHECK(loaded.config.hopLength == original.config.hopLength);
    CHECK(loaded.config.binCount == original.config.binCount);
    CHECK(loaded.config.minFrequencyHz == original.config.minFrequencyHz);
    CHECK(loaded.config.maxFrequencyHz == original.config.maxFrequencyHz);
    CHECK(loaded.frameCount == original.frameCount);
    CHECK(loaded.sampleCount == original.sampleCount);
    REQUIRE(loaded.leftMagnitudeDb == original.leftMagnitudeDb);
    REQUIRE(loaded.rightMagnitudeDb == original.rightMagnitudeDb);
    REQUIRE(loaded.sharedPhaseRadians == original.sharedPhaseRadians);
}

TEST_CASE("Reading a file with bad magic bytes throws", "[stream_file]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-stream-bad.smstream";
    {
        std::ofstream junk(path, std::ios::binary);
        junk << "not a stream file";
    }

    CHECK_THROWS_AS(readStreamFile(path), std::invalid_argument);
    std::filesystem::remove(path);
}

TEST_CASE("Reading a file with an unsupported format version throws", "[stream_file]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-stream-bad-version.smstream";
    {
        std::ofstream badVersion(path, std::ios::binary);
        badVersion.write("SMST", 4);
        const std::uint32_t bogusVersion = 999;
        badVersion.write(reinterpret_cast<const char*>(&bogusVersion), sizeof(bogusVersion));
    }

    CHECK_THROWS_AS(readStreamFile(path), std::invalid_argument);
    std::filesystem::remove(path);
}
