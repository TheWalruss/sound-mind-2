#include "sound_mind/core/paste_application.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "sound_mind/core/blend_mode_application.h"
#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

Clip captureClip(const sound_mind::codec::StreamImage& source, const TimeFrequencyRect& bounds) {
    Clip clip;
    if (source.frameCount == 0 || source.config.binCount == 0) {
        return clip;
    }

    const FrameBinRange range = rangeFor(bounds, source.config, source.frameCount);
    if (range.frameHigh < range.frameLow || range.binHigh < range.binLow) {
        return clip;
    }

    clip.frameCount = static_cast<std::uint32_t>(range.frameHigh - range.frameLow + 1);
    clip.binCount = static_cast<std::uint32_t>(range.binHigh - range.binLow + 1);
    const std::size_t cellCount = static_cast<std::size_t>(clip.frameCount) * clip.binCount;
    clip.leftMagnitudeDb.reserve(cellCount);
    clip.rightMagnitudeDb.reserve(cellCount);
    clip.sharedPhaseRadians.reserve(cellCount);

    for (int bin = range.binLow; bin <= range.binHigh; ++bin) {
        for (int frame = range.frameLow; frame <= range.frameHigh; ++frame) {
            const std::size_t index = cellIndex(bin, frame, source.frameCount);
            clip.leftMagnitudeDb.push_back(source.leftMagnitudeDb[index]);
            clip.rightMagnitudeDb.push_back(source.rightMagnitudeDb[index]);
            clip.sharedPhaseRadians.push_back(source.sharedPhaseRadians[index]);
        }
    }

    return clip;
}

void applyPasteOperation(const PasteOperation& operation, sound_mind::codec::StreamImage& content) {
    const Clip& clip = operation.clip();
    if (content.frameCount == 0 || content.config.binCount == 0 || clip.frameCount == 0 || clip.binCount == 0) {
        return;
    }

    const TimeFrequencyRect placement = operation.bounds();
    const double frameOriginD = std::min(timeToFrameIndex(placement.startTimeSeconds, content.config),
                                          timeToFrameIndex(placement.endTimeSeconds, content.config));
    const float binAtLow = frequencyToBinIndex(static_cast<float>(placement.lowFrequencyHz), content.config);
    const float binAtHigh = frequencyToBinIndex(static_cast<float>(placement.highFrequencyHz), content.config);
    const int frameOrigin = static_cast<int>(std::round(frameOriginD));
    const int binOrigin = static_cast<int>(std::round(std::min(binAtLow, binAtHigh)));

    const std::optional<SelectionRegion>& boundary = operation.boundary();

    for (std::uint32_t clipBin = 0; clipBin < clip.binCount; ++clipBin) {
        const int destBin = binOrigin + static_cast<int>(clipBin);
        if (destBin < 0 || destBin >= static_cast<int>(content.config.binCount)) {
            continue;  // Falls outside the destination's own bin range - silently clipped.
        }
        for (std::uint32_t clipFrame = 0; clipFrame < clip.frameCount; ++clipFrame) {
            const int destFrame = frameOrigin + static_cast<int>(clipFrame);
            if (destFrame < 0 || destFrame >= static_cast<int>(content.frameCount)) {
                continue;  // Falls outside the destination's own frame range - silently clipped.
            }

            // A non-rectangular paste narrows to cells actually inside its
            // own boundary() - see PasteOperation's own docs. Absent,
            // every clip cell is written, unchanged from before this field
            // existed.
            if (boundary && !boundary->containsCell(destBin, destFrame, content.config)) {
                continue;
            }

            const std::size_t clipIndex = cellIndex(clipBin, clipFrame, clip.frameCount);
            const std::size_t destIndex = cellIndex(destBin, destFrame, content.frameCount);
            const BlendedCell base{content.leftMagnitudeDb[destIndex], content.rightMagnitudeDb[destIndex],
                                    content.sharedPhaseRadians[destIndex]};
            const BlendedCell overlay{clip.leftMagnitudeDb[clipIndex], clip.rightMagnitudeDb[clipIndex],
                                       clip.sharedPhaseRadians[clipIndex]};
            const BlendedCell blended = applyBlendedCell(operation.blendMode(), base, overlay, 1.0f);
            content.leftMagnitudeDb[destIndex] = blended.leftMagnitudeDb;
            content.rightMagnitudeDb[destIndex] = blended.rightMagnitudeDb;
            content.sharedPhaseRadians[destIndex] = blended.phaseRadians;
        }
    }
}

}  // namespace sound_mind::core
