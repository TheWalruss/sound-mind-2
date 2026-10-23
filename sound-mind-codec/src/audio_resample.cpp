#include "sound_mind/codec/audio_resample.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "sound_mind/codec/parallel.h"

namespace sound_mind::codec {

namespace {

/// Taps each side of the centre, counted at the lower of the two rates.
constexpr int kHalfTaps = 64;
/// Kernel table entries per tap, linearly interpolated between.
constexpr int kTableResolution = 512;
/// The cutoff as a fraction of the lower rate's Nyquist limit - leaves room
/// for the transition band below it.
constexpr double kCutoffFraction = 0.94;
/// The Kaiser window's shape: about 80 dB of stopband attenuation.
constexpr double kKaiserBeta = 8.0;

/// The zeroth-order modified Bessel function of the first kind.
double besselI0(double x) {
    double sum = 1.0;
    double term = 1.0;
    const double quarterSquare = x * x / 4.0;
    for (int k = 1; k < 64; ++k) {
        term *= quarterSquare / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term < sum * 1e-17) {
            break;
        }
    }
    return sum;
}

/// The windowed-sinc kernel at distances 0..kHalfTaps (in lower-rate
/// samples), kTableResolution entries per sample, plus one guard entry.
std::vector<double> kernelTable() {
    const double cutoff = 0.5 * kCutoffFraction;  // cycles per lower-rate sample.
    const double windowNorm = besselI0(kKaiserBeta);
    std::vector<double> table(static_cast<std::size_t>(kHalfTaps * kTableResolution + 2), 0.0);
    for (std::size_t i = 0; i + 1 < table.size(); ++i) {
        const double u = static_cast<double>(i) / kTableResolution;
        if (u > kHalfTaps) {
            break;
        }
        const double x = 2.0 * cutoff * u;
        const double sinc = x == 0.0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
        const double r = u / kHalfTaps;
        const double window = besselI0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - r * r))) / windowNorm;
        table[i] = 2.0 * cutoff * sinc * window;
    }
    return table;
}

const std::vector<double>& kernel() {
    static const std::vector<double> table = kernelTable();
    return table;
}

}  // namespace

AudioBuffer resample(const AudioBuffer& audio, std::uint32_t targetRateHz) {
    if (audio.sampleRateHz == targetRateHz || audio.sampleRateHz == 0 || targetRateHz == 0) {
        return audio;
    }
    AudioBuffer result;
    result.sampleRateHz = targetRateHz;
    const std::size_t inFrames = audio.frameCount();
    if (inFrames == 0) {
        return result;
    }
    const double step = static_cast<double>(audio.sampleRateHz) / static_cast<double>(targetRateHz);  // input per output.
    const auto outFrames = static_cast<std::size_t>(
        std::llround(static_cast<double>(inFrames) * static_cast<double>(targetRateHz) / audio.sampleRateHz));
    result.left.assign(outFrames, 0.0f);
    result.right.assign(outFrames, 0.0f);

    // Distances are measured in the lower rate's samples; converting down
    // stretches the kernel over more input samples, lowering its cutoff to
    // the new rate's limit.
    const double scale = std::min(1.0, 1.0 / step);
    const double reach = kHalfTaps / scale;  // input samples each side.
    const std::vector<double>& table = kernel();
    const bool hasRight = audio.right.size() == inFrames;

    parallelFor(outFrames, 4096, [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const double t = static_cast<double>(i) * step;  // the output sample's time, in input samples.
            const auto first = static_cast<std::ptrdiff_t>(std::max(0.0, std::ceil(t - reach)));
            const auto last = static_cast<std::ptrdiff_t>(
                std::min(static_cast<double>(inFrames - 1), std::floor(t + reach)));
            double left = 0.0;
            double right = 0.0;
            for (std::ptrdiff_t j = first; j <= last; ++j) {
                const double position = std::abs(t - static_cast<double>(j)) * scale * kTableResolution;
                const auto index = static_cast<std::size_t>(position);
                if (index + 1 >= table.size()) {
                    continue;
                }
                const double fraction = position - static_cast<double>(index);
                const double weight = table[index] + (table[index + 1] - table[index]) * fraction;
                left += weight * audio.left[static_cast<std::size_t>(j)];
                if (hasRight) {
                    right += weight * audio.right[static_cast<std::size_t>(j)];
                }
            }
            result.left[i] = static_cast<float>(left * scale);
            result.right[i] = hasRight ? static_cast<float>(right * scale) : result.left[i];
        }
    });
    return result;
}

}  // namespace sound_mind::codec
