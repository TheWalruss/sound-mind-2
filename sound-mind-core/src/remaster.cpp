#include "sound_mind/core/remaster.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

bool sameGrid(const sound_mind::codec::StreamCodecConfig& a, const sound_mind::codec::StreamCodecConfig& b) {
    return a.sampleRateHz == b.sampleRateHz && a.hopLength == b.hopLength && a.binCount == b.binCount &&
           a.minFrequencyHz == b.minFrequencyHz && a.maxFrequencyHz == b.maxFrequencyHz;
}

double frameSeconds(const sound_mind::codec::StreamCodecConfig& config) {
    return static_cast<double>(config.hopLength) / static_cast<double>(config.sampleRateHz);
}

int clampIndex(long long value, std::uint32_t count) {
    return static_cast<int>(std::clamp<long long>(value, 0, static_cast<long long>(count) - 1));
}

/// Copies `clip`'s cells into a `frames` x `bins` clip, each new cell taken
/// from the old one the two index maps name.
Clip remap(const Clip& clip, std::uint32_t frames, std::uint32_t bins, const std::vector<int>& oldFrameOf,
           const std::vector<int>& oldBinOf) {
    Clip result;
    result.frameCount = frames;
    result.binCount = bins;
    const std::size_t cells = static_cast<std::size_t>(frames) * bins;
    result.leftMagnitudeDb.resize(cells);
    result.rightMagnitudeDb.resize(cells);
    result.sharedPhaseRadians.resize(cells);
    for (std::uint32_t bin = 0; bin < bins; ++bin) {
        for (std::uint32_t frame = 0; frame < frames; ++frame) {
            const std::size_t to = cellIndex(bin, frame, frames);
            const std::size_t from = cellIndex(static_cast<std::uint32_t>(oldBinOf[bin]),
                                               static_cast<std::uint32_t>(oldFrameOf[frame]), clip.frameCount);
            result.leftMagnitudeDb[to] = clip.leftMagnitudeDb[from];
            result.rightMagnitudeDb[to] = clip.rightMagnitudeDb[from];
            result.sharedPhaseRadians[to] = clip.sharedPhaseRadians[from];
        }
    }
    return result;
}

}  // namespace

ProjectSettings remasterSettings(const ProjectSettings& original, const ProjectSettings& wanted) {
    ProjectSettings settings = wanted;
    const double durationMs = static_cast<double>(original.canvasWidth) * original.timestepMs;
    settings.canvasWidth =
        std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::lround(durationMs / wanted.timestepMs)));
    settings.principalMode = original.principalMode;
    settings.decimalSeparator = original.decimalSeparator;
    return settings;
}

Clip resampleClip(const Clip& clip, const TimeFrequencyRect& placement,
                  const sound_mind::codec::StreamCodecConfig& from, const sound_mind::codec::StreamCodecConfig& to) {
    if (sameGrid(from, to) || clip.frameCount == 0 || clip.binCount == 0) {
        return clip;
    }
    // The same origin rule applyPasteOperation() uses.
    const auto originFrame = [&](const sound_mind::codec::StreamCodecConfig& config) {
        return std::lround(std::min(timeToFrameIndex(placement.startTimeSeconds, config),
                                    timeToFrameIndex(placement.endTimeSeconds, config)));
    };
    const auto originBin = [&](const sound_mind::codec::StreamCodecConfig& config) {
        return std::lround(std::min(frequencyToBinIndex(static_cast<float>(placement.lowFrequencyHz), config),
                                    frequencyToBinIndex(static_cast<float>(placement.highFrequencyHz), config)));
    };
    const long oldFrame0 = originFrame(from);
    const long newFrame0 = originFrame(to);
    const long oldBin0 = originBin(from);
    const long newBin0 = originBin(to);

    const auto frames = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(std::lround(clip.frameCount * frameSeconds(from) / frameSeconds(to))));
    const float topHz = binIndexToFrequency(static_cast<float>(oldBin0 + clip.binCount - 1), from);
    const long newTopBin = std::lround(frequencyToBinIndex(topHz, to));
    const auto bins = static_cast<std::uint32_t>(std::max<long>(1, newTopBin - newBin0 + 1));

    std::vector<int> oldFrameOf(frames);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const double seconds = frameIndexToTime(static_cast<double>(newFrame0 + frame), to);
        oldFrameOf[frame] = clampIndex(std::llround(timeToFrameIndex(seconds, from)) - oldFrame0, clip.frameCount);
    }
    std::vector<int> oldBinOf(bins);
    for (std::uint32_t bin = 0; bin < bins; ++bin) {
        const float hz = binIndexToFrequency(static_cast<float>(newBin0 + bin), to);
        oldBinOf[bin] = clampIndex(std::llround(frequencyToBinIndex(hz, from)) - oldBin0, clip.binCount);
    }
    return remap(clip, frames, bins, oldFrameOf, oldBinOf);
}

