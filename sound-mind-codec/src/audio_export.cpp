#include "sound_mind/codec/audio_export.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include <juce_audio_formats/juce_audio_formats.h>

#include "ffmpeg_audio_encoder.h"
#include "ffmpeg_raii.h"

namespace sound_mind::codec {

namespace {

/// @brief Writes `audio` through a JUCE AudioFormat's writer, in fixed-size
/// chunks so `shouldCancel` (see exportCompressedAudio()'s own docs) gets a
/// real chance to interrupt a long write - a single whole-buffer
/// `writeFromFloatArrays()` call had no such checkpoint at all. Used for
/// Flac and Ogg, both of which have real, working JUCE writers (unlike
/// MP3 - see audio_export.h's docs).
void writeViaJuceFormat(juce::AudioFormat& format, const std::filesystem::path& path, const AudioBuffer& audio,
                        const std::function<bool()>& shouldCancel) {
    juce::File file(path.string());
    file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr || stream->failedToOpen()) {
        throw std::runtime_error("could not open output file for writing: " + path.string());
    }

    const auto bitDepths = format.getPossibleBitDepths();
    const int bitsPerSample = bitDepths.contains(16) ? 16 : bitDepths[0];

    // A reasonable, format-agnostic middle-of-the-road default: for a
    // lossless format like Flac this only affects compression effort/file
    // size, not audio quality; for a lossy one like Ogg it lands roughly
    // mid-way between the smallest and highest-quality options offered.
    const auto qualityOptions = format.getQualityOptions();
    const int qualityIndex = qualityOptions.isEmpty() ? 0 : qualityOptions.size() / 2;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(stream.get(), audio.sampleRateHz, 2, bitsPerSample, {}, qualityIndex));
    if (writer == nullptr) {
        throw std::runtime_error("could not create an encoder for this format: " + path.string());
    }
    stream.release();  // the writer now owns it.

    // Same chunk size as encodeAudioTrack()'s own kInputChunk - no
    // particular need to match, just consistent with the other export
    // path's own choice of "small enough for a responsive cancel, large
    // enough not to churn on per-call overhead."
    constexpr int kChunkFrames = 4096;
    const auto totalFrames = static_cast<int>(audio.frameCount());
    for (int pos = 0; pos < totalFrames; pos += kChunkFrames) {
        if (shouldCancel && shouldCancel()) {
            throw ExportCancelled{};
        }
        const int chunkFrames = std::min(kChunkFrames, totalFrames - pos);
        const float* channels[] = {audio.left.data() + pos, audio.right.data() + pos};
        if (!writer->writeFromFloatArrays(channels, 2, chunkFrames)) {
            throw std::runtime_error("failed writing audio samples: " + path.string());
        }
    }
}

/// @brief Writes `audio` as MP3, via ffmpeg (libmp3lame) - see
/// audio_export.h's docs for why (JUCE's own MP3 writer is an
/// unimplemented stub). 192kbps: a reasonable, unconfigurable-for-now
/// default bitrate for lossy export, matching the "middle of the road"
/// choice writeViaJuceFormat() makes for Flac/Ogg's own quality options.
void writeMp3ViaFfmpeg(const std::filesystem::path& path, const AudioBuffer& audio,
                       const std::function<bool()>& shouldCancel) {
    using namespace detail;  // NOLINT(google-build-using-namespace) - this file's own ffmpeg detail helpers.

    AVFormatContext* formatCtxRaw = nullptr;
    checkFfmpeg(avformat_alloc_output_context2(&formatCtxRaw, nullptr, nullptr, path.string().c_str()),
                "could not determine an output format for this path");
    AVFormatContextOutputPtr formatCtx(formatCtxRaw);

    AudioEncoder encoder = createAudioEncoder(*formatCtx, AV_CODEC_ID_MP3, audio, /*bitRate=*/192000);

    checkFfmpeg(avio_open(&formatCtx->pb, path.string().c_str(), AVIO_FLAG_WRITE),
                "could not open output file for writing: " + path.string());
    checkFfmpeg(avformat_write_header(formatCtx.get(), nullptr), "could not write the MP3 file header");

    encodeAudioTrack(*formatCtx, encoder, audio, shouldCancel);

    checkFfmpeg(av_write_trailer(formatCtx.get()), "could not finalize the MP3 file");
}

}  // namespace

void exportCompressedAudio(const std::filesystem::path& path, const AudioBuffer& audio, CompressedAudioFormat format,
                            const std::function<bool()>& shouldCancel) {
    switch (format) {
        case CompressedAudioFormat::Flac: {
            juce::FlacAudioFormat flac;
            writeViaJuceFormat(flac, path, audio, shouldCancel);
            return;
        }
        case CompressedAudioFormat::Ogg: {
            juce::OggVorbisAudioFormat ogg;
            writeViaJuceFormat(ogg, path, audio, shouldCancel);
            return;
        }
        case CompressedAudioFormat::Mp3:
            writeMp3ViaFfmpeg(path, audio, shouldCancel);
            return;
    }
}

}  // namespace sound_mind::codec
