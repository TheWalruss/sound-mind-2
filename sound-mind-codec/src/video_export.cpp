#include "sound_mind/codec/video_export.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "ffmpeg_audio_encoder.h"
#include "ffmpeg_raii.h"

namespace sound_mind::codec {

namespace {

using namespace detail;  // NOLINT(google-build-using-namespace) - this file's own ffmpeg detail helpers.

/// @brief Playhead line color (opaque white) - a simple, always-visible
/// choice regardless of the active color mapping/light-or-dark mode
/// baked into `canvas` (see exportVideo()'s docs: the canvas itself
/// already reflects whichever of those was active at export time).
constexpr std::uint8_t kPlayheadR = 255;
constexpr std::uint8_t kPlayheadG = 255;
constexpr std::uint8_t kPlayheadB = 255;

/// @brief Rounds `value` up to the nearest even number - AV_PIX_FMT_YUV420P
/// (mpeg4's pixel format) needs even width/height, since its chroma planes
/// are half-resolution.
std::uint32_t roundUpToEven(std::uint32_t value) { return (value % 2 == 0) ? value : value + 1; }

/// @brief Creates and opens an mpeg4 video encoder sized to `paddedWidth` x
/// `paddedHeight`, adds its stream to `formatCtx`, and returns the codec
/// context plus the pixel format it needs frames converted to. Must be
/// called before avformat_write_header(), matching createAudioEncoder()'s
/// two-phase split.
struct VideoEncoder {
    AVCodecContextPtr codecCtx;
    AVStream* stream = nullptr;
    AVPixelFormat pixelFormat = AV_PIX_FMT_YUV420P;
};

VideoEncoder createVideoEncoder(AVFormatContext& formatCtx, std::uint32_t paddedWidth, std::uint32_t paddedHeight,
                                 int frameRate) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (codec == nullptr) {
        throw std::runtime_error("ffmpeg has no encoder registered for mpeg4");
    }

    VideoEncoder encoder;
    encoder.codecCtx.reset(avcodec_alloc_context3(codec));
    if (!encoder.codecCtx) {
        throw std::runtime_error("could not allocate a video AVCodecContext");
    }
    AVCodecContext& ctx = *encoder.codecCtx;

    const AVPixelFormat* pixFmts = nullptr;
    int numPixFmts = 0;
    checkFfmpeg(avcodec_get_supported_config(&ctx, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0,
                                              reinterpret_cast<const void**>(&pixFmts), &numPixFmts),
                "could not query the encoder's supported pixel formats");
    encoder.pixelFormat = (pixFmts != nullptr && numPixFmts > 0) ? pixFmts[0] : AV_PIX_FMT_YUV420P;

    ctx.width = static_cast<int>(paddedWidth);
    ctx.height = static_cast<int>(paddedHeight);
    ctx.pix_fmt = encoder.pixelFormat;
    ctx.time_base = AVRational{1, frameRate};
    ctx.framerate = AVRational{frameRate, 1};
    ctx.gop_size = 12;      // a keyframe every half-second at 24-30fps: reasonable for a static-canvas-plus-line video.
    ctx.max_b_frames = 0;   // keeps encode order == display order - simpler, and B-frames buy little here.
    // A flat data-rate target scaled to resolution and frame rate - generous
    // enough to keep spectrogram detail legible without hand-tuning per
    // canvas size; not user-configurable yet (see exportVideo()'s docs).
    ctx.bit_rate = static_cast<std::int64_t>(paddedWidth) * paddedHeight * frameRate / 6;

