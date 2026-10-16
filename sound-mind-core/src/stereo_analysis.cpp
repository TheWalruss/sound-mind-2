#include "sound_mind/core/stereo_analysis.h"
#include "sound_mind/codec/parallel.h"

#include <algorithm>
#include <cmath>

#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief Sums over a window: L*L, R*R, L*R.
struct Sums {
    double ll = 0.0;
    double rr = 0.0;
    double lr = 0.0;
};

double correlationOf(const Sums& sums) {
    const double denominator = std::sqrt(sums.ll * sums.rr);
    return denominator > 1e-20 ? std::clamp(sums.lr / denominator, -1.0, 1.0) : 1.0;
}

/// @brief Width from mid/side energies: E_mid = (ll + rr + 2lr)/4, E_side =
/// (ll + rr - 2lr)/4.
double widthOf(const Sums& sums) {
    const double mid = std::sqrt(std::max(0.0, (sums.ll + sums.rr + 2.0 * sums.lr) / 4.0));
    const double side = std::sqrt(std::max(0.0, (sums.ll + sums.rr - 2.0 * sums.lr) / 4.0));
    return mid + side > 1e-20 ? side / (mid + side) : 0.0;
}

}  // namespace

StereoMeasurement measureStereo(const sound_mind::codec::AudioBuffer& audio) {
    StereoMeasurement result;
    const std::size_t count = audio.frameCount();
    if (count == 0 || audio.right.size() != count || audio.sampleRateHz == 0) {
        return result;
    }
    std::vector<Sums> prefix(count + 1);
    for (std::size_t i = 0; i < count; ++i) {
        const double l = audio.left[i];
        const double r = audio.right[i];
        prefix[i + 1] = Sums{prefix[i].ll + l * l, prefix[i].rr + r * r, prefix[i].lr + l * r};
    }
    const auto window = [&](std::size_t start, std::size_t end) {
        return Sums{prefix[end].ll - prefix[start].ll, prefix[end].rr - prefix[start].rr,
                    prefix[end].lr - prefix[start].lr};
    };
    result.correlation = correlationOf(prefix[count]);
    result.width = widthOf(prefix[count]);

    const double rate = static_cast<double>(audio.sampleRateHz);
    const auto block = static_cast<std::size_t>(std::lround(0.4 * rate));
    const auto step = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(kLoudnessStepSeconds * rate)));
    for (std::size_t end = step; end <= count; end += step) {
        const Sums sums = window(end > block ? end - block : 0, end);
        result.correlationOverTime.push_back(static_cast<float>(correlationOf(sums)));
        result.widthOverTime.push_back(static_cast<float>(widthOf(sums)));
    }
    return result;
}

std::vector<float> bandStereoWidths(const sound_mind::codec::StreamImage& content) {
    const auto& bands = loudnessBands();
    std::vector<float> widths(bands.size(), 0.0f);
    // Each bin's mid/side energy is summed on its own (bins in parallel,
    // v0.1.41.4), then the bins are added up in order - the same result
    // whatever the thread count.
    const std::uint32_t binCount = content.config.binCount;
    std::vector<double> binMid(binCount, 0.0);
    std::vector<double> binSide(binCount, 0.0);
    sound_mind::codec::parallelFor(binCount, 8, [&](std::size_t begin, std::size_t end) {
        for (auto bin = static_cast<std::uint32_t>(begin); bin < end; ++bin) {
            const float frequencyHz = binIndexToFrequency(static_cast<float>(bin), content.config);
            const float weight = sound_mind::codec::aWeightingOffsetDb(frequencyHz);
            double mid = 0.0;
            double side = 0.0;
            for (std::uint32_t frame = 0; frame < content.frameCount; ++frame) {
                const std::size_t cell = cellIndex(bin, frame, content.frameCount);
                const double left = std::pow(10.0, (content.leftMagnitudeDb[cell] - weight) / 20.0);
                const double right = std::pow(10.0, (content.rightMagnitudeDb[cell] - weight) / 20.0);
                mid += 0.25 * (left + right) * (left + right);
                side += 0.25 * (left - right) * (left - right);
            }
            binMid[bin] = mid;
            binSide[bin] = side;
        }
    });
    for (std::size_t band = 0; band < bands.size(); ++band) {
        double midEnergy = 0.0;
        double sideEnergy = 0.0;
        for (std::uint32_t bin = 0; bin < binCount; ++bin) {
            const float frequencyHz = binIndexToFrequency(static_cast<float>(bin), content.config);
            if (frequencyHz < bands[band].lowHz || frequencyHz >= bands[band].highHz) {
                continue;
            }
            midEnergy += binMid[bin];
            sideEnergy += binSide[bin];
        }
        const double mid = std::sqrt(midEnergy);
        const double side = std::sqrt(sideEnergy);
        widths[band] = mid + side > 1e-20 ? static_cast<float>(side / (mid + side)) : 0.0f;
    }
    return widths;
}

}  // namespace sound_mind::core
