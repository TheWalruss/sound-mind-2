#include "sound_mind/codec/stream_codec.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

#include <pocketfft_hdronly.h>

namespace sound_mind::codec {

namespace {

/// @brief dB floor guard well below audible range, avoiding -inf for exact
/// silence rather than approximating a perceptually meaningful noise floor
/// (that's a Pool/TIFF-pixel-quantization concern - see docs/legacy/
/// CODEC_DETAILS.md's -96 dBFS rationale - not relevant here, since Stream
/// stores real float32 values rather than quantizing into a fixed pixel
/// range).
constexpr float kMinLinearAmplitude = 1e-7f;

/// @brief Guards the overlap-add normalization divisor near a signal's
/// start/end, where fewer windows have accumulated, against dividing by
/// (near) zero.
constexpr float kMinWindowSumSquared = 1e-6f;

/// @brief FFT size for a given hop length: 4x hop, i.e. 75% overlap with a
/// Hann analysis/synthesis window - matches the legacy codec's fallback
/// STFT backend's overlap ratio (see docs/legacy/CODEC_DETAILS.md's
/// VulkanSTFTBackend).
[[nodiscard]] std::uint32_t fftSizeFor(std::uint32_t hopLength) noexcept {
    return hopLength * 4;
}

/// @brief A periodic Hann window of the given size.
[[nodiscard]] std::vector<float> hannWindow(std::size_t size) {
    std::vector<float> window(size);
    for (std::size_t i = 0; i < size; ++i) {
        window[i] =
            0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(size));
    }
    return window;
}

/// @brief Copies `frame.size()` samples starting at `start` from `source`
/// into `frame`, zero-filling anywhere that falls outside `source`'s range.
void extractFrame(const std::vector<float>& source, std::ptrdiff_t start, std::vector<float>& frame) {
    const auto sourceSize = static_cast<std::ptrdiff_t>(source.size());
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const std::ptrdiff_t sourceIndex = start + static_cast<std::ptrdiff_t>(i);
        frame[i] = (sourceIndex >= 0 && sourceIndex < sourceSize) ? source[static_cast<std::size_t>(sourceIndex)] : 0.0f;
    }
}

/// @brief Forward real FFT: `frame.size()` real samples -> `frame.size()/2 + 1` complex bins.
void forwardRealFft(const std::vector<float>& frame, std::vector<std::complex<float>>& bins) {
    const std::size_t n = frame.size();
    bins.resize(n / 2 + 1);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t strideIn{sizeof(float)};
    const pocketfft::stride_t strideOut{sizeof(std::complex<float>)};
    pocketfft::r2c(shape, strideIn, strideOut, std::size_t{0}, pocketfft::FORWARD, frame.data(), bins.data(), 1.0f);
}

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

[[nodiscard]] float amplitudeToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude));
}

[[nodiscard]] float dbToAmplitude(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

/// @brief The Nyquist-clamped upper edge of a config's encoded frequency range.
[[nodiscard]] float clampedMaxFrequencyHz(const StreamCodecConfig& config) noexcept {
    return std::min(config.maxFrequencyHz, static_cast<float>(config.sampleRateHz) / 2.0f);
}

/// @brief For each of `config.binCount` log-spaced output bins, the
/// fractional linear-FFT-bin index it maps to, for the given `fftSize`.
[[nodiscard]] std::vector<float> logBinToLinearBinIndex(const StreamCodecConfig& config, std::uint32_t fftSize) {
    std::vector<float> linearIndex(config.binCount);
    const float maxFrequencyHz = clampedMaxFrequencyHz(config);
    const float logRange = std::log(maxFrequencyHz / config.minFrequencyHz);
    const float hzPerLinearBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        const float t = (config.binCount > 1) ? static_cast<float>(bin) / static_cast<float>(config.binCount - 1) : 0.0f;
        const float logFrequencyHz = config.minFrequencyHz * std::exp(t * logRange);
        linearIndex[bin] = logFrequencyHz / hzPerLinearBin;
    }
    return linearIndex;
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

/// @brief One frequency-domain sample: magnitude plus phase decomposed into
/// its cosine/sine components (rather than a raw angle), so callers can
/// linearly interpolate two of these without the wraparound artefacts a
/// direct angle interpolation would produce - the same reasoning as
/// docs/legacy/CODEC_DETAILS.md section 3.9's phase resampling.
struct SpectrumSample {
    float magnitude = 0.0f;
    float cosPhase = 1.0f;
    float sinPhase = 0.0f;
};

/// @brief Linearly interpolates `spectrum` at the fractional index
/// `index`, clamped to the array's valid range.
[[nodiscard]] SpectrumSample sampleSpectrum(const std::vector<std::complex<float>>& spectrum, float index) {
    const auto maxIndex = static_cast<float>(spectrum.size() - 1);
    const float clamped = std::clamp(index, 0.0f, maxIndex);
    const auto lowIndex = static_cast<std::size_t>(clamped);
    const std::size_t highIndex = std::min(lowIndex + 1, spectrum.size() - 1);
    const float t = clamped - static_cast<float>(lowIndex);

    const std::complex<float>& low = spectrum[lowIndex];
    const std::complex<float>& high = spectrum[highIndex];

    SpectrumSample sample;
    sample.magnitude = std::lerp(std::abs(low), std::abs(high), t);
    sample.cosPhase = std::lerp(std::cos(std::arg(low)), std::cos(std::arg(high)), t);
    sample.sinPhase = std::lerp(std::sin(std::arg(low)), std::sin(std::arg(high)), t);
    return sample;
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
