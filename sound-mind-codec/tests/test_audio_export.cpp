#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "sound_mind/codec/audio_export.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::CompressedAudioFormat;
using sound_mind::codec::exportCompressedAudio;

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

/// @brief The best correlation() over small sample-offsets of `b` relative
/// to `a`, searching both directions up to `maxLagSamples`.
///
/// MP3 (unlike Flac/Ogg here, or PCM) has a real, standard encoder delay -
/// libmp3lame's filter bank and bit reservoir prime a few hundred to ~1000+
/// samples of latency before the encoded stream lines up with the source
/// again, and a naive decoder (this test's readBackViaJuce(), via Windows
/// Media Foundation - see its docs) doesn't necessarily strip that using
/// the LAME/Xing header's own encoder-delay metadata. A plain, index-aligned
/// correlation() is the right (and stricter) tool for the lossless/JUCE
/// formats, but not a meaningful test for MP3 specifically - this searches
/// for the actual best alignment instead, the standard way to verify a
/// lossy, delay-introducing codec's round trip.
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

/// @brief Reads an audio file back via JUCE's own registered formats - test
/// verification only, not part of the public API (real import stays
/// WAV-only via readWavFile(), per the confirmed Import & Display scope).
/// registerBasicFormats() alone is enough for MP3 here: it conditionally
/// registers WindowsMediaAudioFormat when JUCE_USE_WINDOWS_MEDIA_FORMAT is
/// set (the default on Windows), which reads MP3 via Windows Media
/// Foundation - JUCE's own separate MP3AudioFormat class only exists when
/// the JUCE_USE_MP3AUDIOFORMAT flag is explicitly enabled (off by default,
/// gated behind its own legal disclaimer), which this project doesn't set.
AudioBuffer readBackViaJuce(const std::filesystem::path& path) {
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const juce::File file(path.string());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) {
        throw std::runtime_error("could not read back exported file: " + path.string());
    }

    AudioBuffer audio;
    audio.sampleRateHz = static_cast<std::uint32_t>(reader->sampleRate);
    const auto numSamples = static_cast<int>(reader->lengthInSamples);
    audio.left.resize(static_cast<std::size_t>(numSamples));
    audio.right.resize(static_cast<std::size_t>(numSamples));

    juce::AudioBuffer<float> buffer(2, numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, reader->numChannels > 1);

    for (int i = 0; i < numSamples; ++i) {
        audio.left[static_cast<std::size_t>(i)] = buffer.getSample(0, i);
        audio.right[static_cast<std::size_t>(i)] = buffer.getSample(reader->numChannels > 1 ? 1 : 0, i);
    }
    return audio;
}

}  // namespace

TEST_CASE("exportCompressedAudio writes a FLAC file that reads back losslessly", "[audio_export]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.flac";

    exportCompressedAudio(path, original, CompressedAudioFormat::Flac);
    const AudioBuffer decoded = readBackViaJuce(path);
    std::filesystem::remove(path);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    CHECK(correlation(decoded.left, original.left) > 0.999f);
    CHECK(correlation(decoded.right, original.right) > 0.999f);
}

TEST_CASE("exportCompressedAudio writes an Ogg file that round-trips recognizably", "[audio_export]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.ogg";

    exportCompressedAudio(path, original, CompressedAudioFormat::Ogg);
    const AudioBuffer decoded = readBackViaJuce(path);
    std::filesystem::remove(path);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    CHECK(correlation(decoded.left, original.left) > 0.9f);
    CHECK(correlation(decoded.right, original.right) > 0.9f);
}

TEST_CASE("exportCompressedAudio writes an MP3 file that round-trips recognizably", "[audio_export]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.5f, 44100);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-export.mp3";

    exportCompressedAudio(path, original, CompressedAudioFormat::Mp3);
    const AudioBuffer decoded = readBackViaJuce(path);
    std::filesystem::remove(path);

    REQUIRE(decoded.sampleRateHz == original.sampleRateHz);
    // bestLagCorrelation(), not correlation() - see its own docs: MP3's
    // encoder delay means the decoded signal is genuinely offset from the
    // original by a real (if small) number of samples, not misencoded.
    constexpr std::size_t kMaxExpectedEncoderDelaySamples = 2000;
    CHECK(bestLagCorrelation(decoded.left, original.left, kMaxExpectedEncoderDelaySamples) > 0.9f);
    CHECK(bestLagCorrelation(decoded.right, original.right, kMaxExpectedEncoderDelaySamples) > 0.9f);
}

TEST_CASE("exportCompressedAudio throws for an unwritable path", "[audio_export]") {
    const AudioBuffer original = makeSineTone(1000.0f, 0.1f, 44100);
    const auto path = std::filesystem::path("Z:/does/not/exist/sound-mind-test-export.flac");

    CHECK_THROWS_AS(exportCompressedAudio(path, original, CompressedAudioFormat::Flac), std::runtime_error);
}
