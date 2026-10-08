#include "sound_mind/codec/pool_codec.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

#include <pocketfft_hdronly.h>

namespace sound_mind::codec {

namespace {

constexpr float kMinLinearAmplitude = 1e-7f;
constexpr float kMinWindowSumSquared = 1e-6f;

// A bin's frequency-domain window needs at least a few samples to be a
// meaningful window at all - below this, treat it as this floor instead of
// letting it collapse to something degenerate (0 or 1).
constexpr std::uint32_t kMinBinWindowLength = 4;

/// @brief The standard IEC 61672 A-weighting curve, as a dB offset
/// normalized to `0` dB at 1 kHz - see `stream_frame_codec.h`'s own
/// identical copy (duplicated rather than shared, per this codebase's
/// convention - Pool doesn't include Stream's own private header) for the
/// full write-up of why this exists.
[[nodiscard]] float aWeightingDb(float frequencyHz) noexcept {
    const float f = std::max(frequencyHz, 1.0f);
    const double f2 = static_cast<double>(f) * static_cast<double>(f);
    constexpr double kF1 = 20.6;
    constexpr double kF2 = 107.7;
    constexpr double kF3 = 737.9;
    constexpr double kF4 = 12194.0;
    const double numerator = (kF4 * kF4) * (f2 * f2);
    const double denominator = (f2 + kF1 * kF1) * std::sqrt((f2 + kF2 * kF2) * (f2 + kF3 * kF3)) * (f2 + kF4 * kF4);
    const double ra = numerator / std::max(denominator, 1e-12);
    return static_cast<float>(20.0 * std::log10(std::max(ra, 1e-12)) + 2.00);
}

[[nodiscard]] float amplitudeToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude));
}

[[nodiscard]] float dbToAmplitude(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

[[nodiscard]] float clampedMaxFrequencyHz(const StreamCodecConfig& config) noexcept {
    return std::min(config.maxFrequencyHz, static_cast<float>(config.sampleRateHz) / 2.0f);
}

/// @brief Center frequency of a given log-spaced bin.
[[nodiscard]] float centerFrequencyHz(const StreamCodecConfig& config, std::uint32_t bin) noexcept {
    const float maxFrequencyHz = clampedMaxFrequencyHz(config);
    const float logRange = std::log(maxFrequencyHz / config.minFrequencyHz);
    const float t = (config.binCount > 1) ? static_cast<float>(bin) / static_cast<float>(config.binCount - 1) : 0.0f;
    return config.minFrequencyHz * std::exp(t * logRange);
}

/// @brief The per-bin frequency-domain window length (in FFT bins of the
/// global `fftSize`-point transform) for a constant-Q log scale: each
/// bin's bandwidth is proportional to its own center frequency, giving
/// deep frequency resolution (long windows) at the low end and fine time
/// resolution (short windows) at the high end.
[[nodiscard]] std::uint32_t binWindowLength(const StreamCodecConfig& config, std::uint32_t bin,
                                            std::uint32_t fftSize) noexcept {
    const float maxFrequencyHz = clampedMaxFrequencyHz(config);
    const float stepRatio = (config.binCount > 1)
                                 ? std::pow(maxFrequencyHz / config.minFrequencyHz, 1.0f / static_cast<float>(config.binCount - 1))
                                 : 1.0f;
    const float centerHz = centerFrequencyHz(config, bin);
    const float bandwidthHz = centerHz * (stepRatio - 1.0f);
    const float hzPerFftBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);
    const auto length = static_cast<std::uint32_t>(std::lround(bandwidthHz / std::max(hzPerFftBin, 1e-6f)));
    return std::max(length, kMinBinWindowLength);
}

/// @brief A periodic Hann window - NSGT's per-bin analysis/synthesis
/// window, applied in the frequency domain (the dual of Stream's
/// time-domain window).
[[nodiscard]] std::vector<float> hannWindow(std::size_t size) {
    std::vector<float> window(size);
    for (std::size_t i = 0; i < size; ++i) {
        window[i] =
            0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(size));
    }
    return window;
}

