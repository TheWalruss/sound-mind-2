#include "ffmpeg_audio_encoder.h"

#include <algorithm>
#include <cstdint>

namespace sound_mind::codec::detail {

AudioEncoder createAudioEncoder(AVFormatContext& formatCtx, AVCodecID codecId, const AudioBuffer& audio,
                                 std::int64_t bitRate) {
    const AVCodec* codec = avcodec_find_encoder(codecId);
    if (codec == nullptr) {
        throw std::runtime_error("ffmpeg has no encoder registered for this codec");
    }

    AudioEncoder encoder;
    encoder.codecCtx.reset(avcodec_alloc_context3(codec));
    if (!encoder.codecCtx) {
        throw std::runtime_error("could not allocate an audio AVCodecContext");
    }
    AVCodecContext& ctx = *encoder.codecCtx;

    // Sample format: no meaningful preference of our own - take the
    // encoder's first supported one and resample/reformat to it in
    // encodeAudioTrack().
    const AVSampleFormat* sampleFmts = nullptr;
    int numSampleFmts = 0;
    checkFfmpeg(avcodec_get_supported_config(&ctx, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                              reinterpret_cast<const void**>(&sampleFmts), &numSampleFmts),
                "could not query the encoder's supported sample formats");
    ctx.sample_fmt = (sampleFmts != nullptr && numSampleFmts > 0) ? sampleFmts[0] : AV_SAMPLE_FMT_FLTP;

    // Sample rate: keep AudioBuffer's own rate if the encoder allows it
    // (avoids an unnecessary resample), else fall back to its first
    // supported rate.
    const int* sampleRates = nullptr;
    int numSampleRates = 0;
    checkFfmpeg(avcodec_get_supported_config(&ctx, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0,
                                              reinterpret_cast<const void**>(&sampleRates), &numSampleRates),
                "could not query the encoder's supported sample rates");
    ctx.sample_rate = static_cast<int>(audio.sampleRateHz);
    if (sampleRates != nullptr && numSampleRates > 0) {
        const bool sourceRateSupported =
            std::find(sampleRates, sampleRates + numSampleRates, ctx.sample_rate) != sampleRates + numSampleRates;
        if (!sourceRateSupported) {
            ctx.sample_rate = sampleRates[0];
        }
    }

    ctx.ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    ctx.bit_rate = bitRate;
    ctx.time_base = AVRational{1, ctx.sample_rate};
    if ((formatCtx.oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        ctx.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    checkFfmpeg(avcodec_open2(&ctx, codec, nullptr), "could not open the audio encoder");

    encoder.stream = avformat_new_stream(&formatCtx, nullptr);
    if (encoder.stream == nullptr) {
        throw std::runtime_error("could not create an audio stream");
    }
    checkFfmpeg(avcodec_parameters_from_context(encoder.stream->codecpar, &ctx),
                "could not copy audio codec parameters to the stream");
    encoder.stream->time_base = ctx.time_base;

    return encoder;
}

namespace {

/// @brief RAII owner for the data-pointer-array + sample buffer that
/// av_samples_alloc_array_and_samples() allocates together - freed per its
/// documented pattern (av_freep(&array[0]) for the buffer, then
/// av_freep(&array) for the array itself).
class PlanarSampleBuffer {
public:
    PlanarSampleBuffer(int numChannels, int nbSamples, AVSampleFormat format) {
        checkFfmpeg(av_samples_alloc_array_and_samples(&data_, &linesize_, numChannels, nbSamples, format, 0),
                    "could not allocate a resample buffer");
    }
    ~PlanarSampleBuffer() {
        if (data_ != nullptr) {
            av_freep(&data_[0]);
            av_freep(&data_);
        }
    }
    PlanarSampleBuffer(const PlanarSampleBuffer&) = delete;
    PlanarSampleBuffer& operator=(const PlanarSampleBuffer&) = delete;

    [[nodiscard]] std::uint8_t** data() const { return data_; }

private:
    std::uint8_t** data_ = nullptr;
    int linesize_ = 0;
};

/// @brief Allocates a new AVFrame configured to match `ctx`'s audio format,
/// holding `nbSamples` samples of freshly allocated buffer space.
AVFramePtr allocAudioFrame(const AVCodecContext& ctx, int nbSamples) {
    AVFramePtr frame(av_frame_alloc());
    if (!frame) {
        throw std::runtime_error("could not allocate an AVFrame");
    }
    frame->format = ctx.sample_fmt;
    frame->sample_rate = ctx.sample_rate;
    checkFfmpeg(av_channel_layout_copy(&frame->ch_layout, &ctx.ch_layout), "could not set the frame's channel layout");
    frame->nb_samples = nbSamples;
    checkFfmpeg(av_frame_get_buffer(frame.get(), 0), "could not allocate an audio frame buffer");
    return frame;
}

}  // namespace

void encodeAudioTrack(AVFormatContext& formatCtx, AudioEncoder& encoder, const AudioBuffer& audio) {
    AVCodecContext& ctx = *encoder.codecCtx;
    AVStream& stream = *encoder.stream;

    AVChannelLayout inLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* swrRaw = nullptr;
    checkFfmpeg(swr_alloc_set_opts2(&swrRaw, &ctx.ch_layout, ctx.sample_fmt, ctx.sample_rate, &inLayout,
                                     AV_SAMPLE_FMT_FLTP, static_cast<int>(audio.sampleRateHz), 0, nullptr),
                "could not configure the resampler");
    SwrContextPtr swr(swrRaw);
    checkFfmpeg(swr_init(swr.get()), "could not initialize the resampler");

    // Fixed-frame-size codecs (mp3, aac) report their required frame_size
    // only after avcodec_open2(); a frame_size of 0 means "any size is
    // fine" (e.g. PCM-like codecs) - 1152 (mp3's own frame size) is just a
    // reasonable chunk to use in that case.
    const int frameSize = ctx.frame_size > 0 ? ctx.frame_size : 1152;
    const int numChannels = ctx.ch_layout.nb_channels;

    AVAudioFifoPtr fifo(av_audio_fifo_alloc(ctx.sample_fmt, numChannels, frameSize));
    if (!fifo) {
        throw std::runtime_error("could not allocate the audio FIFO");
    }

    std::int64_t samplesEncoded = 0;
    auto drainFullFrames = [&]() {
        while (av_audio_fifo_size(fifo.get()) >= frameSize) {
            AVFramePtr frame = allocAudioFrame(ctx, frameSize);
            checkFfmpeg(av_audio_fifo_read(fifo.get(), reinterpret_cast<void* const*>(frame->data), frameSize),
                        "could not read from the audio FIFO");
            frame->pts = samplesEncoded;
            samplesEncoded += frameSize;
            encodeFrameAndWritePackets(formatCtx, ctx, stream, frame.get());
        }
    };

    const auto totalInputFrames = static_cast<int>(audio.frameCount());
    constexpr int kInputChunk = 4096;

    for (int inputPos = 0; inputPos < totalInputFrames; inputPos += kInputChunk) {
        const int chunkFrames = std::min(kInputChunk, totalInputFrames - inputPos);
        const std::uint8_t* inData[2] = {
            reinterpret_cast<const std::uint8_t*>(audio.left.data() + inputPos),
            reinterpret_cast<const std::uint8_t*>(audio.right.data() + inputPos),
        };

        const int maxOutSamples = swr_get_out_samples(swr.get(), chunkFrames);
        checkFfmpeg(maxOutSamples, "could not estimate the resampler's output size");
        if (maxOutSamples > 0) {
            PlanarSampleBuffer outBuffer(numChannels, maxOutSamples, ctx.sample_fmt);
            const int converted = swr_convert(swr.get(), outBuffer.data(), maxOutSamples, inData, chunkFrames);
            checkFfmpeg(converted, "resampling failed");
            if (converted > 0) {
                checkFfmpeg(av_audio_fifo_write(fifo.get(), reinterpret_cast<void* const*>(outBuffer.data()), converted),
                            "could not write to the audio FIFO");
            }
        }

        drainFullFrames();
    }

    // Flush the resampler's own internal buffering (it may hold back a few
    // samples for filter delay compensation).
    const int flushSamples = swr_get_out_samples(swr.get(), 0);
    if (flushSamples > 0) {
        PlanarSampleBuffer outBuffer(numChannels, flushSamples, ctx.sample_fmt);
        const int converted = swr_convert(swr.get(), outBuffer.data(), flushSamples, nullptr, 0);
        checkFfmpeg(converted, "resampler flush failed");
        if (converted > 0) {
            checkFfmpeg(av_audio_fifo_write(fifo.get(), reinterpret_cast<void* const*>(outBuffer.data()), converted),
                        "could not write to the audio FIFO");
        }
    }
    drainFullFrames();

    // One final, shorter-than-frameSize frame for whatever's left - both
    // mp3 and aac accept a short last frame.
    const int remaining = av_audio_fifo_size(fifo.get());
    if (remaining > 0) {
        AVFramePtr frame = allocAudioFrame(ctx, remaining);
        checkFfmpeg(av_audio_fifo_read(fifo.get(), reinterpret_cast<void* const*>(frame->data), remaining),
                    "could not read from the audio FIFO");
        frame->pts = samplesEncoded;
        samplesEncoded += remaining;
        encodeFrameAndWritePackets(formatCtx, ctx, stream, frame.get());
    }

    // Flush the encoder itself.
    encodeFrameAndWritePackets(formatCtx, ctx, stream, nullptr);
}

}  // namespace sound_mind::codec::detail
