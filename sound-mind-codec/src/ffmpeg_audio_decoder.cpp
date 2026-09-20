#include "ffmpeg_audio_decoder.h"

#include "ffmpeg_raii.h"

namespace sound_mind::codec::detail {

namespace {

/// @brief Resamples/reformats one decoded frame (in the decoder's own
/// format/layout) to stereo planar float via `swr`, appending the result to
/// `result`. `frame == nullptr` flushes whatever the resampler is still
/// holding back internally (filter delay), matching the encode side's own
/// identical end-of-stream flush call in ffmpeg_audio_encoder.cpp.
void resampleAndAppend(SwrContext& swr, const AVFrame* frame, AudioBuffer& result) {
    const int inSamples = frame != nullptr ? frame->nb_samples : 0;
    const int maxOutSamples = swr_get_out_samples(&swr, inSamples);
    checkFfmpeg(maxOutSamples, "could not estimate the resampler's output size");
    if (maxOutSamples <= 0) {
        return;
    }

    PlanarSampleBuffer outBuffer(2, maxOutSamples, AV_SAMPLE_FMT_FLTP);
    const int converted = swr_convert(&swr, outBuffer.data(), maxOutSamples,
                                       frame != nullptr ? const_cast<const std::uint8_t**>(frame->data) : nullptr,
                                       inSamples);
    checkFfmpeg(converted, "resampling failed");
    if (converted <= 0) {
        return;
    }

    const auto* leftData = reinterpret_cast<const float*>(outBuffer.data()[0]);
    const auto* rightData = reinterpret_cast<const float*>(outBuffer.data()[1]);
    result.left.insert(result.left.end(), leftData, leftData + converted);
    result.right.insert(result.right.end(), rightData, rightData + converted);
}

/// @brief Drains every frame the decoder currently has buffered (after a
/// send_packet() call, including the final send_packet(nullptr) flush),
/// resampling and appending each to `result`.
void drainDecoder(AVCodecContext& codecCtx, AVFrame& frame, SwrContext& swr, AudioBuffer& result) {
    for (;;) {
        const int status = avcodec_receive_frame(&codecCtx, &frame);
        if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
            break;
        }
        checkFfmpeg(status, "failed receiving frame from decoder");
        resampleAndAppend(swr, &frame, result);
        av_frame_unref(&frame);
    }
}

}  // namespace

AudioBuffer decodeAudioFileViaFFmpeg(const std::filesystem::path& path) {
    AVFormatContext* formatCtxRaw = nullptr;
    checkFfmpeg(avformat_open_input(&formatCtxRaw, path.string().c_str(), nullptr, nullptr),
                "could not open audio file: " + path.string());
    AVFormatContextInputPtr formatCtx(formatCtxRaw);

    checkFfmpeg(avformat_find_stream_info(formatCtx.get(), nullptr), "could not read stream info: " + path.string());

    const AVCodec* decoder = nullptr;
    const int streamIndex = av_find_best_stream(formatCtx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    checkFfmpeg(streamIndex, "no audio stream found in: " + path.string());

    AVCodecContextPtr codecCtx(avcodec_alloc_context3(decoder));
    if (!codecCtx) {
        throw std::runtime_error("could not allocate an audio AVCodecContext");
    }
    checkFfmpeg(avcodec_parameters_to_context(codecCtx.get(), formatCtx->streams[streamIndex]->codecpar),
                "could not copy codec parameters: " + path.string());
    checkFfmpeg(avcodec_open2(codecCtx.get(), decoder, nullptr), "could not open the audio decoder: " + path.string());

    // Same sample rate in and out - only sample format/channel layout are
    // normalized to this codebase's own stereo-planar-float convention, per
    // this function's own docs on why the source's own rate is preserved.
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* swrRaw = nullptr;
    checkFfmpeg(swr_alloc_set_opts2(&swrRaw, &outLayout, AV_SAMPLE_FMT_FLTP, codecCtx->sample_rate,
                                     &codecCtx->ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate, 0, nullptr),
                "could not configure the resampler");
    SwrContextPtr swr(swrRaw);
    checkFfmpeg(swr_init(swr.get()), "could not initialize the resampler");

    AudioBuffer result;
    result.sampleRateHz = static_cast<std::uint32_t>(codecCtx->sample_rate);

    AVPacketPtr packet(av_packet_alloc());
    AVFramePtr frame(av_frame_alloc());
    if (!packet || !frame) {
        throw std::runtime_error("could not allocate an ffmpeg packet/frame");
    }

    while (av_read_frame(formatCtx.get(), packet.get()) >= 0) {
        if (packet->stream_index == streamIndex) {
            checkFfmpeg(avcodec_send_packet(codecCtx.get(), packet.get()), "failed sending packet to decoder");
            drainDecoder(*codecCtx, *frame, *swr, result);
        }
        av_packet_unref(packet.get());
    }

    // Flush the decoder (any frames it was still holding internally), then
    // the resampler (any samples it was still holding internally) -
    // mirroring encodeAudioTrack()'s own identical two-stage flush, in
    // reverse.
    checkFfmpeg(avcodec_send_packet(codecCtx.get(), nullptr), "failed flushing decoder: " + path.string());
    drainDecoder(*codecCtx, *frame, *swr, result);
    resampleAndAppend(*swr, nullptr, result);

    return result;
}

}  // namespace sound_mind::codec::detail
