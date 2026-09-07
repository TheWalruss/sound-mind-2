#pragma once

#include <filesystem>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec {

/**
 * @brief Reads a PCM WAV file into an AudioBuffer.
 *
 * Hand-rolled, minimal RIFF/WAVE parser - deliberately not a general audio
 * library integration. Per the confirmed scope for the Import & Display
 * milestone (`v0.0.3.1`): WAV is simple enough to parse directly with no
 * new dependency, and integrating JUCE for real (for its bundled AIFF/
 * FLAC/OGG/MP3 support) is deferred - partly because JUCE's own
 * distribution tier depends on Sound Mind's own license, which isn't
 * decided yet (see `docs/sound-mind-architecture.md`'s Decisions Needed).
 *
 * Supports 16-bit integer PCM and 32-bit IEEE float, mono or stereo. A
 * mono file is duplicated to both channels of the returned AudioBuffer,
 * matching how the Stream codec's own tests treat mono-in-stereo content.
 *
 * @param path Path to the WAV file.
 * @return The decoded audio.
 * @throws std::ios_base::failure if the file can't be read.
 * @throws std::invalid_argument if the file isn't a valid WAV file, or uses
 *         a format/bit depth/channel count this parser doesn't support.
 */
[[nodiscard]] AudioBuffer readWavFile(const std::filesystem::path& path);

}  // namespace sound_mind::codec
