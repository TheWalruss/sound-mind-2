#pragma once

// Private (non-installed) per-frame STFT analysis primitives shared by
// stream_codec.cpp's whole-buffer encode() and
// stream_incremental_encoder.cpp's growing/live counterpart - factored out
// specifically so both share the exact same DSP logic rather than
// duplicating it (this is real, substantial signal-processing code, not
// the kind of small single-use helper this codebase otherwise duplicates
// per-file - see color_mapping.cpp's fftSizeFor() for that narrower case).
// decode()-side helpers stay in stream_codec.cpp's own anonymous namespace,
// since decoding isn't made incremental here (see stream_incremental_
// encoder.h's docs for why the output side takes a different approach).

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <vector>

#include <pocketfft_hdronly.h>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::codec::detail {

constexpr float kMinLinearAmplitude = 1e-7f;

/// @brief FFT size for a given hop length: 4x hop, i.e. 75% overlap with a
/// Hann analysis/synthesis window - matches the legacy codec's fallback
/// STFT backend's overlap ratio (see docs/legacy/CODEC_DETAILS.md's
/// VulkanSTFTBackend).
[[nodiscard]] inline std::uint32_t fftSizeFor(std::uint32_t hopLength) noexcept { return hopLength * 4; }

/// @brief A periodic Hann window of the given size.
[[nodiscard]] inline std::vector<float> hannWindow(std::size_t size) {
    std::vector<float> window(size);
    for (std::size_t i = 0; i < size; ++i) {
        window[i] =
            0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(size));
    }
    return window;
}

/// @brief The number of samples to reflection-pad a signal by at each end
/// before transforming, per `encode()`'s own docs on edge-artifact
/// mitigation - one period of the lowest encoded frequency, clamped so the
/// reflection never needs to look further back/forward than the signal
/// itself actually has samples for (`numSamples - 1`, matching `std::pad`'s
/// own `'reflect'` mode requirement, and the legacy codec's identical
/// clamp - see `docs/legacy/CODEC_DETAILS.md` section 3.2).
[[nodiscard]] inline std::uint32_t computePadSamples(const StreamCodecConfig& config, std::size_t numSamples) noexcept {
    if (numSamples <= 1 || config.minFrequencyHz <= 0.0f) {
        return 0;
    }
    const auto periodSamples =
        static_cast<std::uint32_t>(static_cast<float>(config.sampleRateHz) / config.minFrequencyHz);
    return std::min(periodSamples, static_cast<std::uint32_t>(numSamples - 1));
}

/// @brief Copies `frame.size()` samples starting at `start` from `source`
/// into `frame`. Anywhere that falls outside `source`'s own range but
/// within `padSamples` of it is filled by reflecting `source`'s own
/// content back on itself (mirroring without repeating the boundary
/// sample - the same `'reflect'` convention `numpy.pad()` uses, and the
/// legacy codec's own edge-artifact mitigation, re-derived here rather
/// than ported); anywhere beyond that - or when `padSamples` is `0`, the
/// exact prior behavior - is zero-filled, same as always.
inline void extractFrame(const std::vector<float>& source, std::ptrdiff_t start, std::uint32_t padSamples,
                          std::vector<float>& frame) {
    const auto sourceSize = static_cast<std::ptrdiff_t>(source.size());
    const auto pad = static_cast<std::ptrdiff_t>(padSamples);
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const std::ptrdiff_t sourceIndex = start + static_cast<std::ptrdiff_t>(i);
        float sample = 0.0f;
        if (sourceIndex >= 0 && sourceIndex < sourceSize) {
            sample = source[static_cast<std::size_t>(sourceIndex)];
        } else if (sourceSize > 1 && sourceIndex < 0 && -sourceIndex <= pad) {
            sample = source[static_cast<std::size_t>(-sourceIndex)];
        } else if (sourceSize > 1 && sourceIndex >= sourceSize && (sourceIndex - sourceSize) < pad) {
            const std::ptrdiff_t reflected = 2 * sourceSize - 2 - sourceIndex;
            if (reflected >= 0 && reflected < sourceSize) {
                sample = source[static_cast<std::size_t>(reflected)];
            }
        }
        frame[i] = sample;
    }
}