/// @brief Extracts a `window.size()`-long, Hann-windowed slice of
/// `spectrum` centered at `centerIndex`, zero-filling anywhere outside
/// `spectrum`'s range.
void extractWindowedSlice(const std::vector<std::complex<float>>& spectrum, std::ptrdiff_t centerIndex,
                           const std::vector<float>& window, std::vector<std::complex<float>>& slice) {
    slice.resize(window.size());
    const auto half = static_cast<std::ptrdiff_t>(window.size() / 2);
    const auto spectrumSize = static_cast<std::ptrdiff_t>(spectrum.size());
    for (std::size_t i = 0; i < window.size(); ++i) {
        const std::ptrdiff_t sourceIndex = centerIndex - half + static_cast<std::ptrdiff_t>(i);
        const std::complex<float> value = (sourceIndex >= 0 && sourceIndex < spectrumSize)
                                               ? spectrum[static_cast<std::size_t>(sourceIndex)]
                                               : std::complex<float>{0.0f, 0.0f};
        slice[i] = value * window[i];
    }
}

/// @brief Forward complex FFT: `n`-length complex input -> `n`-length
/// complex output (unnormalized, per PocketFFT convention).
void forwardComplexFft(const std::vector<std::complex<float>>& input, std::vector<std::complex<float>>& output) {
    const std::size_t n = input.size();
    output.resize(n);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t stride{sizeof(std::complex<float>)};
    pocketfft::c2c(shape, stride, stride, pocketfft::shape_t{0}, pocketfft::FORWARD, input.data(), output.data(), 1.0f);
}

/// @brief Inverse complex FFT: `n`-length complex input -> `n`-length
/// complex output, normalized by `1/n`.
void inverseComplexFft(const std::vector<std::complex<float>>& input, std::vector<std::complex<float>>& output) {
    const std::size_t n = input.size();
    output.resize(n);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t stride{sizeof(std::complex<float>)};
    pocketfft::c2c(shape, stride, stride, pocketfft::shape_t{0}, pocketfft::BACKWARD, input.data(), output.data(),
                   1.0f / static_cast<float>(n));
}

/// @brief Forward real FFT: `samples.size()` real samples ->
/// `samples.size()/2 + 1` complex bins.
void forwardRealFft(const std::vector<float>& samples, std::vector<std::complex<float>>& spectrum) {
    const std::size_t n = samples.size();
    spectrum.resize(n / 2 + 1);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t strideIn{sizeof(float)};
    const pocketfft::stride_t strideOut{sizeof(std::complex<float>)};
    pocketfft::r2c(shape, strideIn, strideOut, std::size_t{0}, pocketfft::FORWARD, samples.data(), spectrum.data(), 1.0f);
}

/// @brief Inverse real FFT: `n/2 + 1` complex bins -> `n` real samples.
void inverseRealFft(const std::vector<std::complex<float>>& spectrum, std::size_t n, std::vector<float>& samples) {
    samples.resize(n);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t strideIn{sizeof(std::complex<float>)};
    const pocketfft::stride_t strideOut{sizeof(float)};
    pocketfft::c2r(shape, strideIn, strideOut, std::size_t{0}, pocketfft::BACKWARD, spectrum.data(), samples.data(),
                   1.0f / static_cast<float>(n));
}

/// @brief One time-domain sample of a bin's native-rate coefficient
/// sequence: magnitude plus phase decomposed into cosine/sine (rather than
/// a raw angle), so two of these can be linearly interpolated without the
/// wraparound artefacts a direct angle interpolation would produce - same
/// reasoning as stream_codec.cpp's SpectrumSample (duplicated here rather
/// than shared, per this codebase's convention for small single-use
/// helpers - see color_mapping.cpp's fftSizeFor() for the same call).
struct NativeSample {
    float magnitude = 0.0f;
    float cosPhase = 1.0f;
    float sinPhase = 0.0f;
};

