#include "sound_mind/core/text_paste.h"

#include <algorithm>
#include <cmath>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/selection_region.h"

namespace sound_mind::core {

std::unique_ptr<PasteOperation> makeTextPaste(OperationId id, LayerId layer, const GlyphRaster& raster,
                                              int frameOrigin, int binOrigin, float leftIntensityDb,
                                              float rightIntensityDb, float opacity, BlendMode blendMode,
                                              const sound_mind::codec::StreamCodecConfig& config) {
    const std::size_t cells = static_cast<std::size_t>(raster.width) * raster.height;
    if (raster.width == 0 || raster.height == 0 || raster.coverage.size() < cells) {
        return nullptr;
    }
    Clip clip;
    clip.frameCount = raster.width;
    clip.binCount = raster.height;
    clip.leftMagnitudeDb.assign(cells, kTextSilenceDb);
    clip.rightMagnitudeDb.assign(cells, kTextSilenceDb);
    clip.sharedPhaseRadians.assign(cells, 0.0f);
    std::vector<bool> mask(cells, false);
    bool any = false;
    const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
    for (std::uint32_t row = 0; row < raster.height; ++row) {
        // Image row 0 is the top - the highest bin.
        const std::uint32_t bin = raster.height - 1 - row;
        for (std::uint32_t column = 0; column < raster.width; ++column) {
            const float level =
                std::clamp(raster.coverage[static_cast<std::size_t>(row) * raster.width + column], 0.0f, 1.0f) *
                clampedOpacity;
            if (level < kTextCoverageThreshold) {
                continue;
            }
            const std::size_t index = cellIndex(bin, column, raster.width);
            const float gainDb = 20.0f * std::log10(level);
            clip.leftMagnitudeDb[index] = leftIntensityDb + gainDb;
            clip.rightMagnitudeDb[index] = rightIntensityDb + gainDb;
            mask[index] = true;
            any = true;
        }
    }
    if (!any) {
        return nullptr;
    }

    const int frameHigh = frameOrigin + static_cast<int>(raster.width) - 1;
    const int binHigh = binOrigin + static_cast<int>(raster.height) - 1;
    TimeFrequencyRect placement;
    placement.startTimeSeconds = frameIndexToTime(static_cast<double>(frameOrigin), config);
    placement.endTimeSeconds = frameIndexToTime(static_cast<double>(frameHigh), config);
    placement.lowFrequencyHz = binIndexToFrequency(static_cast<float>(binOrigin), config);
    placement.highFrequencyHz = binIndexToFrequency(static_cast<float>(binHigh), config);
    return std::make_unique<PasteOperation>(id, layer, placement, std::move(clip), std::nullopt,
                                            SelectionRegion(frameOrigin, frameHigh, binOrigin, binHigh, std::move(mask)),
                                            blendMode);
}

}  // namespace sound_mind::core
