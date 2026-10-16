#include "sound_mind/codec/stream_codec.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

#include <pocketfft_hdronly.h>

#include "sound_mind/codec/parallel.h"
#include "stream_frame_codec.h"

namespace sound_mind::codec {

using namespace detail;  // NOLINT(google-build-using-namespace) - this file's own shared frame-codec helpers.

namespace {

/// @brief Guards the overlap-add normalization divisor near a signal's
/// start/end, where fewer windows have accumulated, against dividing by
/// (near) zero.
constexpr float kMinWindowSumSquared = 1e-6f;

/// @brief Frames per parallelFor() block in encode()/decode() - fixed, so
/// the result never depends on how many threads ran (decode()'s overlap-add
/// merges block sums in block order).
constexpr std::size_t kFramesPerBlock = 64;

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
    // 10^(dB/20) as one exp() - cheaper than pow() in decode()'s inner loop.
    constexpr float kNepersPerDb = 0.11512925464970229f;  // ln(10) / 20.
    return std::exp(db * kNepersPerDb);
}

/// @brief Below this length an interpolated phase vector has no meaningful
/// direction (its two phases were nearly opposite) and decode() treats it as
/// angle 0, as atan2(0, 0) does.
constexpr float kMinPhaseVectorLength = 1e-12f;

/// @brief Where one linear FFT bin reads the stored log bins in decode(),
/// and the dB offset (full-scale reference minus A-weighting) it adds back.
struct LogBinSample {
    std::uint32_t lowBin = 0;
    std::uint32_t highBin = 0;
    float t = 0.0f;
    float offsetDb = 0.0f;
};

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

/// @brief For each linear FFT bin (0..fftSize/2), its own A-weighting dB
/// offset - evaluated at the exact same (clamped) frequency
/// logBinToLinearBinIndex()'s inverse, linearBinToLogBinIndex(), maps that
/// bin to, so decode() removes precisely the offset encode() added for the
/// log-bin position it reads back from (see aWeightingDb()'s own docs).
[[nodiscard]] std::vector<float> linearBinWeightingDb(const StreamCodecConfig& config, std::uint32_t fftSize) {
    const std::size_t linearBinCount = fftSize / 2 + 1;
    std::vector<float> weights(linearBinCount);
    const float maxFrequencyHz = clampedMaxFrequencyHz(config);
    const float hzPerLinearBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);
    for (std::size_t k = 0; k < linearBinCount; ++k) {
        const float frequencyHz = static_cast<float>(k) * hzPerLinearBin;
        const float clampedFrequencyHz = std::clamp(frequencyHz, config.minFrequencyHz, maxFrequencyHz);
        weights[k] = aWeightingDb(clampedFrequencyHz);
    }
    return weights;
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
    const std::vector<float> weightDb = logBinWeightingDb(config);
    const std::uint32_t padSamples = computePadSamples(config, numSamples);

    const float referenceDb = fullScaleReferenceDb(fftSize);

    std::vector<float> mid(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
        mid[i] = 0.5f * (audio.left[i] + audio.right[i]);
    }

    StreamImage image;
    image.config = config;
    image.frameCount = frameCount;
    image.sampleCount = numSamples;
    image.leftMagnitudeDb.resize(std::size_t{config.binCount} * frameCount);
    image.rightMagnitudeDb.resize(std::size_t{config.binCount} * frameCount);
    image.sharedPhaseRadians.resize(std::size_t{config.binCount} * frameCount);

