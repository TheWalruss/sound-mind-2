#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

#include "sound_mind/codec/audio_export.h"
#include "sound_mind/codec/audio_file.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::CompressedAudioFormat;
using sound_mind::codec::exportCompressedAudio;
using sound_mind::codec::readAudioFile;

namespace {

AudioBuffer makeSineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto sampleCount = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    audio.left.resize(sampleCount);
    audio.right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample =
            std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
        audio.left[i] = sample;
        audio.right[i] = sample;
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

/// @brief The best correlation() over small sample-offsets - see
/// test_audio_export.cpp's own identical helper for why MP3's own encoder
/// delay needs this rather than a plain index-aligned correlation().
float bestLagCorrelation(const std::vector<float>& a, const std::vector<float>& b, std::size_t maxLagSamples) {
    float best = correlation(a, b);
    for (std::size_t lag = 1; lag <= maxLagSamples; ++lag) {
        const std::vector<float> bShiftedRight(b.begin() + static_cast<std::ptrdiff_t>(lag), b.end());
        best = std::max(best, correlation(a, bShiftedRight));
        const std::vector<float> aShiftedRight(a.begin() + static_cast<std::ptrdiff_t>(lag), a.end());
        best = std::max(best, correlation(aShiftedRight, b));
    }
    return best;
}

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

/// @brief Writes a minimal, valid 16-bit PCM mono WAV file to `path` - just
/// enough to confirm readAudioFile() actually dispatches a `.wav` path to
/// readWavFile() rather than ffmpeg; WAV parsing correctness itself is
/// test_wav_file.cpp's own job, not re-tested here.
void writeMinimalWavFile(const std::filesystem::path& path) {
    const std::vector<std::int16_t> samples = {1000, -1000, 2000, -2000};
    const std::uint32_t dataSize = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));

    std::vector<char> bytes;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    appendUint32(bytes, 36 + dataSize);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});
    bytes.insert(bytes.end(), {'f', 'm', 't', ' '});
    appendUint32(bytes, 16);
    appendUint16(bytes, 1);  // PCM
    appendUint16(bytes, 1);  // mono
    appendUint32(bytes, 44100);
    appendUint32(bytes, 44100 * 2);
    appendUint16(bytes, 2);
    appendUint16(bytes, 16);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendUint32(bytes, dataSize);
    for (const std::int16_t sample : samples) {
        appendUint16(bytes, static_cast<std::uint16_t>(sample));
    }

    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

TEST_CASE("readAudioFile dispatches a .wav path to readWavFile", "[audio_file]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-read-audio.wav";
    writeMinimalWavFile(path);

    const AudioBuffer audio = readAudioFile(path);
    std::filesystem::remove(path);

    REQUIRE(audio.frameCount() == 4);
    REQUIRE(audio.sampleRateHz == 44100);
}

TEST_CASE("readAudioFile dispatches a .WAV path (any case) to readWavFile", "[audio_file]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-read-audio-upper.WAV";
    writeMinimalWavFile(path);

    const AudioBuffer audio = readAudioFile(path);
    std::filesystem::remove(path);

    REQUIRE(audio.frameCount() == 4);
}

TEST_CASE("readAudioFile decodes an MP3 file via ffmpeg", "[audio_file]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-read-audio.mp3";
    exportCompressedAudio(path, original, CompressedAudioFormat::Mp3);

    const AudioBuffer decoded = readAudioFile(path);
    std::filesystem::remove(path);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    REQUIRE(decoded.frameCount() > 0);
    // Same "search for the actual best alignment" reasoning
    // test_audio_export.cpp's own MP3 round-trip test uses - see
    // bestLagCorrelation()'s own docs.
    constexpr std::size_t kMaxExpectedEncoderDelaySamples = 2000;
    CHECK(bestLagCorrelation(decoded.left, original.left, kMaxExpectedEncoderDelaySamples) > 0.9f);
    CHECK(bestLagCorrelation(decoded.right, original.right, kMaxExpectedEncoderDelaySamples) > 0.9f);
}

TEST_CASE("readAudioFile decodes a FLAC file via ffmpeg losslessly", "[audio_file]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-read-audio.flac";
    exportCompressedAudio(path, original, CompressedAudioFormat::Flac);

    const AudioBuffer decoded = readAudioFile(path);
    std::filesystem::remove(path);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    CHECK(correlation(decoded.left, original.left) > 0.999f);
    CHECK(correlation(decoded.right, original.right) > 0.999f);
}

TEST_CASE("readAudioFile decodes an Ogg file via ffmpeg recognizably", "[audio_file]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-read-audio.ogg";
    exportCompressedAudio(path, original, CompressedAudioFormat::Ogg);

    const AudioBuffer decoded = readAudioFile(path);
    std::filesystem::remove(path);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    CHECK(correlation(decoded.left, original.left) > 0.9f);
    CHECK(correlation(decoded.right, original.right) > 0.9f);
}

TEST_CASE("readAudioFile throws for a nonexistent non-wav file", "[audio_file]") {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-does-not-exist.mp3";
    CHECK_THROWS_AS(readAudioFile(path), std::runtime_error);
}

TEST_CASE("readAudioFile throws for a file with no audio stream", "[audio_file]") {
    // A plain text file has no recognizable container/codec at all.
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-not-audio.mp3";
    std::ofstream stream(path);
    stream << "this is not an audio file";
    stream.close();

    CHECK_THROWS_AS(readAudioFile(path), std::runtime_error);
    std::filesystem::remove(path);
}
