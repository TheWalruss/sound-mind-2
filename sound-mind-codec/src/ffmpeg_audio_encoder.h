#pragma once

// Private (non-installed) helper shared by audio_export.cpp's MP3 path and
// video_export.cpp's AAC audio track - both need to encode an AudioBuffer
// into an already-open (but header-not-yet-written) ffmpeg output
// AVFormatContext. Split into two phases because muxing requires a stream's
// codec parameters to be finalized *before* avformat_write_header(), while
// the actual sample data can only be encoded *after* it.

#include "ffmpeg_raii.h"
#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec::detail {

/// @brief An opened audio encoder plus the muxer stream it's attached to.
/// `stream` is owned by the AVFormatContext it was created from, not by
/// this struct.
struct AudioEncoder {
    /// @brief The opened encoder itself.
    AVCodecContextPtr codecCtx;
    /// @brief The muxer stream `codecCtx` encodes into - owned by its AVFormatContext, not this struct.
    AVStream* stream = nullptr;
};

/// @brief Phase 1: creates and opens an encoder for `codecId`, adds a
/// matching stream to `formatCtx`, and fills in that stream's codec
/// parameters/time_base. Must be called *before* avformat_write_header().
///
/// The encoder's sample format is whatever it reports as its first
/// supported one via avcodec_get_supported_config() (ffmpeg 9's
/// replacement for the old direct AVCodec::sample_fmts array field - there
/// is no meaningful preference for us to express here, since
/// encodeAudioTrack() resamples/reformats to it regardless). The sample
/// rate keeps `audio`'s own rate when the encoder allows it (avoiding a
/// needless resample), else falls back to the encoder's first supported
/// rate. Channel layout is always stereo, matching AudioBuffer.
///
/// @throws std::runtime_error if ffmpeg has no encoder for `codecId`, or if
///         any allocation/query/open step fails.
AudioEncoder createAudioEncoder(AVFormatContext& formatCtx, AVCodecID codecId, const AudioBuffer& audio,
                                 std::int64_t bitRate);

/// @brief Phase 2: resamples/reformats `audio` (planar float, stereo) to
/// `encoder`'s chosen format via libswresample, chunks it to the encoder's
/// fixed frame_size through an AVAudioFifo, and encodes+writes every
/// resulting packet to `encoder.stream` within `formatCtx` - including a
/// final short frame for any samples left over, and a trailing flush of the
/// encoder itself. Must be called *after* avformat_write_header().
///
/// @throws std::runtime_error if resampling or encoding fails.
void encodeAudioTrack(AVFormatContext& formatCtx, AudioEncoder& encoder, const AudioBuffer& audio);

}  // namespace sound_mind::codec::detail