    // Frames are independent and each writes only its own cells, so blocks
    // of them run on worker threads (v0.1.41.2), each with its own buffers.
    parallelFor(frameCount, kFramesPerBlock, [&](std::size_t blockBegin, std::size_t blockEnd) {
    std::vector<float> frameLeft(fftSize);
    std::vector<float> frameRight(fftSize);
    std::vector<float> frameMid(fftSize);
    std::vector<std::complex<float>> spectrumLeft;
    std::vector<std::complex<float>> spectrumRight;
    std::vector<std::complex<float>> spectrumMid;

    for (auto frame = static_cast<std::uint32_t>(blockBegin); frame < blockEnd; ++frame) {
        const auto start = static_cast<std::ptrdiff_t>(frame) * static_cast<std::ptrdiff_t>(config.hopLength);
        extractFrame(audio.left, start, padSamples, frameLeft);
        extractFrame(audio.right, start, padSamples, frameRight);
        extractFrame(mid, start, padSamples, frameMid);

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
            image.leftMagnitudeDb[cell] = amplitudeToDb(left.magnitude) - referenceDb + weightDb[bin];
            image.rightMagnitudeDb[cell] = amplitudeToDb(right.magnitude) - referenceDb + weightDb[bin];
            image.sharedPhaseRadians[cell] = std::atan2(mid_.sinPhase, mid_.cosPhase);
        }
    }
    });

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
    const std::vector<float> weightDb = linearBinWeightingDb(config, fftSize);
    const float referenceDb = fullScaleReferenceDb(fftSize);

    const std::uint32_t frameCount = image.frameCount;
    const std::size_t outputLength = static_cast<std::size_t>(frameCount - 1) * config.hopLength + fftSize;

    std::vector<float> leftAccumulator(outputLength, 0.0f);
    std::vector<float> rightAccumulator(outputLength, 0.0f);
    std::vector<float> windowSumSquared(outputLength, 0.0f);

    // Where each linear bin reads the stored log bins, worked out once for
    // the whole decode rather than per frame (v0.1.41.3).
    std::vector<LogBinSample> samples(linearBinCount);
    for (std::size_t k = 0; k < linearBinCount; ++k) {
        const float clamped = std::clamp(logBinIndexForLinearBin[k], 0.0f, static_cast<float>(config.binCount - 1));
        samples[k].lowBin = static_cast<std::uint32_t>(clamped);
        samples[k].highBin = std::min(samples[k].lowBin + 1, config.binCount - 1);
        samples[k].t = clamped - static_cast<float>(samples[k].lowBin);
        samples[k].offsetDb = referenceDb - weightDb[k];
    }

    // Each block of frames is synthesized on a worker thread (v0.1.41.2)
    // into its own overlap-add buffers, covering just the samples its frames
    // reach; the blocks are then summed into the output in block order, so
    // the result is the same whatever the thread count.
    struct BlockSum {
        std::size_t offset = 0;
        std::vector<float> left;
        std::vector<float> right;
        std::vector<float> weight;
    };
    const std::size_t blockCount = (frameCount + kFramesPerBlock - 1) / kFramesPerBlock;
    std::vector<BlockSum> blockSums(blockCount);
    parallelFor(frameCount, kFramesPerBlock, [&](std::size_t blockBegin, std::size_t blockEnd) {
    BlockSum& sum = blockSums[blockBegin / kFramesPerBlock];
    sum.offset = blockBegin * config.hopLength;
    const std::size_t span = (blockEnd - 1 - blockBegin) * config.hopLength + fftSize;
    sum.left.assign(span, 0.0f);
    sum.right.assign(span, 0.0f);
    sum.weight.assign(span, 0.0f);

    std::vector<std::complex<float>> spectrumLeft(linearBinCount);
    std::vector<std::complex<float>> spectrumRight(linearBinCount);
    std::vector<float> frameLeft;
    std::vector<float> frameRight;
    std::vector<float> cosStored(config.binCount);
    std::vector<float> sinStored(config.binCount);

    for (auto frame = static_cast<std::uint32_t>(blockBegin); frame < blockEnd; ++frame) {
        // Each stored phase's cosine and sine, once per frame (v0.1.41.3) -
        // rather than twice per linear bin reading it, plus an atan2 and
        // two polar() calls per bin to turn the interpolated vector back
        // into an angle and out again.
        for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
            const float phase = image.sharedPhaseRadians[static_cast<std::size_t>(bin) * frameCount + frame];
            cosStored[bin] = std::cos(phase);
            sinStored[bin] = std::sin(phase);
        }
        for (std::size_t k = 0; k < linearBinCount; ++k) {
            const LogBinSample& at = samples[k];
            const std::size_t low = static_cast<std::size_t>(at.lowBin) * frameCount + frame;
            const std::size_t high = static_cast<std::size_t>(at.highBin) * frameCount + frame;
            // The interpolated phase vector, normalized: exactly the unit
            // vector polar(1, atan2(sin, cos)) gives, without the trig.
            // A vanishing vector (opposite phases) reads as angle 0, as
            // atan2(0, 0) did.
            const float cosPhase = std::lerp(cosStored[at.lowBin], cosStored[at.highBin], at.t);
            const float sinPhase = std::lerp(sinStored[at.lowBin], sinStored[at.highBin], at.t);
            const float norm = std::sqrt(cosPhase * cosPhase + sinPhase * sinPhase);
            const float unitCos = norm > kMinPhaseVectorLength ? cosPhase / norm : 1.0f;
            const float unitSin = norm > kMinPhaseVectorLength ? sinPhase / norm : 0.0f;
            const float leftDb = std::lerp(image.leftMagnitudeDb[low], image.leftMagnitudeDb[high], at.t);
            const float rightDb = std::lerp(image.rightMagnitudeDb[low], image.rightMagnitudeDb[high], at.t);
            const float leftAmplitude = dbToAmplitude(leftDb + at.offsetDb);
            const float rightAmplitude = dbToAmplitude(rightDb + at.offsetDb);
            spectrumLeft[k] = {leftAmplitude * unitCos, leftAmplitude * unitSin};
            spectrumRight[k] = {rightAmplitude * unitCos, rightAmplitude * unitSin};
        }

        inverseRealFft(spectrumLeft, fftSize, frameLeft);
        inverseRealFft(spectrumRight, fftSize, frameRight);

        const std::size_t start = static_cast<std::size_t>(frame) * config.hopLength - sum.offset;
        for (std::uint32_t i = 0; i < fftSize; ++i) {
            const float w = window[i];
            sum.left[start + i] += frameLeft[i] * w;
            sum.right[start + i] += frameRight[i] * w;
            sum.weight[start + i] += w * w;
        }
    }
    });
    for (const BlockSum& sum : blockSums) {
        for (std::size_t i = 0; i < sum.left.size(); ++i) {
            leftAccumulator[sum.offset + i] += sum.left[i];
            rightAccumulator[sum.offset + i] += sum.right[i];
            windowSumSquared[sum.offset + i] += sum.weight[i];
        }
    }


    // In the steady state the windows' squared sum is constant; only the
    // first and last frames see just the edge of a window, where it tends
    // to zero. Dividing by that tiny sum multiplied whatever those frames
    // held - not tapered, since the spectrum is interpolated from log bins -
    // into a loud click at both ends. Flooring it at a quarter of the steady
    // value leaves the steady state untouched and turns the ends into a
    // short fade instead.
    const float steadyWindowSum = *std::max_element(windowSumSquared.begin(), windowSumSquared.end());
    const float normalizerFloor = std::max(kMinWindowSumSquared, 0.25f * steadyWindowSum);
    for (std::size_t i = 0; i < outputLength; ++i) {
        const float normalizer = std::max(windowSumSquared[i], normalizerFloor);
        leftAccumulator[i] = leftAccumulator[i] / normalizer;
        rightAccumulator[i] = rightAccumulator[i] / normalizer;
    }

    const std::size_t trimmedLength = std::min(static_cast<std::size_t>(image.sampleCount), outputLength);
    result.left.assign(leftAccumulator.begin(), leftAccumulator.begin() + static_cast<std::ptrdiff_t>(trimmedLength));
    result.right.assign(rightAccumulator.begin(), rightAccumulator.begin() + static_cast<std::ptrdiff_t>(trimmedLength));
    return result;
}

float aWeightingOffsetDb(float frequencyHz) noexcept { return detail::aWeightingDb(frequencyHz); }

}  // namespace sound_mind::codec
