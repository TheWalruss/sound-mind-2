#include "sound_mind/codec/stream_codec.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

#include <pocketfft_hdronly.h>

#include "stream_frame_codec.h"

namespace sound_mind::codec {

using namespace detail;  // NOLINT(google-build-using-namespace) - this file's own shared frame-codec helpers.

namespace {

/// @brief Guards the overlap-add normalization divisor near a signal's
/// start/end, where fewer windows have accumulated, against dividing by
/// (near) zero.
constexpr float kMinWindowSumSquared = 1e-6f;

/// @brief Inverse real FFT: `n/2 + 1` complex bins -> `n` real samples.
void inverseRealFft(const std::vector<std::complex<float>>& bins, std::size_t n, std::vector<float>& frame) {
    frame.resize(n);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t strideIn{sizeof(std::complex<float>)};
    const pocketfft::stride_t strideOut{sizeof(float)};
    // PocketFFT does not normalize its inverse transform - folding the 1/n
    // scale factor in here is what makes a forward+inverse pair round-trip
    // to the original signal instead of an n-times-too-loud one.
    pocketfft::c2r(shape, strideIn, strideOut, std::size_t{0}, pocketfft::BACKWARD, bins.data(), frame.data(),
                   1.0f / static_cast<float>(n));
}

[[nodiscard]] float dbToAmplitude(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

/// @brief The inverse mapping of logBinToLinearBinIndex(): for each linear
/// FFT bin (0..fftSize/2), the fractional log-bin index it corresponds to.
[[nodiscard]] std::vector<float> linearBinToLogBinIndex(const StreamCodecConfig& config, std::uint32_t fftSize) {
    const std::size_t linearBinCount = fftSize / 2 + 1;
    std::vector<float> logIndex(linearBinCount);
    const float maxFrequencyHz = clampedMaxFrequencyHz(config);
    const float logRange = std::log(maxFrequencyHz / config.minFrequencyHz);
    const float hzPerLinearBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);
    for (std::size_t k = 0; k < linearBinCount; ++k) {
        const float frequencyHz = static_cast<float>(k) * hzPerLinearBin;
        const float clampedFrequencyHz = std::clamp(frequencyHz, config.minFrequencyHz, maxFrequencyHz);
        const float t = std::log(clampedFrequencyHz / config.minFrequencyHz) / logRange;
        logIndex[k] = t * static_cast<float>(config.binCount - 1);
    }
    return logIndex;
}

/// @brief Linearly interpolates a stored `[bin][frame]` plane's `frame`
/// column at the fractional bin index `logBinIndex`, clamped to range.
[[nodiscard]] float sampleStoredPlane(const std::vector<float>& plane, std::uint32_t binCount, std::uint32_t frameCount,
                                      std::uint32_t frame, float logBinIndex) {
    const float clamped = std::clamp(logBinIndex, 0.0f, static_cast<float>(binCount - 1));
    const auto lowBin = static_cast<std::uint32_t>(clamped);
    const std::uint32_t highBin = std::min(lowBin + 1, binCount - 1);
    const float t = clamped - static_cast<float>(lowBin);
    const float low = plane[static_cast<std::size_t>(lowBin) * frameCount + frame];
    const float high = plane[static_cast<std::size_t>(highBin) * frameCount + frame];
    return std::lerp(low, high, t);
}

/// @brief Same idea as sampleStoredPlane(), but for the phase plane: the
/// two neighboring stored phases are decomposed into cosine/sine before
/// interpolating, for the same wraparound reason as SpectrumSample.
[[nodiscard]] float sampleStoredPhase(const std::vector<float>& phasePlane, std::uint32_t binCount, std::uint32_t frameCount,
                                      std::uint32_t frame, float logBinIndex) {
    const float clamped = std::clamp(logBinIndex, 0.0f, static_cast<float>(binCount - 1));
    const auto lowBin = static_cast<std::uint32_t>(clamped);
    const std::uint32_t highBin = std::min(lowBin + 1, binCount - 1);
    const float t = clamped - static_cast<float>(lowBin);
    const float lowPhase = phasePlane[static_cast<std::size_t>(lowBin) * frameCount + frame];
    const float highPhase = phasePlane[static_cast<std::size_t>(highBin) * frameCount + frame];
    const float cosPhase = std::lerp(std::cos(lowPhase), std::cos(highPhase), t);
    const float sinPhase = std::lerp(std::sin(lowPhase), std::sin(highPhase), t);
    return std::atan2(sinPhase, cosPhase);
}

}  // namespace

StreamImage encode(const AudioBuffer& audio, const StreamCodecConfig& configIn) {
    StreamCodecConfig config = configIn;
    config.sampleRateHz = audio.sampleRateHz;

    const std::uint32_t fftSize = fftSizeFor(config.hopLength);
    const std::vector<float> window = hannWindow(fftSize);
    const std::size_t numSamples = audio.frameCount();
    const auto frameCount =
        static_cast<std::uint32_t>(numSamples == 0 ? 1 : (numSamples + config.hopLength - 1) / config.hopLength);
    const std::vector<float> linearBinIndex = logBinToLinearBinIndex(config, fftSize);

    StreamImage image;
    image.config = config;
    image.frameCount = frameCount;
    image.sampleCount = numSamples;
    image.leftMagnitudeDb.resize(std::size_t{config.binCount} * frameCount);
    image.rightMagnitudeDb.resize(std::size_t{config.binCount} * frameCount);
    image.sharedPhaseRadians.resize(std::size_t{config.binCount} * frameCount);

    std::vector<float> mid(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
        mid[i] = 0.5f * (audio.left[i] + audio.right[i]);
    }

    std::vector<float> frameLeft(fftSize);
    std::vector<float> frameRight(fftSize);
    std::vector<float> frameMid(fftSize);
    std::vector<std::complex<float>> spectrumLeft;
    std::vector<std::complex<float>> spectrumRight;
    std::vector<std::complex<float>> spectrumMid;

    for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
        const auto start = static_cast<std::ptrdiff_t>(frame) * static_cast<std::ptrdiff_t>(config.hopLength);
        extractFrame(audio.left, start, frameLeft);
        extractFrame(audio.right, start, frameRight);
        extractFrame(mid, start, frameMid);

        for (std::uint32_t i = 0; i < fftSize; ++i) {
            frameLeft[i] *= window[i];
            frameRight[i] *= window[i];
            frameMid[i] *= window[i];
        }

        forwardRealFft(frameLeft, spectrumLeft);
        forwardRealFft(frameRight, spectrumRight);
        forwardRealFft(frameMid, spectrumMid);

        for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
            const float index = linearBinIndex[bin];
            const SpectrumSample left = sampleSpectrum(spectrumLeft, index);
            const SpectrumSample right = sampleSpectrum(spectrumRight, index);
            const SpectrumSample mid_ = sampleSpectrum(spectrumMid, index);

            const std::size_t cell = static_cast<std::size_t>(bin) * frameCount + frame;
            image.leftMagnitudeDb[cell] = amplitudeToDb(left.magnitude);
            image.rightMagnitudeDb[cell] = amplitudeToDb(right.magnitude);
            image.sharedPhaseRadians[cell] = std::atan2(mid_.sinPhase, mid_.cosPhase);
        }
    }

    return image;
}