[[nodiscard]] NativeSample sampleNativeSequence(const std::vector<std::complex<float>>& native, float index) {
    const auto maxIndex = static_cast<float>(native.size() - 1);
    const float clamped = std::clamp(index, 0.0f, maxIndex);
    const auto lowIndex = static_cast<std::size_t>(clamped);
    const std::size_t highIndex = std::min(lowIndex + 1, native.size() - 1);
    const float t = clamped - static_cast<float>(lowIndex);

    const std::complex<float>& low = native[lowIndex];
    const std::complex<float>& high = native[highIndex];

    NativeSample sample;
    sample.magnitude = std::lerp(std::abs(low), std::abs(high), t);
    sample.cosPhase = std::lerp(std::cos(std::arg(low)), std::cos(std::arg(high)), t);
    sample.sinPhase = std::lerp(std::sin(std::arg(low)), std::sin(std::arg(high)), t);
    return sample;
}

/// @brief Interpolates a bin's native-rate coefficient sequence onto the
/// image's `frameCount`-wide common grid, storing dB magnitude (plus
/// `weightDb`, the bin's own A-weighting offset - see `aWeightingDb()`)
/// and phase.
void storeInterpolated(const std::vector<std::complex<float>>& native, std::uint32_t bin, std::uint32_t frameCount,
                        float weightDb, std::vector<float>& magnitudeDb, std::vector<float>& phaseRadians) {
    for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
        const float t = (frameCount > 1) ? static_cast<float>(frame) / static_cast<float>(frameCount - 1) : 0.0f;
        const float nativeIndex = t * static_cast<float>(native.size() - 1);
        const NativeSample sample = sampleNativeSequence(native, nativeIndex);
        const std::size_t cell = static_cast<std::size_t>(bin) * frameCount + frame;
        magnitudeDb[cell] = amplitudeToDb(sample.magnitude) + weightDb;
        phaseRadians[cell] = std::atan2(sample.sinPhase, sample.cosPhase);
    }
}

/// @brief The inverse of storeInterpolated()'s magnitude side: linearly
/// interpolates a stored `[bin][frame]` plane along the frame axis, for a
/// fixed bin, at the fractional frame index `frameIndex`.
[[nodiscard]] float sampleStoredMagnitude(const std::vector<float>& magnitudeDb, std::uint32_t frameCount,
                                          std::uint32_t bin, float frameIndex) {
    const float clamped = std::clamp(frameIndex, 0.0f, static_cast<float>(frameCount - 1));
    const auto lowFrame = static_cast<std::uint32_t>(clamped);
    const std::uint32_t highFrame = std::min(lowFrame + 1, frameCount - 1);
    const float t = clamped - static_cast<float>(lowFrame);
    const float low = magnitudeDb[static_cast<std::size_t>(bin) * frameCount + lowFrame];
    const float high = magnitudeDb[static_cast<std::size_t>(bin) * frameCount + highFrame];
    return std::lerp(low, high, t);
}

/// @brief The inverse of storeInterpolated()'s phase side, same idea as
/// sampleStoredMagnitude() but decomposing into cosine/sine first to avoid
/// wraparound artefacts.
[[nodiscard]] float sampleStoredPhase(const std::vector<float>& phaseRadians, std::uint32_t frameCount,
                                      std::uint32_t bin, float frameIndex) {
    const float clamped = std::clamp(frameIndex, 0.0f, static_cast<float>(frameCount - 1));
    const auto lowFrame = static_cast<std::uint32_t>(clamped);
    const std::uint32_t highFrame = std::min(lowFrame + 1, frameCount - 1);
    const float t = clamped - static_cast<float>(lowFrame);
    const float lowPhase = phaseRadians[static_cast<std::size_t>(bin) * frameCount + lowFrame];
    const float highPhase = phaseRadians[static_cast<std::size_t>(bin) * frameCount + highFrame];
    const float cosPhase = std::lerp(std::cos(lowPhase), std::cos(highPhase), t);
    const float sinPhase = std::lerp(std::sin(lowPhase), std::sin(highPhase), t);
    return std::atan2(sinPhase, cosPhase);
}

}  // namespace

