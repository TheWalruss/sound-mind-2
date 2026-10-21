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
/// see exportVideo()'s docs) through `sws`, encoding+writing each one. The
/// canvas cuts to `segments[i + 1]`'s own once `playheadSeconds` reaches
/// its `startTimeSeconds` - see exportSegmentedVideo()'s own docs. A plain
/// `exportVideo()` call arrives here as a single-element segment list.
void encodeVideoTrack(AVFormatContext& formatCtx, VideoEncoder& encoder, const std::vector<VideoSegment>& segments,
                       std::uint32_t paddedWidth, std::uint32_t paddedHeight, double durationSeconds, int frameRate,
                       const std::function<bool()>& shouldCancel) {
    AVCodecContext& ctx = *encoder.codecCtx;
    AVStream& stream = *encoder.stream;

    SwsContextPtr sws(sws_getContext(static_cast<int>(paddedWidth), static_cast<int>(paddedHeight), AV_PIX_FMT_RGB24,
                                      static_cast<int>(paddedWidth), static_cast<int>(paddedHeight),
                                      encoder.pixelFormat, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!sws) {
        throw std::runtime_error("could not create an RGB24->YUV converter");
    }

    // Every segment's own base frame, padded once up front rather than
    // per-video-frame - the same "pad once, reuse" optimization the
    // original single-canvas version already made.
    std::vector<std::vector<std::uint8_t>> baseFrames;
    baseFrames.reserve(segments.size());
    for (const VideoSegment& segment : segments) {
        baseFrames.push_back(padCanvasToRgb24(segment.canvas, paddedWidth, paddedHeight));
    }

    const auto totalFrames = static_cast<std::int64_t>(std::ceil(durationSeconds * frameRate));
    std::size_t activeSegment = 0;

    for (std::int64_t frameIndex = 0; frameIndex < totalFrames; ++frameIndex) {
        if (shouldCancel && shouldCancel()) {
            throw ExportCancelled{};
        }

        const double playheadSeconds = static_cast<double>(frameIndex) / frameRate;

        // Forward-only scan (never resets to 0) - segments are ordered by
        // their own strictly-increasing startTimeSeconds, so this whole
        // pass stays O(totalFrames + segments.size()), not O(totalFrames *
        // segments.size()).
        while (activeSegment + 1 < segments.size() &&
               segments[activeSegment + 1].startTimeSeconds <= playheadSeconds) {
            ++activeSegment;
        }

        std::vector<std::uint8_t> frameRgb24 = baseFrames[activeSegment];

        // The playhead's own sweep is always relative to the *whole*
        // video's duration, never reset at a segment boundary - see
        // exportSegmentedVideo()'s/VideoSegment's own docs.
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

void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio, int frameRate,
                  const std::function<bool()>& shouldCancel) {
    exportSegmentedVideo(path, {VideoSegment{canvas, 0.0}}, audio, frameRate, shouldCancel);
}

void exportSegmentedVideo(const std::filesystem::path& path, const std::vector<VideoSegment>& segments,
                           const AudioBuffer& audio, int frameRate, const std::function<bool()>& shouldCancel) {
    if (segments.empty()) {
        throw std::runtime_error("cannot export video with no segments");
    }
    const RgbImage& firstCanvas = segments.front().canvas;
    if (firstCanvas.width == 0 || firstCanvas.height == 0) {
        throw std::runtime_error("cannot export video for an empty canvas");
    }
    for (const VideoSegment& segment : segments) {
        if (segment.canvas.width != firstCanvas.width || segment.canvas.height != firstCanvas.height) {
            throw std::runtime_error("every segment's own canvas must share the same dimensions");
        }
    }
    if (frameRate <= 0) {
        throw std::runtime_error("frameRate must be positive");
    }

    const std::uint32_t paddedWidth = roundUpToEven(firstCanvas.width);
    const std::uint32_t paddedHeight = roundUpToEven(firstCanvas.height);
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
    encodeVideoTrack(*formatCtx, videoEncoder, segments, paddedWidth, paddedHeight, durationSeconds, frameRate,
                      shouldCancel);
    encodeAudioTrack(*formatCtx, audioEncoder, audio, shouldCancel);

    checkFfmpeg(av_write_trailer(formatCtx.get()), "could not finalize the MP4 file");
}

/// @brief Everything an open StreamedVideoWriter holds - the muxer, both
///        encoders and the converters feeding them.
struct StreamedVideoWriter::State {
    /// @brief The MP4 muxer.
    AVFormatContextOutputPtr formatCtx;
    /// @brief The video encoder and its stream.
    VideoEncoder video;
    /// @brief The audio encoder and its stream.
    AudioEncoder audio;
    /// @brief RGB24 to the encoder's pixel format.
    SwsContextPtr sws;
    /// @brief Planar float stereo to the encoder's sample format and rate.
    SwrContextPtr swr;
    /// @brief Resampled audio waiting to fill whole encoder frames.
    AVAudioFifoPtr fifo;
    /// @brief The canvas size, padded to even.
    std::uint32_t paddedWidth = 0;
    /// @brief See paddedWidth.
    std::uint32_t paddedHeight = 0;
    /// @brief The input audio's sample rate.
    std::uint32_t sampleRateHz = 0;
    /// @brief Video frames per second.
    int frameRate = 30;
    /// @brief Samples per encoder frame.
    int audioFrameSize = 1024;
    /// @brief Video frames written so far.
    std::int64_t framesWritten = 0;
    /// @brief Audio samples encoded so far.
    std::int64_t samplesEncoded = 0;
    /// @brief Whether finish() ran.
    bool finished = false;

    /// @brief A fresh audio frame in the encoder's format.
    /// @param samples Its length.
    /// @return The frame.
    AVFramePtr audioFrame(int samples) const {
        const AVCodecContext& ctx = *audio.codecCtx;
        AVFramePtr frame(av_frame_alloc());
        if (!frame) {
            throw std::runtime_error("could not allocate an AVFrame");
        }
        frame->format = ctx.sample_fmt;
        frame->sample_rate = ctx.sample_rate;
        checkFfmpeg(av_channel_layout_copy(&frame->ch_layout, &ctx.ch_layout), "could not set the channel layout");
        frame->nb_samples = samples;
        checkFfmpeg(av_frame_get_buffer(frame.get(), 0), "could not allocate an audio frame buffer");
        return frame;
    }

    /// @brief Encodes whole frames from the FIFO.
    /// @param all Also encode the remainder, as a last, shorter frame.
    void drainAudio(bool all) {
        while (av_audio_fifo_size(fifo.get()) >= audioFrameSize || (all && av_audio_fifo_size(fifo.get()) > 0)) {
            const int samples = std::min(audioFrameSize, av_audio_fifo_size(fifo.get()));
            AVFramePtr frame = audioFrame(samples);
            checkFfmpeg(av_audio_fifo_read(fifo.get(), reinterpret_cast<void* const*>(frame->data), samples),
                        "could not read from the audio FIFO");
            frame->pts = samplesEncoded;
            samplesEncoded += samples;
            encodeFrameAndWritePackets(*formatCtx, *audio.codecCtx, *audio.stream, frame.get());
        }
    }

    /// @brief Resamples input into the FIFO.
    /// @param input Planar left/right float pointers, or `nullptr` to flush.
    /// @param frames How many input frames.
    void resample(const std::uint8_t** input, int frames) {
        const AVCodecContext& ctx = *audio.codecCtx;
        const int maxOut = swr_get_out_samples(swr.get(), frames);
        checkFfmpeg(maxOut, "could not estimate the resampler's output size");
        if (maxOut <= 0) {
            return;
        }
        PlanarSampleBuffer out(ctx.ch_layout.nb_channels, maxOut, ctx.sample_fmt);
        const int converted = swr_convert(swr.get(), out.data(), maxOut, input, frames);
        checkFfmpeg(converted, "resampling failed");
        if (converted > 0) {
            checkFfmpeg(av_audio_fifo_write(fifo.get(), reinterpret_cast<void* const*>(out.data()), converted),
                        "could not write to the audio FIFO");
        }
    }
};

StreamedVideoWriter::StreamedVideoWriter(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
                                         std::uint32_t sampleRateHz, int frameRate)
    : state_(std::make_unique<State>()) {
    if (width == 0 || height == 0 || frameRate <= 0 || sampleRateHz == 0) {
        throw std::runtime_error("cannot write a video with an empty canvas, frame rate or sample rate");
    }
    State& st = *state_;
    st.paddedWidth = roundUpToEven(width);
    st.paddedHeight = roundUpToEven(height);
    st.sampleRateHz = sampleRateHz;
    st.frameRate = frameRate;

    AVFormatContext* formatCtxRaw = nullptr;
    checkFfmpeg(avformat_alloc_output_context2(&formatCtxRaw, nullptr, "mp4", path.string().c_str()),
                "could not determine an output format for this path");
    st.formatCtx.reset(formatCtxRaw);
    st.video = createVideoEncoder(*st.formatCtx, st.paddedWidth, st.paddedHeight, frameRate);
    AudioBuffer rateOnly;
    rateOnly.sampleRateHz = sampleRateHz;
    st.audio = createAudioEncoder(*st.formatCtx, AV_CODEC_ID_AAC, rateOnly, /*bitRate=*/192000);
    checkFfmpeg(avio_open(&st.formatCtx->pb, path.string().c_str(), AVIO_FLAG_WRITE),
                "could not open output file for writing: " + path.string());
    checkFfmpeg(avformat_write_header(st.formatCtx.get(), nullptr), "could not write the MP4 file header");

    st.sws.reset(sws_getContext(static_cast<int>(st.paddedWidth), static_cast<int>(st.paddedHeight), AV_PIX_FMT_RGB24,
                                static_cast<int>(st.paddedWidth), static_cast<int>(st.paddedHeight),
                                st.video.pixelFormat, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!st.sws) {
        throw std::runtime_error("could not create an RGB24->YUV converter");
    }
    const AVCodecContext& audioCtx = *st.audio.codecCtx;
    AVChannelLayout inLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* swrRaw = nullptr;
    checkFfmpeg(swr_alloc_set_opts2(&swrRaw, &audioCtx.ch_layout, audioCtx.sample_fmt, audioCtx.sample_rate, &inLayout,
                                    AV_SAMPLE_FMT_FLTP, static_cast<int>(sampleRateHz), 0, nullptr),
                "could not configure the resampler");
    st.swr.reset(swrRaw);
    checkFfmpeg(swr_init(st.swr.get()), "could not initialize the resampler");
    st.audioFrameSize = audioCtx.frame_size > 0 ? audioCtx.frame_size : 1024;
    st.fifo.reset(av_audio_fifo_alloc(audioCtx.sample_fmt, audioCtx.ch_layout.nb_channels, st.audioFrameSize));
    if (!st.fifo) {
        throw std::runtime_error("could not allocate the audio FIFO");
    }
}

StreamedVideoWriter::~StreamedVideoWriter() {
    try {
        finish();
    } catch (...) {  // NOLINT(bugprone-empty-catch) - a destructor must not throw; the file is left incomplete.
    }
}

double StreamedVideoWriter::videoSeconds() const noexcept {
    return static_cast<double>(state_->framesWritten) / state_->frameRate;
}

void StreamedVideoWriter::addVideo(const RgbImage& canvas, double seconds, const PlayheadAt& playheadAt) {
    State& st = *state_;
    if (st.finished || seconds <= 0.0) {
        return;
    }
    const std::vector<std::uint8_t> base = padCanvasToRgb24(canvas, st.paddedWidth, st.paddedHeight);
    const double end = videoSeconds() + seconds;
    AVCodecContext& ctx = *st.video.codecCtx;
    while (static_cast<double>(st.framesWritten) / st.frameRate < end - 1e-9) {
        std::vector<std::uint8_t> frameRgb24 = base;
        if (playheadAt) {
            if (const auto fraction = playheadAt(static_cast<double>(st.framesWritten) / st.frameRate)) {
                const auto x = static_cast<std::uint32_t>(std::clamp(*fraction, 0.0, 1.0) * st.paddedWidth);
                drawPlayheadLine(frameRgb24, st.paddedWidth, st.paddedHeight, x);
            }
        }
        AVFramePtr frame(av_frame_alloc());
        if (!frame) {
            throw std::runtime_error("could not allocate a video AVFrame");
        }
        frame->format = st.video.pixelFormat;
        frame->width = static_cast<int>(st.paddedWidth);
        frame->height = static_cast<int>(st.paddedHeight);
        checkFfmpeg(av_frame_get_buffer(frame.get(), 0), "could not allocate a video frame buffer");
        const std::uint8_t* srcSlices[1] = {frameRgb24.data()};
        const int srcStride[1] = {static_cast<int>(st.paddedWidth) * 3};
        checkFfmpeg(sws_scale(st.sws.get(), srcSlices, srcStride, 0, static_cast<int>(st.paddedHeight), frame->data,
                              frame->linesize),
                    "RGB24->YUV conversion failed");
        frame->pts = st.framesWritten++;
        encodeFrameAndWritePackets(*st.formatCtx, ctx, *st.video.stream, frame.get());
    }
}

void StreamedVideoWriter::addAudio(const AudioBuffer& audio) {
    State& st = *state_;
    if (st.finished) {
        return;
    }
    constexpr std::size_t kChunk = 4096;
    for (std::size_t position = 0; position < audio.frameCount(); position += kChunk) {
        const auto frames = static_cast<int>(std::min(kChunk, audio.frameCount() - position));
        const std::uint8_t* input[2] = {reinterpret_cast<const std::uint8_t*>(audio.left.data() + position),
                                        reinterpret_cast<const std::uint8_t*>(audio.right.data() + position)};
        st.resample(input, frames);
        st.drainAudio(/*all=*/false);
    }
}

void StreamedVideoWriter::finish() {
    State& st = *state_;
    if (st.finished) {
        return;
    }
    st.finished = true;
    encodeFrameAndWritePackets(*st.formatCtx, *st.video.codecCtx, *st.video.stream, nullptr);
    st.resample(nullptr, 0);
    st.drainAudio(/*all=*/true);
    encodeFrameAndWritePackets(*st.formatCtx, *st.audio.codecCtx, *st.audio.stream, nullptr);
    checkFfmpeg(av_write_trailer(st.formatCtx.get()), "could not finalize the MP4 file");
}

}  // namespace sound_mind::codec
