#include "sound_mind/codec/stream_incremental_encoder.h"

#include "stream_frame_codec.h"

namespace sound_mind::codec {

using namespace detail;  // NOLINT(google-build-using-namespace) - this file's own shared frame-codec helpers.

StreamIncrementalEncoder::StreamIncrementalEncoder(StreamCodecConfig config)
    : config_(config),
      fftSize_(fftSizeFor(config.hopLength)),
      window_(hannWindow(fftSize_)),
      linearBinIndex_(logBinToLinearBinIndex(config, fftSize_)),
      // Pre-sized to fftSize_ once here, matching encode()'s own pattern -
      // extractFrame() fills exactly `frame.size()` samples, it doesn't
      // resize `frame` itself.
      frameLeftScratch_(fftSize_),
      frameRightScratch_(fftSize_),
      frameMidScratch_(fftSize_) {}

void StreamIncrementalEncoder::encodeFrameAt(std::ptrdiff_t startSample) {
    extractFrame(left_, startSample, frameLeftScratch_);
    extractFrame(right_, startSample, frameRightScratch_);
    extractFrame(mid_, startSample, frameMidScratch_);

    for (std::uint32_t i = 0; i < fftSize_; ++i) {
        frameLeftScratch_[i] *= window_[i];
        frameRightScratch_[i] *= window_[i];
        frameMidScratch_[i] *= window_[i];
    }

    forwardRealFft(frameLeftScratch_, spectrumLeftScratch_);
    forwardRealFft(frameRightScratch_, spectrumRightScratch_);
    forwardRealFft(frameMidScratch_, spectrumMidScratch_);

    std::vector<float> leftDb(config_.binCount);
    std::vector<float> rightDb(config_.binCount);
    std::vector<float> phase(config_.binCount);
    for (std::uint32_t bin = 0; bin < config_.binCount; ++bin) {
        const float index = linearBinIndex_[bin];
        const SpectrumSample left = sampleSpectrum(spectrumLeftScratch_, index);
        const SpectrumSample right = sampleSpectrum(spectrumRightScratch_, index);
        const SpectrumSample mid = sampleSpectrum(spectrumMidScratch_, index);
        leftDb[bin] = amplitudeToDb(left.magnitude);
        rightDb[bin] = amplitudeToDb(right.magnitude);
        phase[bin] = std::atan2(mid.sinPhase, mid.cosPhase);
    }

    framesLeftDb_.push_back(std::move(leftDb));
    framesRightDb_.push_back(std::move(rightDb));
    framesPhase_.push_back(std::move(phase));
}

void StreamIncrementalEncoder::pushSamples(const float* left, const float* right, std::size_t numSamples) {
    std::lock_guard<std::mutex> lock(mutex_);

    left_.insert(left_.end(), left, left + numSamples);
    right_.insert(right_.end(), right, right + numSamples);
    mid_.reserve(mid_.size() + numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
        mid_.push_back(0.5f * (left[i] + right[i]));
    }

    // Encode every frame that now has a complete analysis window available
    // - frame N needs samples [N*hopLength, N*hopLength + fftSize_).
    for (;;) {
        const auto nextFrame = framesLeftDb_.size();
        const auto start = nextFrame * config_.hopLength;
        if (start + fftSize_ > left_.size()) {
            break;
        }
        encodeFrameAt(static_cast<std::ptrdiff_t>(start));
    }
}

std::size_t StreamIncrementalEncoder::sampleCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return left_.size();
}

std::uint32_t StreamIncrementalEncoder::frameCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<std::uint32_t>(framesLeftDb_.size());
}

StreamImage StreamIncrementalEncoder::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);

    StreamImage image;
    image.config = config_;
    const auto frameCount = static_cast<std::uint32_t>(framesLeftDb_.size());
    image.frameCount = frameCount;
    image.sampleCount = left_.size();
    image.leftMagnitudeDb.resize(std::size_t{config_.binCount} * frameCount);
    image.rightMagnitudeDb.resize(std::size_t{config_.binCount} * frameCount);
    image.sharedPhaseRadians.resize(std::size_t{config_.binCount} * frameCount);

    for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
        for (std::uint32_t bin = 0; bin < config_.binCount; ++bin) {
            const std::size_t cell = std::size_t{bin} * frameCount + frame;
            image.leftMagnitudeDb[cell] = framesLeftDb_[frame][bin];
            image.rightMagnitudeDb[cell] = framesRightDb_[frame][bin];
            image.sharedPhaseRadians[cell] = framesPhase_[frame][bin];
        }
    }

    return image;
}

}  // namespace sound_mind::codec