    if ((formatCtx.oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        ctx.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    checkFfmpeg(avcodec_open2(&ctx, codec, nullptr), "could not open the video encoder");

    encoder.stream = avformat_new_stream(&formatCtx, nullptr);
    if (encoder.stream == nullptr) {
        throw std::runtime_error("could not create a video stream");
    }
    checkFfmpeg(avcodec_parameters_from_context(encoder.stream->codecpar, &ctx),
                "could not copy video codec parameters to the stream");
    encoder.stream->time_base = ctx.time_base;

    return encoder;
}

/// @brief `canvas`, right/bottom-padded with black pixels to `paddedWidth` x
/// `paddedHeight` if needed (see roundUpToEven()), as packed RGB24 rows.
std::vector<std::uint8_t> padCanvasToRgb24(const RgbImage& canvas, std::uint32_t paddedWidth,
                                            std::uint32_t paddedHeight) {
    std::vector<std::uint8_t> padded(std::size_t{paddedWidth} * paddedHeight * 3, 0);
    for (std::uint32_t y = 0; y < canvas.height; ++y) {
        const auto* srcRow = canvas.pixels.data() + std::size_t{y} * canvas.width * 3;
        auto* dstRow = padded.data() + std::size_t{y} * paddedWidth * 3;
        std::copy_n(srcRow, std::size_t{canvas.width} * 3, dstRow);
    }
    return padded;
}

/// @brief Draws the playhead: a 1px-wide vertical line at horizontal
/// position `x` over `frameRgb24` (packed RGB24, `width` x `height`).
void drawPlayheadLine(std::vector<std::uint8_t>& frameRgb24, std::uint32_t width, std::uint32_t height,
                       std::uint32_t x) {
    if (x >= width) {
        return;
    }
    for (std::uint32_t y = 0; y < height; ++y) {
        std::uint8_t* pixel = frameRgb24.data() + (std::size_t{y} * width + x) * 3;
        pixel[0] = kPlayheadR;
        pixel[1] = kPlayheadG;
        pixel[2] = kPlayheadB;
    }
}

/// @brief Renders every video frame (static canvas + moving playhead line,
/// see exportVideo()'s docs) through `sws`, encoding+writing each one.
void encodeVideoTrack(AVFormatContext& formatCtx, VideoEncoder& encoder, const RgbImage& canvas,
                       std::uint32_t paddedWidth, std::uint32_t paddedHeight, double durationSeconds,
                       int frameRate) {
    AVCodecContext& ctx = *encoder.codecCtx;
    AVStream& stream = *encoder.stream;

    SwsContextPtr sws(sws_getContext(static_cast<int>(paddedWidth), static_cast<int>(paddedHeight), AV_PIX_FMT_RGB24,
                                      static_cast<int>(paddedWidth), static_cast<int>(paddedHeight),
                                      encoder.pixelFormat, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!sws) {
        throw std::runtime_error("could not create an RGB24->YUV converter");
    }

    const std::vector<std::uint8_t> baseFrame = padCanvasToRgb24(canvas, paddedWidth, paddedHeight);
    const auto totalFrames = static_cast<std::int64_t>(std::ceil(durationSeconds * frameRate));

    for (std::int64_t frameIndex = 0; frameIndex < totalFrames; ++frameIndex) {
        std::vector<std::uint8_t> frameRgb24 = baseFrame;

        const double playheadSeconds = static_cast<double>(frameIndex) / frameRate;
        const double playheadFraction =
            durationSeconds > 0.0 ? std::clamp(playheadSeconds / durationSeconds, 0.0, 1.0) : 0.0;
        const auto playheadX = static_cast<std::uint32_t>(playheadFraction * paddedWidth);
        drawPlayheadLine(frameRgb24, paddedWidth, paddedHeight, playheadX);

        AVFramePtr frame(av_frame_alloc());
        if (!frame) {
            throw std::runtime_error("could not allocate a video AVFrame");
        }
        frame->format = encoder.pixelFormat;
        frame->width = static_cast<int>(paddedWidth);
        frame->height = static_cast<int>(paddedHeight);
        checkFfmpeg(av_frame_get_buffer(frame.get(), 0), "could not allocate a video frame buffer");

        const std::uint8_t* srcSlices[1] = {frameRgb24.data()};
        const int srcStride[1] = {static_cast<int>(paddedWidth) * 3};
        checkFfmpeg(sws_scale(sws.get(), srcSlices, srcStride, 0, static_cast<int>(paddedHeight), frame->data,
                               frame->linesize),
                    "RGB24->YUV conversion failed");

        frame->pts = frameIndex;
        encodeFrameAndWritePackets(formatCtx, ctx, stream, frame.get());
    }

    encodeFrameAndWritePackets(formatCtx, ctx, stream, nullptr);  // flush.
}

}  // namespace

void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                  int frameRate) {
    if (canvas.width == 0 || canvas.height == 0) {
        throw std::runtime_error("cannot export video for an empty canvas");
    }
    if (frameRate <= 0) {
        throw std::runtime_error("frameRate must be positive");
    }

    const std::uint32_t paddedWidth = roundUpToEven(canvas.width);
    const std::uint32_t paddedHeight = roundUpToEven(canvas.height);
    const double durationSeconds =
        audio.sampleRateHz > 0 ? static_cast<double>(audio.frameCount()) / audio.sampleRateHz : 0.0;

    AVFormatContext* formatCtxRaw = nullptr;
    checkFfmpeg(avformat_alloc_output_context2(&formatCtxRaw, nullptr, "mp4", path.string().c_str()),
                "could not determine an output format for this path");
    AVFormatContextOutputPtr formatCtx(formatCtxRaw);

    VideoEncoder videoEncoder = createVideoEncoder(*formatCtx, paddedWidth, paddedHeight, frameRate);
    AudioEncoder audioEncoder = createAudioEncoder(*formatCtx, AV_CODEC_ID_AAC, audio, /*bitRate=*/192000);

    checkFfmpeg(avio_open(&formatCtx->pb, path.string().c_str(), AVIO_FLAG_WRITE),
                "could not open output file for writing: " + path.string());
    checkFfmpeg(avformat_write_header(formatCtx.get(), nullptr), "could not write the MP4 file header");

    // Muxer order doesn't need to match temporal order - av_interleaved_write_frame()
    // (used by both encode*Track() helpers) buffers and reorders packets across
    // streams by dts as needed, so encoding the whole video track and then the
    // whole audio track is just as correct as interleaving them here, and simpler.
    encodeVideoTrack(*formatCtx, videoEncoder, canvas, paddedWidth, paddedHeight, durationSeconds, frameRate);
    encodeAudioTrack(*formatCtx, audioEncoder, audio);

    checkFfmpeg(av_write_trailer(formatCtx.get()), "could not finalize the MP4 file");
}

}  // namespace sound_mind::codec
