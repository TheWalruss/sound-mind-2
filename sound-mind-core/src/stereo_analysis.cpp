#include "sound_mind/core/stereo_analysis.h"

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
    for (std::size_t band = 0; band < bands.size(); ++band) {
        double midEnergy = 0.0;
        double sideEnergy = 0.0;
        for (std::uint32_t bin = 0; bin < content.config.binCount; ++bin) {
            const float frequencyHz = binIndexToFrequency(static_cast<float>(bin), content.config);
            if (frequencyHz < bands[band].lowHz || frequencyHz >= bands[band].highHz) {
                continue;
            }
            const float weight = sound_mind::codec::aWeightingOffsetDb(frequencyHz);
            for (std::uint32_t frame = 0; frame < content.frameCount; ++frame) {
                const std::size_t cell = cellIndex(bin, frame, content.frameCount);
                const double left = std::pow(10.0, (content.leftMagnitudeDb[cell] - weight) / 20.0);
                const double right = std::pow(10.0, (content.rightMagnitudeDb[cell] - weight) / 20.0);
                midEnergy += 0.25 * (left + right) * (left + right);
                sideEnergy += 0.25 * (left - right) * (left - right);
            }
        }
        const double mid = std::sqrt(midEnergy);
        const double side = std::sqrt(sideEnergy);
        widths[band] = mid + side > 1e-20 ? static_cast<float>(side / (mid + side)) : 0.0f;
    }
    return widths;
}

}  // namespace sound_mind::core