AudioBuffer decode(const StreamImage& image) {
    const StreamCodecConfig& config = image.config;

    AudioBuffer result;
    result.sampleRateHz = config.sampleRateHz;
    if (image.frameCount == 0) {
        return result;
    }

    const std::uint32_t fftSize = fftSizeFor(config.hopLength);
    const std::vector<float> window = hannWindow(fftSize);
    const std::size_t linearBinCount = fftSize / 2 + 1;
    const std::vector<float> logBinIndexForLinearBin = linearBinToLogBinIndex(config, fftSize);

    const std::uint32_t frameCount = image.frameCount;
    const std::size_t outputLength = static_cast<std::size_t>(frameCount - 1) * config.hopLength + fftSize;

    std::vector<float> leftAccumulator(outputLength, 0.0f);
    std::vector<float> rightAccumulator(outputLength, 0.0f);
    std::vector<float> windowSumSquared(outputLength, 0.0f);

    std::vector<std::complex<float>> spectrumLeft(linearBinCount);
    std::vector<std::complex<float>> spectrumRight(linearBinCount);
    std::vector<float> frameLeft;
    std::vector<float> frameRight;

    for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
        for (std::size_t k = 0; k < linearBinCount; ++k) {
            const float logBinIndex = logBinIndexForLinearBin[k];
            const float phase = sampleStoredPhase(image.sharedPhaseRadians, config.binCount, frameCount, frame, logBinIndex);
            const float leftDb = sampleStoredPlane(image.leftMagnitudeDb, config.binCount, frameCount, frame, logBinIndex);
            const float rightDb = sampleStoredPlane(image.rightMagnitudeDb, config.binCount, frameCount, frame, logBinIndex);
            spectrumLeft[k] = std::polar(dbToAmplitude(leftDb), phase);
            spectrumRight[k] = std::polar(dbToAmplitude(rightDb), phase);
        }

        inverseRealFft(spectrumLeft, fftSize, frameLeft);
        inverseRealFft(spectrumRight, fftSize, frameRight);

        const auto start = static_cast<std::size_t>(frame) * config.hopLength;
        for (std::uint32_t i = 0; i < fftSize; ++i) {
            const float w = window[i];
            leftAccumulator[start + i] += frameLeft[i] * w;
            rightAccumulator[start + i] += frameRight[i] * w;
            windowSumSquared[start + i] += w * w;
        }
    }

    for (std::size_t i = 0; i < outputLength; ++i) {
        const float normalizer = std::max(windowSumSquared[i], kMinWindowSumSquared);
        leftAccumulator[i] /= normalizer;
        rightAccumulator[i] /= normalizer;
    }

    const std::size_t trimmedLength = std::min(static_cast<std::size_t>(image.sampleCount), outputLength);
    result.left.assign(leftAccumulator.begin(), leftAccumulator.begin() + static_cast<std::ptrdiff_t>(trimmedLength));
    result.right.assign(rightAccumulator.begin(), rightAccumulator.begin() + static_cast<std::ptrdiff_t>(trimmedLength));
    return result;
}

}  // namespace sound_mind::codec
