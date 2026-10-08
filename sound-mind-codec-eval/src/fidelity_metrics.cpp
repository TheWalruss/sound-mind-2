#include "sound_mind/codec_eval/fidelity_metrics.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include <pocketfft_hdronly.h>

namespace sound_mind::codec_eval {

namespace {

// Floors an error-energy denominator so a sample-exact (or silent-error)
// comparison divides by a small positive number instead of zero - the same
// epsilon-floor pattern sound-mind-codec's own amplitudeToDb() helpers use
// (see stream_frame_codec.h's kMinLinearAmplitude), just named for this
// tool's own use rather than shared across modules. Deliberately tiny
// (not just "small"): real audio signal energy ranges many orders of
// magnitude (a single FFT bin's leaked energy from windowing a
// non-periodic tone can be as low as ~1e-8), and this floor must still be
// far enough below *that* for a genuinely zero-error comparison to read as
// kSnrCapDb, not just "high."
constexpr double kMinErrorEnergy = 1e-30;

/// @brief Forward real FFT: `n` real samples -> `n/2 + 1` complex bins.
/// Duplicated rather than shared with sound-mind-codec's own private
/// per-file copies (stream_frame_codec.h, pool_codec.cpp) - this module
/// deliberately doesn't link sound-mind-codec's internal headers, only its
/// public include/ surface, per this codebase's module-layering convention.
void forwardRealFft(const std::vector<float>& samples, std::vector<std::complex<float>>& spectrum) {
    const std::size_t n = samples.size();
    spectrum.resize(n / 2 + 1);
    const pocketfft::shape_t shape{n};
    const pocketfft::stride_t strideIn{sizeof(float)};
    const pocketfft::stride_t strideOut{sizeof(std::complex<float>)};
    pocketfft::r2c(shape, strideIn, strideOut, std::size_t{0}, pocketfft::FORWARD, samples.data(), spectrum.data(), 1.0f);
}

[[nodiscard]] double energy(const std::vector<float>& signal) noexcept {
    double sum = 0.0;
    for (const float sample : signal) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return sum;
}

[[nodiscard]] float snrFromEnergies(double signalEnergy, double errorEnergy) noexcept {
    if (signalEnergy <= 0.0) {
        return 0.0f;
    }
    const double ratio = signalEnergy / std::max(errorEnergy, kMinErrorEnergy);
    const float db = static_cast<float>(10.0 * std::log10(ratio));
    return std::min(db, kSnrCapDb);
}

}  // namespace

float correlation(const std::vector<float>& a, const std::vector<float>& b) {
    const std::size_t n = std::min(a.size(), b.size());
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
        normA += static_cast<double>(a[i]) * static_cast<double>(a[i]);
        normB += static_cast<double>(b[i]) * static_cast<double>(b[i]);
    }
    if (normA <= 0.0 || normB <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(dot / std::sqrt(normA * normB));
}

float rootMeanSquare(const std::vector<float>& signal) {
    if (signal.empty()) {
        return 0.0f;
    }
    return static_cast<float>(std::sqrt(energy(signal) / static_cast<double>(signal.size())));
}

float peakAbsolute(const std::vector<float>& signal) {
    float peak = 0.0f;
    for (const float sample : signal) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

float signalToNoiseRatioDb(const std::vector<float>& original, const std::vector<float>& decoded) {
    const std::size_t n = std::min(original.size(), decoded.size());
    double signalEnergy = 0.0;
    double errorEnergy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        signalEnergy += static_cast<double>(original[i]) * static_cast<double>(original[i]);
        const double error = static_cast<double>(original[i]) - static_cast<double>(decoded[i]);
        errorEnergy += error * error;
    }
    return snrFromEnergies(signalEnergy, errorEnergy);
}

BandSnrDb bandSignalToNoiseRatioDb(const std::vector<float>& original, const std::vector<float>& decoded,
                                    std::uint32_t sampleRateHz) {
    const std::size_t n = std::min(original.size(), decoded.size());
    BandSnrDb result;
    if (n == 0 || sampleRateHz == 0) {
        return result;
    }

    std::vector<float> trimmedOriginal(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(n));
    std::vector<float> error(n);
    for (std::size_t i = 0; i < n; ++i) {
        error[i] = original[i] - decoded[i];
    }

    std::vector<std::complex<float>> originalSpectrum;
    std::vector<std::complex<float>> errorSpectrum;
    forwardRealFft(trimmedOriginal, originalSpectrum);
    forwardRealFft(error, errorSpectrum);

    constexpr float kLowHighHz = 250.0f;
    constexpr float kMidHighHz = 4000.0f;
    const float hzPerBin = static_cast<float>(sampleRateHz) / static_cast<float>(n);

    double lowSignal = 0.0;
    double lowError = 0.0;
    double midSignal = 0.0;
    double midError = 0.0;
    double highSignal = 0.0;
    double highError = 0.0;

    for (std::size_t k = 0; k < originalSpectrum.size(); ++k) {
        const float frequencyHz = static_cast<float>(k) * hzPerBin;
        const double signalMagSq = std::norm(originalSpectrum[k]);
        const double errorMagSq = std::norm(errorSpectrum[k]);
        if (frequencyHz < kLowHighHz) {
            lowSignal += signalMagSq;
            lowError += errorMagSq;
        } else if (frequencyHz < kMidHighHz) {
            midSignal += signalMagSq;
            midError += errorMagSq;
        } else {
            highSignal += signalMagSq;
            highError += errorMagSq;
        }
    }

    result.lowDb = snrFromEnergies(lowSignal, lowError);
    result.midDb = snrFromEnergies(midSignal, midError);
    result.highDb = snrFromEnergies(highSignal, highError);
    return result;
}

}  // namespace sound_mind::codec_eval