/// @brief The standard IEC 61672 A-weighting curve, as a dB offset
/// normalized to `0` dB at 1 kHz - frequencies well below 1 kHz read
/// negative (attenuated, matching the ear's reduced bass sensitivity),
/// frequencies well above read slightly negative too past ~8 kHz. Shared,
/// identical-formula, decode-reversible cosmetic weighting applied to
/// every stored dB value in both codecs - see
/// `docs/sound-mind-codec-design.md` for the full write-up of why.
[[nodiscard]] inline float aWeightingDb(float frequencyHz) noexcept {
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

/// @brief Forward real FFT: `frame.size()` real samples -> `frame.size()/2 + 1` complex bins.
inline void forwardRealFft(const std::vector<float>& frame, std::vector<std::complex<float>>& bins) {
    const std::size_t n = frame.size();
    bins.resize(n / 2 + 1);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t strideIn{sizeof(float)};
    const pocketfft::stride_t strideOut{sizeof(std::complex<float>)};
    pocketfft::r2c(shape, strideIn, strideOut, std::size_t{0}, pocketfft::FORWARD, frame.data(), bins.data(), 1.0f);
}

[[nodiscard]] inline float amplitudeToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude));
}

/// @brief The Nyquist-clamped upper edge of a config's encoded frequency range.
[[nodiscard]] inline float clampedMaxFrequencyHz(const StreamCodecConfig& config) noexcept {
    return std::min(config.maxFrequencyHz, static_cast<float>(config.sampleRateHz) / 2.0f);
}

/// @brief The center frequency of a config's `bin`-th log-spaced output
/// bin - the same log-spacing formula every log-binned frequency axis in
/// this codebase uses (see `pool_codec.cpp`'s own `centerFrequencyHz()`,
/// duplicated there rather than shared, per this codebase's convention).
[[nodiscard]] inline float logBinCenterFrequencyHz(const StreamCodecConfig& config, std::uint32_t bin) noexcept {
    const float maxFrequencyHz = clampedMaxFrequencyHz(config);
    const float logRange = std::log(maxFrequencyHz / config.minFrequencyHz);
    const float t = (config.binCount > 1) ? static_cast<float>(bin) / static_cast<float>(config.binCount - 1) : 0.0f;
    return config.minFrequencyHz * std::exp(t * logRange);
}

/// @brief For each of `config.binCount` log-spaced output bins, the
/// fractional linear-FFT-bin index it maps to, for the given `fftSize`.
[[nodiscard]] inline std::vector<float> logBinToLinearBinIndex(const StreamCodecConfig& config, std::uint32_t fftSize) {
    std::vector<float> linearIndex(config.binCount);
    const float hzPerLinearBin = static_cast<float>(config.sampleRateHz) / static_cast<float>(fftSize);
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        linearIndex[bin] = logBinCenterFrequencyHz(config, bin) / hzPerLinearBin;
    }
    return linearIndex;
}

/// @brief The A-weighting dB offset (see aWeightingDb()) for each of
/// `config.binCount` log-spaced output bins, in bin order - precomputed
/// once per `encode()`/`StreamIncrementalEncoder` construction rather than
/// recomputed per frame, since it depends only on `bin`.
[[nodiscard]] inline std::vector<float> logBinWeightingDb(const StreamCodecConfig& config) {
    std::vector<float> weights(config.binCount);
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        weights[bin] = aWeightingDb(logBinCenterFrequencyHz(config, bin));
    }
    return weights;
}

/// @brief One frequency-domain sample: magnitude plus phase decomposed into
/// its cosine/sine components (rather than a raw angle), so callers can
/// linearly interpolate two of these without the wraparound artefacts a
/// direct angle interpolation would produce - the same reasoning as
/// docs/legacy/CODEC_DETAILS.md section 3.9's phase resampling.
struct SpectrumSample {
    /// @brief Linear magnitude (not dB).
    float magnitude = 0.0f;
    /// @brief Cosine of the phase angle.
    float cosPhase = 1.0f;
    /// @brief Sine of the phase angle.
    float sinPhase = 0.0f;
};

/// @brief Linearly interpolates `spectrum` at the fractional index
/// `index`, clamped to the array's valid range.
[[nodiscard]] inline SpectrumSample sampleSpectrum(const std::vector<std::complex<float>>& spectrum, float index) {
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

}  // namespace sound_mind::codec::detail
