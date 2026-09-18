#include "sound_mind/core/fill_application.h"

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

void applyFillOperation(const FillOperation& operation, sound_mind::codec::StreamImage& content) {
    if (content.frameCount == 0 || content.config.binCount == 0) {
        return;
    }

    const FrameBinRange range = rangeFor(operation.bounds(), content.config, content.frameCount);
    const std::optional<Path>& boundary = operation.boundary();

    for (int frame = range.frameLow; frame <= range.frameHigh; ++frame) {
        // t runs left-to-right across the selection's own time axis - see
        // this function's own docs for why that direction, specifically.
        const float t = (range.frameHigh > range.frameLow) ? static_cast<float>(frame - range.frameLow) /
                                                                   static_cast<float>(range.frameHigh - range.frameLow)
                                                             : 0.0f;
        const GradientStop target = operation.gradient().evaluate(t);

        for (int bin = range.binLow; bin <= range.binHigh; ++bin) {
            // A Lasso-shaped fill narrows to cells actually inside its own
            // boundary() - see FillOperation's own docs. Absent, every
            // cell in bounds() is touched, unchanged from before this
            // field existed.
            if (boundary) {
                const TimeFrequencyPoint cellPoint{frameIndexToTime(static_cast<double>(frame), content.config),
                                                    static_cast<double>(binIndexToFrequency(
                                                        static_cast<float>(bin), content.config))};
                if (!containsPoint(*boundary, cellPoint)) {
                    continue;
                }
            }

            const std::size_t index = cellIndex(bin, frame, content.frameCount);
            blendTowardStop(content.leftMagnitudeDb[index], content.rightMagnitudeDb[index], target);
        }
    }
}

}  // namespace sound_mind::core
