#pragma once

// Private (non-installed) helper shared by audio_export.cpp's MP3 path and
// video_export.cpp: RAII wrappers around ffmpeg's C API handles (every
// ffmpeg *_alloc()/*_open() pairs with a matching *_free()/*_close() the
// caller must remember to call - std::unique_ptr with a small deleter makes
// that automatic, including on the throw paths checkFfmpeg() below takes).
//
// API surface confirmed against the actual installed ffmpeg 9.0.1 headers
// (vcpkg buildtrees/ffmpeg/src), not assumed from memory - notably,
// AVCodec's old direct sample_fmts/ch_layouts array fields are gone in this
// version; avcodec_get_supported_config() replaces them (see audio_export.cpp
// and video_export.cpp's use of it).

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <memory>
#include <stdexcept>
#include <string>

namespace sound_mind::codec::detail {

/// @brief Throws std::runtime_error with ffmpeg's own decoded message if
/// `ffmpegResult` is negative, per ffmpeg's "negative return = AVERROR"
/// convention used throughout its C API.
inline void checkFfmpeg(int ffmpegResult, const std::string& what) {
    if (ffmpegResult < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(ffmpegResult, buf, sizeof(buf));
        throw std::runtime_error(what + ": " + buf);
    }
}

/// @brief Closes and frees an output AVFormatContext, including its AVIO
/// file handle if one was opened (skipped for AVFMT_NOFILE-style muxers,
/// none of which this codebase uses, but checked for correctness anyway).
struct AVFormatContextOutputDeleter {
    /// @brief Closes `ctx`'s AVIO handle (if any) and frees `ctx` itself.
    /// @param ctx The context to close/free; a no-op if `nullptr`.
    void operator()(AVFormatContext* ctx) const {
        if (ctx == nullptr) {
            return;
        }
        if (ctx->pb != nullptr && (ctx->oformat == nullptr || (ctx->oformat->flags & AVFMT_NOFILE) == 0)) {
            avio_closep(&ctx->pb);
        }
        avformat_free_context(ctx);
    }
};
using AVFormatContextOutputPtr = std::unique_ptr<AVFormatContext, AVFormatContextOutputDeleter>;

/// @brief Frees an AVCodecContext (avcodec_free_context()).
struct AVCodecContextDeleter {
    /// @brief Frees `ctx` (a no-op if `nullptr`), per avcodec_free_context()'s own contract.
    /// @param ctx The context to free.
    void operator()(AVCodecContext* ctx) const { avcodec_free_context(&ctx); }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

/// @brief Frees an AVFrame (av_frame_free()).
struct AVFrameDeleter {
    /// @brief Frees `frame` (a no-op if `nullptr`), per av_frame_free()'s own contract.
    /// @param frame The frame to free.
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

/// @brief Frees an AVPacket (av_packet_free()).
struct AVPacketDeleter {
    /// @brief Frees `packet` (a no-op if `nullptr`), per av_packet_free()'s own contract.
    /// @param packet The packet to free.
    void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/// @brief Frees a resampler context (swr_free()).
struct SwrContextDeleter {
    /// @brief Frees `ctx` (a no-op if `nullptr`), per swr_free()'s own contract.
    /// @param ctx The resampler context to free.
    void operator()(SwrContext* ctx) const { swr_free(&ctx); }
};
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

/// @brief Frees a pixel-format converter context (sws_freeContext()).
struct SwsContextDeleter {
    /// @brief Frees `ctx` (a no-op if `nullptr`), per sws_freeContext()'s own contract.
    /// @param ctx The pixel-format converter context to free.
    void operator()(SwsContext* ctx) const { sws_freeContext(ctx); }
};
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

/// @brief Frees an audio sample FIFO (av_audio_fifo_free()).
struct AVAudioFifoDeleter {
    /// @brief Frees `fifo` (a no-op if `nullptr`), per av_audio_fifo_free()'s own contract.
    /// @param fifo The audio FIFO to free.
    void operator()(AVAudioFifo* fifo) const { av_audio_fifo_free(fifo); }
};
using AVAudioFifoPtr = std::unique_ptr<AVAudioFifo, AVAudioFifoDeleter>;

/// @brief Sends `frame` (nullptr to flush/drain the encoder at end-of-stream)
/// to `codecCtx`'s encoder and writes every packet it produces to `stream`
/// within `formatCtx`, rescaling each packet's timestamps from the codec's
/// internal time_base to the stream's (muxer) time_base. Identical for
/// audio and video encoding, so shared by both (see ffmpeg_audio_encoder.cpp
/// and video_export.cpp).
///
/// @note Not real-time-safe: this is offline export encoding only.
inline void encodeFrameAndWritePackets(AVFormatContext& formatCtx, AVCodecContext& codecCtx, AVStream& stream,
                                        AVFrame* frame) {
    checkFfmpeg(avcodec_send_frame(&codecCtx, frame), "failed sending frame to encoder");

    AVPacketPtr packet(av_packet_alloc());
    if (!packet) {
        throw std::runtime_error("could not allocate an AVPacket");
    }

    for (;;) {
        const int result = avcodec_receive_packet(&codecCtx, packet.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            break;
        }
        checkFfmpeg(result, "failed receiving packet from encoder");

        packet->stream_index = stream.index;
        av_packet_rescale_ts(packet.get(), codecCtx.time_base, stream.time_base);
        // av_interleaved_write_frame() always takes ownership of pkt's
        // reference (blanking it, even on error) - no separate unref needed.
        checkFfmpeg(av_interleaved_write_frame(&formatCtx, packet.get()), "failed writing packet");
    }
}

}  // namespace sound_mind::codec::detail