Clip resampleClipProportionally(const Clip& clip, const sound_mind::codec::StreamCodecConfig& from,
                                const sound_mind::codec::StreamCodecConfig& to) {
    if (sameGrid(from, to) || clip.frameCount == 0 || clip.binCount == 0) {
        return clip;
    }
    const double timeRatio = frameSeconds(from) / frameSeconds(to);
    const double binRatio = static_cast<double>(to.binCount) / static_cast<double>(from.binCount);
    const auto frames = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::lround(clip.frameCount * timeRatio)));
    const auto bins = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::lround(clip.binCount * binRatio)));
    std::vector<int> oldFrameOf(frames);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        oldFrameOf[frame] = clampIndex(std::llround(frame / timeRatio), clip.frameCount);
    }
    std::vector<int> oldBinOf(bins);
    for (std::uint32_t bin = 0; bin < bins; ++bin) {
        oldBinOf[bin] = clampIndex(std::llround(bin / binRatio), clip.binCount);
    }
    return remap(clip, frames, bins, oldFrameOf, oldBinOf);
}

nlohmann::json resampleSelectionJson(const nlohmann::json& boundary, const sound_mind::codec::StreamCodecConfig& from,
                                     const sound_mind::codec::StreamCodecConfig& to) {
    if (!boundary.is_object() || boundary.value("kind", "") != "mask" || sameGrid(from, to)) {
        return boundary;
    }
    const int frameLow = boundary.at("frameLow").get<int>();
    const int frameHigh = boundary.at("frameHigh").get<int>();
    const int binLow = boundary.at("binLow").get<int>();
    const int binHigh = boundary.at("binHigh").get<int>();
    const auto mask = boundary.at("mask").get<std::vector<bool>>();
    const int oldWidth = frameHigh - frameLow + 1;
    const int oldHeight = binHigh - binLow + 1;
    if (oldWidth <= 0 || oldHeight <= 0) {
        return boundary;
    }
    const auto newFrame = [&](int frame) {
        return static_cast<int>(std::lround(timeToFrameIndex(frameIndexToTime(frame, from), to)));
    };
    const auto newBin = [&](int bin) {
        return static_cast<int>(std::lround(frequencyToBinIndex(binIndexToFrequency(static_cast<float>(bin), from), to)));
    };
    const int newFrameLow = newFrame(frameLow);
    const int newFrameHigh = std::max(newFrameLow, newFrame(frameHigh));
    const int newBinLow = newBin(binLow);
    const int newBinHigh = std::max(newBinLow, newBin(binHigh));
    const int width = newFrameHigh - newFrameLow + 1;
    const int height = newBinHigh - newBinLow + 1;
    std::vector<bool> resampled(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), false);
    for (int bin = 0; bin < height; ++bin) {
        const float hz = binIndexToFrequency(static_cast<float>(newBinLow + bin), to);
        const int oldBin = std::clamp(static_cast<int>(std::lround(frequencyToBinIndex(hz, from))) - binLow, 0,
                                      oldHeight - 1);
        for (int frame = 0; frame < width; ++frame) {
            const double seconds = frameIndexToTime(newFrameLow + frame, to);
            const int oldFrame = std::clamp(static_cast<int>(std::lround(timeToFrameIndex(seconds, from))) - frameLow,
                                            0, oldWidth - 1);
            resampled[static_cast<std::size_t>(bin) * width + frame] =
                mask[static_cast<std::size_t>(oldBin) * oldWidth + oldFrame];
        }
    }
    nlohmann::json result = boundary;
    result["frameLow"] = newFrameLow;
    result["frameHigh"] = newFrameHigh;
    result["binLow"] = newBinLow;
    result["binHigh"] = newBinHigh;
    result["mask"] = resampled;
    return result;
}

void adaptEntryForRemaster(HistoryEntry& entry, const ProjectSettings& fromSettings, const ProjectSettings& toSettings) {
    const auto from = streamCodecConfigFor(fromSettings);
    const auto to = streamCodecConfigFor(toSettings);
    if (sameGrid(from, to)) {
        return;
    }
    const double frameRatio = frameSeconds(from) / frameSeconds(to);
    for (HistoryLayerChange& change : entry.layers) {
        for (nlohmann::json* side : {&change.before, &change.after}) {
            if (side->is_object() && side->contains("translationColumns")) {
                const auto columns = (*side)["translationColumns"].get<std::int64_t>();
                (*side)["translationColumns"] = static_cast<std::int64_t>(std::llround(columns * frameRatio));
            }
        }
    }
    for (nlohmann::json& operation : entry.appendedOperations) {
        if (operation.contains("boundary")) {
            operation["boundary"] = resampleSelectionJson(operation["boundary"], from, to);
        }
        if (operation.value("kind", "") == "paste" && operation.contains("clip")) {
            const Clip clip = operation["clip"].get<Clip>();
            const auto placement = operation["placement"].get<TimeFrequencyRect>();
            operation["clip"] = resampleClip(clip, placement, from, to);
        }
    }
    for (HistoryResourceChange& resource : entry.resources) {
        if (resource.collection != "mindShots") {
            continue;
        }
        for (nlohmann::json* side : {&resource.before, &resource.after}) {
            if (!side->is_array()) {
                continue;
            }
            for (nlohmann::json& shot : *side) {
                if (shot.contains("clip")) {
                    shot["clip"] = resampleClipProportionally(shot["clip"].get<Clip>(), from, to);
                }
            }
        }
    }
}

}  // namespace sound_mind::core
