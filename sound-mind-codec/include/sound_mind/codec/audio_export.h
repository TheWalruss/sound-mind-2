#pragma once

#include <filesystem>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec {

/**
 * @brief Compressed audio formats `exportCompressedAudio()` can write, per
 *        `docs/sound-mind-design.md`'s Export section ("at minimum MP3 and
 *        OGG/FLAC... from the start").
 */
enum class CompressedAudioFormat {
    Flac,  ///< Lossless. Written via JUCE's built-in FLAC codec.
    Ogg,   ///< Lossy (Vorbis). Written via JUCE's built-in Ogg Vorbis codec.
    Mp3,   ///< Lossy. Written via ffmpeg (libmp3lame) - JUCE can only *read* MP3, not write it.
           ///< Introduces a real encoder delay (libmp3lame's filter-bank
           ///< priming, typically hundreds to ~1000+ samples) before the
           ///< decoded audio lines back up with the source - an inherent
           ///< property of MP3 itself, not specific to this encoder path.
};

/**
 * @brief Exports audio to a compressed file.
 *
 * FLAC and Ogg go through JUCE's own bundled codecs (`juce_audio_formats`)
 * - no extra dependency beyond JUCE, already linked for `PlaybackEngine`.
 * MP3 goes through ffmpeg, since JUCE's own `MP3AudioFormat` writer is an
 * explicit stub (`jassertfalse; // not yet implemented!`, confirmed by
 * reading its actual source before choosing this path).
 *
 * @param path Destination path. Its extension is not inspected or
 *        enforced - callers are expected to use one matching `format`.
 * @param audio The audio to export.
 * @param format Which compressed format to write.
 * @throws std::runtime_error if the file can't be written, or the
 *         requested format's encoder couldn't be created/opened.
 */
void exportCompressedAudio(const std::filesystem::path& path, const AudioBuffer& audio, CompressedAudioFormat format);

}  // namespace sound_mind::codec
