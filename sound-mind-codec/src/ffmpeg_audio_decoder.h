#pragma once

// Private (non-installed) helper behind readAudioFile()'s own non-WAV path -
// see audio_file.h's own docs. Decodes any audio file ffmpeg's own installed
// build recognizes (MP3 in particular, plus FLAC/Ogg/AIFF/M4A/Opus/etc. - see
// docs/sound-mind-design.md's own "at minimum" import format list) into this
// codebase's own AudioBuffer convention (stereo, planar float).

#include <filesystem>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec::detail {

/**
 * @brief Decodes an audio file via ffmpeg's own generic demux/decode
 * pipeline (avformat_open_input()/avcodec_*), resampled/reformatted to
 * stereo planar float via libswresample - the exact inverse of
 * ffmpeg_audio_encoder.h's own encodeAudioTrack(), which goes the other
 * direction for export.
 *
 * The file's own first audio stream is used (`av_find_best_stream()`'s own
 * "which stream ffmpeg itself judges most likely the intended one"
 * selection - the only sensible choice for a plain audio file, which
 * normally has exactly one). The result keeps the source's own sample
 * rate (no rate conversion - only sample format/channel layout are
 * normalized to this codebase's own convention), matching readWavFile()'s
 * own precedent of preserving whatever rate a file was actually encoded
 * at. A mono source is upmixed to stereo (both channels identical) by
 * libswresample's own standard mono-to-stereo behavior, the same outcome
 * readWavFile()'s own explicit "duplicated to both channels" handling
 * produces for a mono WAV.
 *
 * @param path Path to the audio file.
 * @return The decoded audio.
 * @throws std::runtime_error if the file can't be opened, has no audio
 *         stream, or ffmpeg has no decoder for its own codec, or any
 *         decode/resample step fails.
 */
[[nodiscard]] AudioBuffer decodeAudioFileViaFFmpeg(const std::filesystem::path& path);

}  // namespace sound_mind::codec::detail
