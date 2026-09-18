#include "sound_mind/core/wand_selection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief This codebase's own `-96..0` dB display range (matching every
/// other file's own local copy of this convention - see
/// `docs/sound-mind-architecture.md`'s Build & Module Layout on why no
/// single shared constant exists to reuse instead).
constexpr double kDisplayMinDb = -96.0;
constexpr double kDisplayMaxDb = 0.0;

/// @brief A harmonic whose own starting cell is at or below this level has
/// no real signal there - skipped rather than flood-filled from near-
/// silence. Slightly above kDisplayMinDb itself, to tolerate a touch of
/// noise floor rather than requiring bit-exact silence.
constexpr double kHarmonicSilenceFloorDb = -90.0;

/// @brief The average of a cell's own left/right channel dB values - the
/// single "amplitude" a Wand selection compares, since it doesn't care
/// about stereo balance, only overall loudness.
double cellAmplitudeDb(const sound_mind::codec::StreamImage& content, int bin, int frame) {
    const std::size_t index = cellIndex(bin, frame, content.frameCount);
    return (static_cast<double>(content.leftMagnitudeDb[index]) + static_cast<double>(content.rightMagnitudeDb[index])) /
           2.0;
}

/// @brief 4-connected flood fill from `(startBin, startFrame)`, comparing
/// every reachable cell against *this call's own* starting cell (not a
/// running average) - see selectByAmplitudeSimilarity()'s own docs. Marks
/// newly-selected cells directly into `selected` (already sized to
/// `binCount * frameCount`); a cell already `true` (from an earlier
/// harmonic's own fill) is left alone and not re-expanded from, so this is
/// safe to call repeatedly into the same accumulator.
void floodFillInto(const sound_mind::codec::StreamImage& content, int startBin, int startFrame, double toleranceDb,
                    std::vector<bool>& selected) {
    const int binCount = static_cast<int>(content.config.binCount);
    const int frameCount = static_cast<int>(content.frameCount);
    if (startBin < 0 || startBin >= binCount || startFrame < 0 || startFrame >= frameCount) {
        return;
    }
    const std::size_t startIndex = cellIndex(startBin, startFrame, content.frameCount);
    if (selected[startIndex]) {
        return;
    }
    const double referenceDb = cellAmplitudeDb(content, startBin, startFrame);

    std::vector<std::pair<int, int>> queue;
    queue.emplace_back(startBin, startFrame);
    selected[startIndex] = true;

    constexpr std::array<std::pair<int, int>, 4> kNeighborOffsets = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    while (!queue.empty()) {
        const auto [bin, frame] = queue.back();
        queue.pop_back();
        for (const auto& [deltaBin, deltaFrame] : kNeighborOffsets) {
            const int neighborBin = bin + deltaBin;
            const int neighborFrame = frame + deltaFrame;
            if (neighborBin < 0 || neighborBin >= binCount || neighborFrame < 0 || neighborFrame >= frameCount) {
                continue;
            }
            const std::size_t neighborIndex = cellIndex(neighborBin, neighborFrame, content.frameCount);
            if (selected[neighborIndex]) {
                continue;
            }
            if (std::abs(cellAmplitudeDb(content, neighborBin, neighborFrame) - referenceDb) > toleranceDb) {
                continue;
            }
            selected[neighborIndex] = true;
            queue.emplace_back(neighborBin, neighborFrame);
        }
    }
}

}  // namespace

SelectionRegion selectByAmplitudeSimilarity(const sound_mind::codec::StreamImage& content, TimeFrequencyPoint anchor,
                                             double tolerancePercent, bool harmonicsAware) {
    const SelectionRegion empty(0, -1, 0, -1, {});
    if (content.frameCount == 0 || content.config.binCount == 0) {
        return empty;
    }

    const int startFrame = static_cast<int>(std::lround(timeToFrameIndex(anchor.timeSeconds, content.config)));
    const int startBin = static_cast<int>(std::lround(
        static_cast<double>(frequencyToBinIndex(static_cast<float>(anchor.frequencyHz), content.config))));
    if (startFrame < 0 || startFrame >= static_cast<int>(content.frameCount) || startBin < 0 ||
        startBin >= static_cast<int>(content.config.binCount)) {
        return empty;
    }

    const double toleranceDb = (tolerancePercent / 100.0) * (kDisplayMaxDb - kDisplayMinDb);
    std::vector<bool> selected(static_cast<std::size_t>(content.config.binCount) * content.frameCount, false);
    floodFillInto(content, startBin, startFrame, toleranceDb, selected);

    if (harmonicsAware) {
        const float fundamentalHz = binIndexToFrequency(static_cast<float>(startBin), content.config);
        const float nyquistHz = static_cast<float>(content.config.sampleRateHz) / 2.0f;
        const float maxFrequencyHz = std::min(content.config.maxFrequencyHz, nyquistHz);
        for (int harmonic = 2; static_cast<float>(harmonic) * fundamentalHz < maxFrequencyHz; ++harmonic) {
            const float harmonicHz = static_cast<float>(harmonic) * fundamentalHz;
            const int harmonicBin = static_cast<int>(
                std::lround(static_cast<double>(frequencyToBinIndex(harmonicHz, content.config))));
            if (harmonicBin < 0 || harmonicBin >= static_cast<int>(content.config.binCount)) {
                continue;
            }
            if (cellAmplitudeDb(content, harmonicBin, startFrame) <= kHarmonicSilenceFloorDb) {
                continue;  // No real signal at this harmonic - nothing to extend the selection to.
            }
            floodFillInto(content, harmonicBin, startFrame, toleranceDb, selected);
        }
    }

    int minBin = std::numeric_limits<int>::max();
    int maxBin = std::numeric_limits<int>::min();
    int minFrame = std::numeric_limits<int>::max();
    int maxFrame = std::numeric_limits<int>::min();
    for (int bin = 0; bin < static_cast<int>(content.config.binCount); ++bin) {
        for (int frame = 0; frame < static_cast<int>(content.frameCount); ++frame) {
            if (selected[cellIndex(bin, frame, content.frameCount)]) {
                minBin = std::min(minBin, bin);
                maxBin = std::max(maxBin, bin);
                minFrame = std::min(minFrame, frame);
                maxFrame = std::max(maxFrame, frame);
            }
        }
    }
    if (maxBin < minBin) {
        return empty;  // Shouldn't happen - the anchor cell always matches itself.
    }

    const auto frameSpan = static_cast<std::size_t>(maxFrame - minFrame + 1);
    const auto binSpan = static_cast<std::size_t>(maxBin - minBin + 1);
    std::vector<bool> mask(frameSpan * binSpan, false);
    for (int bin = minBin; bin <= maxBin; ++bin) {
        for (int frame = minFrame; frame <= maxFrame; ++frame) {
            if (selected[cellIndex(bin, frame, content.frameCount)]) {
                mask[static_cast<std::size_t>(bin - minBin) * frameSpan + static_cast<std::size_t>(frame - minFrame)] =
                    true;
            }
        }
    }
    return SelectionRegion(minFrame, maxFrame, minBin, maxBin, std::move(mask));
}

}  // namespace sound_mind::core
