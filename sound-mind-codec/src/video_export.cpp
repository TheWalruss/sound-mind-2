#include "sound_mind/codec/video_export.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "ffmpeg_audio_encoder.h"
#include "ffmpeg_raii.h"
#include "sound_mind/codec/video_frame_renderer.h"

namespace sound_mind::codec {

namespace {

using namespace detail;  // NOLINT(google-build-using-namespace) - this file's own ffmpeg detail helpers.

/// @brief An opened video encoder, its muxer stream (if any) and the pixel
/// format frames must be converted to.
struct VideoEncoder {
    AVCodecContextPtr codecCtx;
    AVStream* stream = nullptr;
    AVPixelFormat pixelFormat = AV_PIX_FMT_YUV420P;
};

/// @brief The FFmpeg encoder for `codec`, or `nullptr` if this build has
/// none. H.264/HEVC are Windows' Media Foundation encoders - the build is
/// LGPL, without x264/x265.
const AVCodec* findVideoEncoder(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264:
            return avcodec_find_encoder_by_name("h264_mf");
        case VideoCodec::Hevc:
            return avcodec_find_encoder_by_name("hevc_mf");
        case VideoCodec::ProRes:
            return avcodec_find_encoder_by_name("prores_ks");
        case VideoCodec::Mpeg4:
            return avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    }
    return nullptr;
}

/// @brief Data rate per pixel per frame for the Media Foundation encoders,
/// by quality - 1080p at 30 fps gives about 4, 8 and 16 Mbit/s for H.264.
double bitsPerPixel(VideoCodec codec, VideoQuality quality) {
    const double h264 = quality == VideoQuality::Low ? 0.065 : quality == VideoQuality::Medium ? 0.13 : 0.26;
    return codec == VideoCodec::Hevc ? h264 * 0.6 : h264;
}

/// @brief Creates and opens a video encoder; with a `formatCtx`, also adds
/// its stream. Throws if the encoder is missing or won't open (which is how
/// availableVideoCodecs() probes).
VideoEncoder createVideoEncoder(AVFormatContext* formatCtx, VideoCodec videoCodec, VideoQuality quality,
                                std::uint32_t width, std::uint32_t height, int frameRate) {
    const AVCodec* codec = findVideoEncoder(videoCodec);
    if (codec == nullptr) {
        throw std::runtime_error("this video format isn't available on this computer");
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
    if (videoCodec == VideoCodec::ProRes) {
        encoder.pixelFormat = AV_PIX_FMT_YUV422P10LE;  // ProRes 422.
    }

    ctx.width = static_cast<int>(width);
    ctx.height = static_cast<int>(height);
    ctx.pix_fmt = encoder.pixelFormat;
    ctx.time_base = AVRational{1, frameRate};
    ctx.framerate = AVRational{frameRate, 1};
    ctx.gop_size = frameRate * 2;
    ctx.max_b_frames = 0;  // encode order == display order; B-frames buy little for a mostly static picture.

    switch (videoCodec) {
        case VideoCodec::Mpeg4: {
            // Constant quality: quantizer 6/3/2 (lower is better).
            const int q = quality == VideoQuality::Low ? 6 : quality == VideoQuality::Medium ? 3 : 2;
            ctx.flags |= AV_CODEC_FLAG_QSCALE;
            ctx.global_quality = FF_QP2LAMBDA * q;
            break;
        }
        case VideoCodec::H264:
        case VideoCodec::Hevc:
            ctx.bit_rate = static_cast<std::int64_t>(bitsPerPixel(videoCodec, quality) * width * height * frameRate);
            break;
        case VideoCodec::ProRes:
            av_opt_set(ctx.priv_data, "profile",
                       quality == VideoQuality::Low ? "lt" : quality == VideoQuality::Medium ? "standard" : "hq", 0);
            break;
    }

    if (formatCtx != nullptr && (formatCtx->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        ctx.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    checkFfmpeg(avcodec_open2(&ctx, codec, nullptr), "could not open the video encoder");

    if (formatCtx != nullptr) {
        encoder.stream = avformat_new_stream(formatCtx, nullptr);
        if (encoder.stream == nullptr) {
            throw std::runtime_error("could not create a video stream");
        }
        checkFfmpeg(avcodec_parameters_from_context(encoder.stream->codecpar, &ctx),
                    "could not copy video codec parameters to the stream");
        if (videoCodec == VideoCodec::Hevc) {
            encoder.stream->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');  // what Apple players expect.
        }
        encoder.stream->time_base = ctx.time_base;
    }
    return encoder;
}

}  // namespace

const std::vector<VideoCodec>& availableVideoCodecs() {
    static std::once_flag once;
    static std::vector<VideoCodec> available;
    std::call_once(once, []() {
        // A thread of its own: Media Foundation initializes COM for itself,
        // which fails on a thread (like Qt's UI thread) already set up
        // differently.
        std::thread probe([]() {
            for (const VideoCodec codec : {VideoCodec::H264, VideoCodec::Hevc, VideoCodec::ProRes, VideoCodec::Mpeg4}) {
                try {
                    (void)createVideoEncoder(nullptr, codec, VideoQuality::Medium, 640, 360, 30);
                    available.push_back(codec);
                } catch (const std::exception&) {
                    // Not on this computer.
                }
            }
        });
        probe.join();
    });
    return available;
}

bool isVideoCodecAvailable(VideoCodec codec) {
    const auto& available = availableVideoCodecs();
    return std::find(available.begin(), available.end(), codec) != available.end();
}

std::string videoFileExtension(VideoCodec codec) { return codec == VideoCodec::ProRes ? "mov" : "mp4"; }

VideoCodec preferredVideoCodec() {
    return isVideoCodecAvailable(VideoCodec::H264) ? VideoCodec::H264 : VideoCodec::Mpeg4;
}

/// @brief Everything an open StreamedVideoWriter holds - the muxer, both
///        encoders, the frame renderer and the converters feeding them.
struct StreamedVideoWriter::State {
    /// @brief The muxer.
    AVFormatContextOutputPtr formatCtx;
    /// @brief The video encoder and its stream.
    VideoEncoder video;
    /// @brief The audio encoder and its stream.
    AudioEncoder audio;
    /// @brief Canvas plus playhead to finished frames.
    std::unique_ptr<VideoFrameRenderer> renderer;
    /// @brief RGB24 frames to the encoder's pixel format.
    SwsContextPtr sws;
    /// @brief Planar float stereo to the encoder's sample format and rate.
    SwrContextPtr swr;
    /// @brief Resampled audio waiting to fill whole encoder frames.
    AVAudioFifoPtr fifo;
    /// @brief Video frames per second.
    int frameRate = 30;
    /// @brief Samples per encoder frame.
    int audioFrameSize = 1024;
    /// @brief Video frames written so far.
    std::int64_t framesWritten = 0;
    /// @brief Audio samples encoded so far.
    std::int64_t samplesEncoded = 0;
    /// @brief Checked before every frame.
    std::function<bool()> shouldCancel;
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

StreamedVideoWriter::StreamedVideoWriter(const std::filesystem::path& path, std::uint32_t canvasWidth,
                                         std::uint32_t canvasHeight, double projectSeconds,
                                         std::uint32_t sampleRateHz, const VideoExportOptions& options)
    : state_(std::make_unique<State>()) {
    if (canvasWidth == 0 || canvasHeight == 0) {
        throw std::runtime_error("cannot export video for an empty canvas");
    }
    if (options.frameRate <= 0) {
        throw std::runtime_error("frameRate must be positive");
    }
    if (sampleRateHz == 0) {
        throw std::runtime_error("cannot export video without an audio sample rate");
    }
    State& st = *state_;
    st.frameRate = options.frameRate;
    st.renderer = std::make_unique<VideoFrameRenderer>(options, canvasWidth, canvasHeight, projectSeconds);
    const std::uint32_t width = st.renderer->width();
    const std::uint32_t height = st.renderer->height();

    const char* container = options.codec == VideoCodec::ProRes ? "mov" : "mp4";
    AVFormatContext* formatCtxRaw = nullptr;
    checkFfmpeg(avformat_alloc_output_context2(&formatCtxRaw, nullptr, container, path.string().c_str()),
                "could not determine an output format for this path");
    st.formatCtx.reset(formatCtxRaw);
    st.video = createVideoEncoder(st.formatCtx.get(), options.codec, options.quality, width, height,
                                  options.frameRate);
    AudioBuffer rateOnly;
    rateOnly.sampleRateHz = sampleRateHz;
    st.audio = createAudioEncoder(*st.formatCtx,
                                  options.codec == VideoCodec::ProRes ? AV_CODEC_ID_PCM_S16LE : AV_CODEC_ID_AAC,
                                  rateOnly, /*bitRate=*/256000);
    checkFfmpeg(avio_open(&st.formatCtx->pb, path.string().c_str(), AVIO_FLAG_WRITE),
                "could not open output file for writing: " + path.string());
    checkFfmpeg(avformat_write_header(st.formatCtx.get(), nullptr), "could not write the video file header");

    st.sws.reset(sws_getContext(static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_RGB24,
                                static_cast<int>(width), static_cast<int>(height), st.video.pixelFormat,
                                SWS_BICUBIC | SWS_ACCURATE_RND, nullptr, nullptr, nullptr));
    if (!st.sws) {
        throw std::runtime_error("could not create an RGB24 to video pixel format converter");
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

std::uint32_t StreamedVideoWriter::frameWidth() const noexcept { return state_->renderer->width(); }

std::uint32_t StreamedVideoWriter::frameHeight() const noexcept { return state_->renderer->height(); }

void StreamedVideoWriter::setShouldCancel(std::function<bool()> shouldCancel) {
    state_->shouldCancel = std::move(shouldCancel);
}

void StreamedVideoWriter::addVideo(const RgbImage& canvas, double seconds, const PlayheadAt& playheadAt) {
    State& st = *state_;
    if (st.finished || seconds <= 0.0) {
        return;
    }
    st.renderer->setCanvas(canvas);
    const double end = videoSeconds() + seconds;
    const std::uint32_t width = st.renderer->width();
    const std::uint32_t height = st.renderer->height();
    AVCodecContext& ctx = *st.video.codecCtx;
    while (static_cast<double>(st.framesWritten) / st.frameRate < end - 1e-9) {
        if (st.shouldCancel && st.shouldCancel()) {
            throw ExportCancelled{};
        }
        const double at = static_cast<double>(st.framesWritten) / st.frameRate;
        const RgbImage rgb = st.renderer->render(playheadAt ? playheadAt(at) : std::nullopt);

        AVFramePtr frame(av_frame_alloc());
        if (!frame) {
            throw std::runtime_error("could not allocate a video AVFrame");
        }
        frame->format = st.video.pixelFormat;
        frame->width = static_cast<int>(width);
        frame->height = static_cast<int>(height);
        checkFfmpeg(av_frame_get_buffer(frame.get(), 0), "could not allocate a video frame buffer");
        const std::uint8_t* srcSlices[1] = {rgb.pixels.data()};
        const int srcStride[1] = {static_cast<int>(width) * 3};
        checkFfmpeg(sws_scale(st.sws.get(), srcSlices, srcStride, 0, static_cast<int>(height), frame->data,
                              frame->linesize),
                    "RGB24 to video pixel format conversion failed");
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
    checkFfmpeg(av_write_trailer(st.formatCtx.get()), "could not finalize the video file");
}

void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                 const VideoExportOptions& options, const std::function<bool()>& shouldCancel,
                 const std::function<void(double)>& progress) {
    const double duration =
        audio.sampleRateHz > 0 ? static_cast<double>(audio.frameCount()) / audio.sampleRateHz : 0.0;
    StreamedVideoWriter writer(path, canvas.width, canvas.height, duration, audio.sampleRateHz, options);
    writer.setShouldCancel(shouldCancel);
    const auto playheadAt = [duration](double seconds) -> std::optional<double> {
        return duration > 0.0 ? std::optional(seconds / duration) : std::nullopt;
    };
    // A second at a time, video and audio together, so the muxer never has
    // to hold one stream's whole track waiting for the other.
    const std::size_t samplesPerSecond = audio.sampleRateHz;
    for (std::size_t first = 0; first < audio.frameCount(); first += samplesPerSecond) {
        const std::size_t count = std::min(samplesPerSecond, audio.frameCount() - first);
        writer.addVideo(canvas, static_cast<double>(first + count) / audio.sampleRateHz - writer.videoSeconds(),
                        playheadAt);
        AudioBuffer slice;
        slice.sampleRateHz = audio.sampleRateHz;
        slice.left.assign(audio.left.begin() + static_cast<std::ptrdiff_t>(first),
                          audio.left.begin() + static_cast<std::ptrdiff_t>(first + count));
        slice.right.assign(audio.right.begin() + static_cast<std::ptrdiff_t>(first),
                           audio.right.begin() + static_cast<std::ptrdiff_t>(first + count));
        writer.addAudio(slice);
        if (progress && duration > 0.0) {
            progress(std::min(1.0, writer.videoSeconds() / duration));
        }
    }
    writer.finish();
}

void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio, int frameRate,
                 const std::function<bool()>& shouldCancel) {
    VideoExportOptions options;
    options.frameRate = frameRate;
    exportVideo(path, canvas, audio, options, shouldCancel);
}

}  // namespace sound_mind::codec
