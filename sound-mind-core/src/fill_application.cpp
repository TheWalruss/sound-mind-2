#include "sound_mind/core/fill_application.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

void applyFillOperation(const FillOperation& operation, sound_mind::codec::StreamImage& content) {
    if (content.frameCount == 0 || content.config.binCount == 0) {
        return;
    }

    const TimeFrequencyRect bounds = operation.bounds();
    const double frameLowD = timeToFrameIndex(bounds.startTimeSeconds, content.config);
    const double frameHighD = timeToFrameIndex(bounds.endTimeSeconds, content.config);
    // The frequency axis is log-scaled (see frequencyToBinIndex()'s own
    // docs), but its own direction still agrees with Hz - lowFrequencyHz
    // always maps to the smaller bin index - so no separate min/max
    // ordering is needed here beyond what std::minmax already gives.
    const float binAtLow = frequencyToBinIndex(static_cast<float>(bounds.lowFrequencyHz), content.config);
    const float binAtHigh = frequencyToBinIndex(static_cast<float>(bounds.highFrequencyHz), content.config);

    const int frameLow = std::clamp(static_cast<int>(std::round(std::min(frameLowD, frameHighD))), 0,
                                     static_cast<int>(content.frameCount) - 1);
    const int frameHigh = std::clamp(static_cast<int>(std::round(std::max(frameLowD, frameHighD))), 0,
                                      static_cast<int>(content.frameCount) - 1);
    const int binLow = std::clamp(static_cast<int>(std::round(std::min(binAtLow, binAtHigh))), 0,
                                   static_cast<int>(content.config.binCount) - 1);
    const int binHigh = std::clamp(static_cast<int>(std::round(std::max(binAtLow, binAtHigh))), 0,
                                    static_cast<int>(content.config.binCount) - 1);

    for (int frame = frameLow; frame <= frameHigh; ++frame) {
        // t runs left-to-right across the selection's own time axis - see
        // this function's own docs for why that direction, specifically.
        const float t = (frameHigh > frameLow)
                             ? static_cast<float>(frame - frameLow) / static_cast<float>(frameHigh - frameLow)
                             : 0.0f;
        const GradientStop target = operation.gradient().evaluate(t);

        for (int bin = binLow; bin <= binHigh; ++bin) {
            const std::size_t index = static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
            float& left = content.leftMagnitudeDb[index];
            float& right = content.rightMagnitudeDb[index];
            left += (target.leftIntensity - left) * target.leftOpacity;
            right += (target.rightIntensity - right) * target.rightOpacity;
        }
    }
}

}  // namespace sound_mind::core
