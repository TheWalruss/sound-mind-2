#include "sound_mind/core/selection_region.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief Whether `(bin, frame)` falls within `region` - `region ==
/// std::nullopt` means "a plain rectangle", true for every cell within
/// `rangeIfAbsent` - the shared per-cell test `combine()` runs for each of
/// its two operands.
bool containsCellOrRect(const std::optional<SelectionRegion>& region, FrameBinRange rangeIfAbsent, int bin, int frame,
                         const sound_mind::codec::StreamCodecConfig& config) {
    if (region) {
        return region->containsCell(bin, frame, config);
    }
    return frame >= rangeIfAbsent.frameLow && frame <= rangeIfAbsent.frameHigh && bin >= rangeIfAbsent.binLow &&
           bin <= rangeIfAbsent.binHigh;
}

}  // namespace

bool SelectionRegion::containsCell(int bin, int frame, const sound_mind::codec::StreamCodecConfig& config) const {
    if (kind_ == SelectionRegionKind::Path) {
        const TimeFrequencyPoint point{frameIndexToTime(static_cast<double>(frame), config),
                                        static_cast<double>(binIndexToFrequency(static_cast<float>(bin), config))};
        return containsPoint(path_, point);
    }
    if (frame < frameLow_ || frame > frameHigh_ || bin < binLow_ || bin > binHigh_) {
        return false;
    }
    const auto frameSpan = static_cast<std::size_t>(frameHigh_ - frameLow_ + 1);
    const auto index =
        static_cast<std::size_t>(bin - binLow_) * frameSpan + static_cast<std::size_t>(frame - frameLow_);
    return index < mask_.size() && mask_[index];
}

bool SelectionRegion::contains(TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config) const {
    if (kind_ == SelectionRegionKind::Path) {
        return containsPoint(path_, point);
    }
    const int frame = static_cast<int>(std::lround(timeToFrameIndex(point.timeSeconds, config)));
    const int bin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(static_cast<float>(point.frequencyHz), config))));
    return containsCell(bin, frame, config);
}

SelectionRegion SelectionRegion::translated(double deltaTimeSeconds, double deltaFrequencyBins,
                                              const sound_mind::codec::StreamCodecConfig& config) const {
    if (kind_ == SelectionRegionKind::Path) {
        return SelectionRegion(path_.translated(deltaTimeSeconds, deltaFrequencyBins, config));
    }
    const int frameDelta =
        static_cast<int>(std::lround(deltaTimeSeconds * config.sampleRateHz / static_cast<double>(config.hopLength)));
    const int binDelta = static_cast<int>(std::lround(deltaFrequencyBins));
    return SelectionRegion(frameLow_ + frameDelta, frameHigh_ + frameDelta, binLow_ + binDelta, binHigh_ + binDelta,
                            mask_);
}

SelectionRegion SelectionRegion::combine(const std::optional<SelectionRegion>& a, FrameBinRange aRange,
                                          const std::optional<SelectionRegion>& b, FrameBinRange bRange,
                                          FrameBinRange resultRange, BooleanOp op,
                                          const sound_mind::codec::StreamCodecConfig& config) {
    if (resultRange.frameHigh < resultRange.frameLow || resultRange.binHigh < resultRange.binLow) {
        return SelectionRegion(resultRange.frameLow, resultRange.frameHigh, resultRange.binLow, resultRange.binHigh,
                                std::vector<bool>{});
    }

    const auto frameSpan = static_cast<std::size_t>(resultRange.frameHigh - resultRange.frameLow + 1);
    const auto binSpan = static_cast<std::size_t>(resultRange.binHigh - resultRange.binLow + 1);
    std::vector<bool> mask(frameSpan * binSpan, false);

    for (int bin = resultRange.binLow; bin <= resultRange.binHigh; ++bin) {
        for (int frame = resultRange.frameLow; frame <= resultRange.frameHigh; ++frame) {
            const bool inA = containsCellOrRect(a, aRange, bin, frame, config);
            const bool inB = containsCellOrRect(b, bRange, bin, frame, config);
            bool selected = false;
            switch (op) {
                case BooleanOp::Add:
                    selected = inA || inB;
                    break;
                case BooleanOp::Subtract:
                    selected = inA && !inB;
                    break;
                case BooleanOp::Intersect:
                    selected = inA && inB;
                    break;
            }
            if (selected) {
                const auto index =
                    static_cast<std::size_t>(bin - resultRange.binLow) * frameSpan +
                    static_cast<std::size_t>(frame - resultRange.frameLow);
                mask[index] = true;
            }
        }
    }

    return SelectionRegion(resultRange.frameLow, resultRange.frameHigh, resultRange.binLow, resultRange.binHigh,
                            std::move(mask));
}

void to_json(nlohmann::json& json, const SelectionRegion& region) {
    if (region.kind() == SelectionRegionKind::Path) {
        json = nlohmann::json{{"kind", "path"}, {"path", region.path()}};
    } else {
        json = nlohmann::json{{"kind", "mask"},
                               {"frameLow", region.maskFrameLow()},
                               {"frameHigh", region.maskFrameHigh()},
                               {"binLow", region.maskBinLow()},
                               {"binHigh", region.maskBinHigh()},
                               {"mask", region.maskCells()}};
    }
}

void from_json(const nlohmann::json& json, SelectionRegion& region) {
    const std::string kind = json.at("kind").get<std::string>();
    if (kind == "path") {
        region = SelectionRegion(json.at("path").get<Path>());
    } else if (kind == "mask") {
        region = SelectionRegion(json.at("frameLow").get<int>(), json.at("frameHigh").get<int>(),
                                  json.at("binLow").get<int>(), json.at("binHigh").get<int>(),
                                  json.at("mask").get<std::vector<bool>>());
    } else {
        throw std::invalid_argument("SelectionRegion: unrecognized kind \"" + kind + "\"");
    }
}

}  // namespace sound_mind::core