PoolImage poolEncode(const AudioBuffer& audio, const StreamCodecConfig& configIn) {
    StreamCodecConfig config = configIn;
    config.sampleRateHz = audio.sampleRateHz;

    const std::size_t numSamples = audio.frameCount();
    const auto fftSize = static_cast<std::uint32_t>(std::max<std::size_t>(numSamples, 1));
    const auto frameCount =
        static_cast<std::uint32_t>(numSamples == 0 ? 1 : (numSamples + config.hopLength - 1) / config.hopLength);

    PoolImage image;
    image.config = config;
    image.frameCount = frameCount;
    image.sampleCount = numSamples;
    image.leftMagnitudeDb.resize(std::size_t{config.binCount} * frameCount);
    image.rightMagnitudeDb.resize(std::size_t{config.binCount} * frameCount);
    image.leftPhaseRadians.resize(std::size_t{config.binCount} * frameCount);
    image.rightPhaseRadians.resize(std::size_t{config.binCount} * frameCount);

    // Input-level normalization - see poolEncode()'s own docs. One
    // combined peak across both channels (not one per channel), so stereo
    // balance is preserved rather than each channel scaling independently.
    float peak = 0.0f;
    for (const float sample : audio.left) peak = std::max(peak, std::abs(sample));
    for (const float sample : audio.right) peak = std::max(peak, std::abs(sample));
    const float scale = (peak > 0.0f) ? (0.95f / peak) : 1.0f;
    image.inputNormalizationScale = scale;

    // NSGT operates on the whole (normalized) signal at once - one global
    // real FFT per channel, not framed like Stream's STFT.
    std::vector<float> leftPadded(numSamples);
    std::vector<float> rightPadded(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
        leftPadded[i] = audio.left[i] * scale;
        rightPadded[i] = audio.right[i] * scale;
    }
    leftPadded.resize(fftSize, 0.0f);
    rightPadded.resize(fftSize, 0.0f);

    std::vector<std::complex<float>> leftSpectrum;
    std::vector<std::complex<float>> rightSpectrum;
    forwardRealFft(leftPadded, leftSpectrum);
    forwardRealFft(rightPadded, rightSpectrum);

    const float hzPerFftBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);

    std::vector<std::complex<float>> slice;
    std::vector<std::complex<float>> nativeSequence;

    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        const float centerHz = centerFrequencyHz(config, bin);
        const auto centerIndex = static_cast<std::ptrdiff_t>(std::lround(centerHz / hzPerFftBin));
        const std::uint32_t windowLength = binWindowLength(config, bin, fftSize);
        const std::vector<float> window = hannWindow(windowLength);
        const float weightDb = aWeightingDb(centerHz);

        extractWindowedSlice(leftSpectrum, centerIndex, window, slice);
        inverseComplexFft(slice, nativeSequence);
        storeInterpolated(nativeSequence, bin, frameCount, weightDb, image.leftMagnitudeDb, image.leftPhaseRadians);

        extractWindowedSlice(rightSpectrum, centerIndex, window, slice);
        inverseComplexFft(slice, nativeSequence);
        storeInterpolated(nativeSequence, bin, frameCount, weightDb, image.rightMagnitudeDb, image.rightPhaseRadians);
    }

    return image;
}

