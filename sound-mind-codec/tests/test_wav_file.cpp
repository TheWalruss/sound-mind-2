#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "sound_mind/codec/wav_file.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::readWavFile;

namespace {

void appendUint32(std::vector<char>& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

void appendUint16(std::vector<char>& bytes, std::uint16_t value) {
    for (int i = 0; i < 2; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

void appendTag(std::vector<char>& bytes, const char* tag) {
    bytes.insert(bytes.end(), tag, tag + 4);
}

/// @brief Builds a minimal 16-bit PCM WAV file's raw bytes from interleaved
/// int16 samples (mono if `numChannels == 1`, else stereo).
std::vector<char> buildPcm16Wav(std::uint32_t sampleRateHz, std::uint16_t numChannels,
                                 const std::vector<std::int16_t>& interleavedSamples) {
    std::vector<char> bytes;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(interleavedSamples.size() * sizeof(std::int16_t));
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(numChannels * sizeof(std::int16_t));

    appendTag(bytes, "RIFF");
    appendUint32(bytes, 36 + dataSize);
    appendTag(bytes, "WAVE");

    appendTag(bytes, "fmt ");
    appendUint32(bytes, 16);
    appendUint16(bytes, 1);  // PCM
    appendUint16(bytes, numChannels);
    appendUint32(bytes, sampleRateHz);
    appendUint32(bytes, sampleRateHz * blockAlign);
    appendUint16(bytes, blockAlign);
    appendUint16(bytes, 16);  // bits per sample

    appendTag(bytes, "data");
    appendUint32(bytes, dataSize);
    for (const std::int16_t sample : interleavedSamples) {
        appendUint16(bytes, static_cast<std::uint16_t>(sample));
    }

    return bytes;
}

std::filesystem::path writeTempFile(const std::vector<char>& bytes, const char* filename) {
    const auto path = std::filesystem::temp_directory_path() / filename;
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}  // namespace

TEST_CASE("Reading a 16-bit PCM stereo WAV file reproduces its samples", "[wav_file]") {
    const std::vector<std::int16_t> interleaved = {100, -200, 300, -400, 32767, -32768};
    const auto bytes = buildPcm16Wav(44100, 2, interleaved);
    const auto path = writeTempFile(bytes, "sound-mind-test-stereo.wav");

    const AudioBuffer audio = readWavFile(path);
    std::filesystem::remove(path);

    REQUIRE(audio.sampleRateHz == 44100);
    REQUIRE(audio.frameCount() == 3);
    CHECK(audio.left[0] == Catch::Approx(100.0f / 32768.0f));
    CHECK(audio.right[0] == Catch::Approx(-200.0f / 32768.0f));
    CHECK(audio.left[1] == Catch::Approx(300.0f / 32768.0f));
    CHECK(audio.right[1] == Catch::Approx(-400.0f / 32768.0f));
    CHECK(audio.left[2] == Catch::Approx(32767.0f / 32768.0f));
    CHECK(audio.right[2] == Catch::Approx(-32768.0f / 32768.0f));
}

TEST_CASE("Reading a mono WAV file duplicates it to both channels", "[wav_file]") {
    const std::vector<std::int16_t> samples = {1000, -1000, 2000};
    const auto bytes = buildPcm16Wav(22050, 1, samples);
    const auto path = writeTempFile(bytes, "sound-mind-test-mono.wav");

    const AudioBuffer audio = readWavFile(path);
    std::filesystem::remove(path);

    REQUIRE(audio.sampleRateHz == 22050);
    REQUIRE(audio.frameCount() == 3);
    for (std::size_t i = 0; i < audio.frameCount(); ++i) {
        CHECK(audio.left[i] == audio.right[i]);
    }
}

TEST_CASE("Reading a file that isn't a WAV file throws", "[wav_file]") {
    std::vector<char> junk = {'n', 'o', 't', ' ', 'a', ' ', 'w', 'a', 'v'};
    const auto path = writeTempFile(junk, "sound-mind-test-not-a-wav.wav");

    CHECK_THROWS_AS(readWavFile(path), std::invalid_argument);
    std::filesystem::remove(path);
}
