#pragma once

#include <filesystem>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec {

/**
 * @brief Reads any supported audio file into an AudioBuffer -
 *        `docs/sound-mind-design.md`'s own Import section ("At minimum,
 *        format coverage should match the legacy version: WAV, MP3, FLAC,
 *        OGG, AIFF, M4A, and Opus"), `v0.0.42.3` (Workflow & Device Polish,
 *        Installment C).
 *
 * Dispatches by `path`'s own extension: a `.wav` file goes through
 * `readWavFile()`'s own hand-rolled, dependency-free parser (unchanged,
 * still the fast/direct path for the common case); every other extension
 * goes through ffmpeg's own generic demux/decode pipeline (see
 * `docs/sound-mind-architecture.md`'s Decision on this installment for why
 * ffmpeg, already a required dependency for MP3/MP4 export, was kept over
 * JUCE's own bundled MP3/Flac/Ogg readers - it alone covers the design
 * doc's own full format list, including M4A/Opus). ffmpeg's own format
 * detection is itself content-based, not purely extension-based, so a
 * misnamed non-WAV file (e.g. an MP3 saved with a `.mp4` extension) still
 * decodes correctly as long as its own extension isn't `.wav`.
 *
 * @param path Path to the audio file.
 * @return The decoded audio.
 * @throws std::ios_base::failure if a `.wav` file can't be read (see
 *         `readWavFile()`'s own docs).
 * @throws std::invalid_argument if a `.wav` file isn't actually a valid WAV
 *         file, or uses a format/bit depth/channel count `readWavFile()`
 *         doesn't support.
 * @throws std::runtime_error if a non-`.wav` file can't be opened, has no
 *         audio stream, or ffmpeg has no decoder for its own codec.
 */
[[nodiscard]] AudioBuffer readAudioFile(const std::filesystem::path& path);

}  // namespace sound_mind::codec