AudioBuffer poolDecode(const PoolImage& image) {
    const StreamCodecConfig& config = image.config;

    AudioBuffer result;
    result.sampleRateHz = config.sampleRateHz;
    if (image.frameCount == 0 || image.sampleCount == 0) {
        return result;
    }

    const auto fftSize = static_cast<std::uint32_t>(image.sampleCount);
    const std::size_t spectrumSize = fftSize / 2 + 1;

    std::vector<std::complex<float>> leftAccumulator(spectrumSize, std::complex<float>{0.0f, 0.0f});
    std::vector<std::complex<float>> rightAccumulator(spectrumSize, std::complex<float>{0.0f, 0.0f});
    std::vector<float> windowSumSquared(spectrumSize, 0.0f);

    const float hzPerFftBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);

    std::vector<std::complex<float>> nativeSequence;
    std::vector<std::complex<float>> resynthesized;

    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        const float centerHz = centerFrequencyHz(config, bin);
        const auto centerIndex = static_cast<std::ptrdiff_t>(std::lround(centerHz / hzPerFftBin));
        const std::uint32_t windowLength = binWindowLength(config, bin, fftSize);
        const std::vector<float> window = hannWindow(windowLength);
        const auto half = static_cast<std::ptrdiff_t>(windowLength / 2);
        const float weightDb = aWeightingDb(centerHz);

        for (int channel = 0; channel < 2; ++channel) {
            const std::vector<float>& magnitudeDb = (channel == 0) ? image.leftMagnitudeDb : image.rightMagnitudeDb;
            const std::vector<float>& phaseRadians = (channel == 0) ? image.leftPhaseRadians : image.rightPhaseRadians;
            std::vector<std::complex<float>>& accumulator = (channel == 0) ? leftAccumulator : rightAccumulator;

            nativeSequence.resize(windowLength);
            for (std::uint32_t i = 0; i < windowLength; ++i) {
                const float t = (windowLength > 1) ? static_cast<float>(i) / static_cast<float>(windowLength - 1) : 0.0f;
                const float frameIndex = t * static_cast<float>(image.frameCount - 1);
                const float db = sampleStoredMagnitude(magnitudeDb, image.frameCount, bin, frameIndex);
                const float phase = sampleStoredPhase(phaseRadians, image.frameCount, bin, frameIndex);
                nativeSequence[i] = std::polar(dbToAmplitude(db - weightDb), phase);
            }

            forwardComplexFft(nativeSequence, resynthesized);

            for (std::uint32_t i = 0; i < windowLength; ++i) {
                const std::ptrdiff_t targetIndex = centerIndex - half + static_cast<std::ptrdiff_t>(i);
                if (targetIndex >= 0 && targetIndex < static_cast<std::ptrdiff_t>(spectrumSize)) {
                    const float w = window[i];
                    accumulator[static_cast<std::size_t>(targetIndex)] += resynthesized[i] * w;
                    if (channel == 0) {
                        windowSumSquared[static_cast<std::size_t>(targetIndex)] += w * w;
                    }
                }
            }
        }
    }

    for (std::size_t i = 0; i < spectrumSize; ++i) {
        const float normalizer = std::max(windowSumSquared[i], kMinWindowSumSquared);
        leftAccumulator[i] /= normalizer;
        rightAccumulator[i] /= normalizer;
    }

    std::vector<float> leftTime;
    std::vector<float> rightTime;
    inverseRealFft(leftAccumulator, fftSize, leftTime);
    inverseRealFft(rightAccumulator, fftSize, rightTime);

    // Undo poolEncode()'s input-level normalization - see
    // PoolImage::inputNormalizationScale's own docs. Floored defensively,
    // same reasoning as stream_codec.cpp's decode().
    const float unnormalize = 1.0f / std::max(image.inputNormalizationScale, 1e-6f);
    for (float& sample : leftTime) sample *= unnormalize;
    for (float& sample : rightTime) sample *= unnormalize;

    const std::size_t trimmedLength = std::min(static_cast<std::size_t>(image.sampleCount), static_cast<std::size_t>(fftSize));
    result.left.assign(leftTime.begin(), leftTime.begin() + static_cast<std::ptrdiff_t>(trimmedLength));
    result.right.assign(rightTime.begin(), rightTime.begin() + static_cast<std::ptrdiff_t>(trimmedLength));
    return result;
}

}  // namespace sound_mind::codec
