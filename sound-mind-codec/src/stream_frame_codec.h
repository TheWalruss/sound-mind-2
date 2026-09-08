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

/// @brief Copies `frame.size()` samples starting at `start` from `source`
/// into `frame`, zero-filling anywhere that falls outside `source`'s range.
inline void extractFrame(const std::vector<float>& source, std::ptrdiff_t start, std::vector<float>& frame) {
    const auto sourceSize = static_cast<std::ptrdiff_t>(source.size());
    for (std::size_t i = 0; i < frame.size(); ++i) {
        const std::ptrdiff_t sourceIndex = start + static_cast<std::ptrdiff_t>(i);
        frame[i] = (sourceIndex >= 0 && sourceIndex < sourceSize) ? source[static_cast<std::size_t>(sourceIndex)] : 0.0f;
    }
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

/// @brief For each of `config.binCount` log-spaced output bins, the
/// fractional linear-FFT-bin index it maps to, for the given `fftSize`.
[[nodiscard]] inline std::vector<float> logBinToLinearBinIndex(const StreamCodecConfig& config, std::uint32_t fftSize) {
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
