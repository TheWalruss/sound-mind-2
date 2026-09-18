#include "sound_mind/core/warp_application.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief This codebase's own `-96..0` dB display range (matching every
/// other file's own local copy of this convention).
constexpr float kSilenceFloorDb = -96.0f;

/// @brief The fewest steps any curve segment gets tessellated into,
/// regardless of how short its own frame/bin span is.
constexpr int kMinStepsPerSegment = 4;

/// @brief Densely samples `curve` (a cubic-Bézier `Path`) with enough
/// resolution that no integer frame or bin index it actually passes
/// through is skipped - adaptive per segment (unlike containsPoint()'s
/// own fixed subdivision count, which only needs a close-enough polygon,
/// not to hit every discrete cell), since a skipped index here would
/// wrongly read as "the curve never reaches this column/row" instead of
/// zero deflection.
std::vector<TimeFrequencyPoint> denseSampleCurve(const Path& curve, const sound_mind::codec::StreamCodecConfig& config) {
    std::vector<TimeFrequencyPoint> samples;
    const auto& nodes = curve.nodes();
    if (nodes.size() < 2) {
        return samples;
    }

    for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
        const PathNode& start = nodes[i];
        const PathNode& end = nodes[i + 1];
        const TimeFrequencyPoint p0 = start.anchor;
        const TimeFrequencyPoint p1 = start.handleOut.value_or(start.anchor);
        const TimeFrequencyPoint p2 = end.handleIn.value_or(end.anchor);
        const TimeFrequencyPoint p3 = end.anchor;

        const double frameSpan =
            std::abs(timeToFrameIndex(p3.timeSeconds, config) - timeToFrameIndex(p0.timeSeconds, config));
        const double binSpan = std::abs(
            static_cast<double>(frequencyToBinIndex(static_cast<float>(p3.frequencyHz), config)) -
            static_cast<double>(frequencyToBinIndex(static_cast<float>(p0.frequencyHz), config)));
        // 2x oversampled, so a diagonal segment can't skip an integer
        // index on either axis.
        const int steps =
            std::max(kMinStepsPerSegment, static_cast<int>(std::ceil(std::max(frameSpan, binSpan))) * 2);

        if (i == 0) {
            samples.push_back(p0);
        }
        for (int step = 1; step <= steps; ++step) {
            const double t = static_cast<double>(step) / static_cast<double>(steps);
            samples.push_back(evaluateCubicBezier(p0, p1, p2, p3, t));
        }
    }
    return samples;
}

/// @brief One column's (or row's) own deflection, plus whether the curve
/// actually reaches it at all.
struct Deflection {
    double value = 0.0;
    bool covered = false;
};

/// @brief Builds one deflection per integer index in `[rangeLow, rangeHigh]`
/// (frame index for `WarpAxis::Frequency`, bin index for `WarpAxis::Time`),
/// by binning `samples` to their own nearest such index and comparing
/// each sample's own *other*-axis position (converted to bins for
/// Frequency, frames for Time) against `startValue` - the curve's own
/// first node, already converted the same way. When more than one sample
/// lands on the same index, the one closest to `startValue` wins (the
/// same "smallest absolute deflection" convention the legacy tool uses).
std::vector<Deflection> buildDeflections(const std::vector<TimeFrequencyPoint>& samples, WarpAxis axis,
                                          int rangeLow, int rangeHigh, double startValue,
                                          const sound_mind::codec::StreamCodecConfig& config) {
    std::vector<Deflection> deflections(static_cast<std::size_t>(rangeHigh - rangeLow + 1));
    for (const TimeFrequencyPoint& sample : samples) {
        int index = 0;
        double value = 0.0;
        if (axis == WarpAxis::Frequency) {
            index = static_cast<int>(std::lround(timeToFrameIndex(sample.timeSeconds, config)));
            value = static_cast<double>(frequencyToBinIndex(static_cast<float>(sample.frequencyHz), config));
        } else {
            index = static_cast<int>(
                std::lround(static_cast<double>(frequencyToBinIndex(static_cast<float>(sample.frequencyHz), config))));
            value = timeToFrameIndex(sample.timeSeconds, config);
        }
        if (index < rangeLow || index > rangeHigh) {
            continue;
        }
        Deflection& slot = deflections[static_cast<std::size_t>(index - rangeLow)];
        const double deflection = value - startValue;
        if (!slot.covered || std::abs(deflection) < std::abs(slot.value)) {
            slot.value = deflection;
            slot.covered = true;
        }
    }
    return deflections;
}

/// @brief Shifts one column's (or row's) own `planeFull` in place - a
/// direct, per-plane port of the legacy tool's own `_shift_slice_bilinear()`.
/// `selLow`/`selHigh` are inclusive, matching `FrameBinRange`'s own
/// convention (the legacy function's own `sel_start`/`sel_end` are half-
/// open - `selHigh + 1` below is that same adjustment).
void shiftSliceBilinear(const std::vector<float>& snapshotFull, std::vector<float>& destFull, int selLow, int selHigh,
                         double deflection, float vacateFillValue) {
    const int fullLen = static_cast<int>(destFull.size());
    const int selEndExclusive = selHigh + 1;

    for (int i = selLow; i < selEndExclusive && i < fullLen; ++i) {
        if (i >= 0) {
            destFull[static_cast<std::size_t>(i)] = vacateFillValue;
        }
    }

    const int deflectionFloor = static_cast<int>(std::floor(deflection));
    const int outStart = std::max(0, selLow + deflectionFloor);
    const int outEnd = std::min(fullLen, selEndExclusive + deflectionFloor + 1);  // +1: bilinear spread.
    if (outEnd <= outStart) {
        return;
    }

    const int selLen = selEndExclusive - selLow;
    for (int outIndex = outStart; outIndex < outEnd; ++outIndex) {
        const double srcF = outIndex - deflection;
        const int srcFloor = static_cast<int>(std::floor(srcF));
        const double srcFrac = srcF - srcFloor;

        const int local0 = srcFloor - selLow;
        const int local1 = local0 + 1;
        const bool in0 = local0 >= 0 && local0 < selLen;
        const bool in1 = local1 >= 0 && local1 < selLen;
        if (!in0 && !in1) {
            continue;
        }

        const float v0 = in0 ? snapshotFull[static_cast<std::size_t>(selLow + local0)] : vacateFillValue;
        const float v1 = in1 ? snapshotFull[static_cast<std::size_t>(selLow + local1)] : vacateFillValue;
        destFull[static_cast<std::size_t>(outIndex)] =
            static_cast<float>(v0 * (1.0 - srcFrac) + v1 * srcFrac);
    }
}

}  // namespace

void applyWarpOperation(const WarpOperation& operation, sound_mind::codec::StreamImage& content) {
    if (content.frameCount == 0 || content.config.binCount == 0) {
        return;
    }
    if (operation.curve().nodes().size() < 2) {
        return;
    }

    const FrameBinRange range = rangeFor(operation.bounds(), content.config, content.frameCount);
    if (range.frameHigh < range.frameLow || range.binHigh < range.binLow) {
        return;
    }

    const std::vector<TimeFrequencyPoint> samples = denseSampleCurve(operation.curve(), content.config);
    if (samples.empty()) {
        return;
    }

    const TimeFrequencyPoint curveStart = operation.curve().nodes().front().anchor;
    const int binCount = static_cast<int>(content.config.binCount);
    const int frameCount = static_cast<int>(content.frameCount);

    // Snapshot every plane, unmodified, to read from - the same
    // "read from a separate buffer, never the one being written"
    // correctness precedent every other multi-cell edit already
    // establishes.
    const std::vector<float> leftSnapshot = content.leftMagnitudeDb;
    const std::vector<float> rightSnapshot = content.rightMagnitudeDb;
    const std::vector<float> phaseSnapshot = content.sharedPhaseRadians;

    std::vector<float> column(static_cast<std::size_t>(binCount));  // scratch buffer, reused per column/row.

    if (operation.axis() == WarpAxis::Frequency) {
        const double startBin = frequencyToBinIndex(static_cast<float>(curveStart.frequencyHz), content.config);
        std::vector<Deflection> deflections =
            buildDeflections(samples, WarpAxis::Frequency, range.frameLow, range.frameHigh, startBin, content.config);

        const int span = range.frameHigh - range.frameLow;
        for (int frame = range.frameLow; frame <= range.frameHigh; ++frame) {
            Deflection& deflection = deflections[static_cast<std::size_t>(frame - range.frameLow)];
            if (!deflection.covered) {
                continue;
            }
            if (operation.mode() == WarpMode::Stretch) {
                const double scale = (span > 0) ? static_cast<double>(frame - range.frameLow) / span : 0.0;
                deflection.value *= scale;
            }
            if (std::abs(deflection.value) < 1e-6) {
                continue;
            }

            const auto readColumn = [&](const std::vector<float>& snapshot) {
                for (int bin = 0; bin < binCount; ++bin) {
                    column[static_cast<std::size_t>(bin)] = snapshot[cellIndex(bin, frame, content.frameCount)];
                }
            };
            const auto writeColumn = [&](std::vector<float>& destination) {
                for (int bin = 0; bin < binCount; ++bin) {
                    destination[cellIndex(bin, frame, content.frameCount)] = column[static_cast<std::size_t>(bin)];
                }
            };

            readColumn(leftSnapshot);
            std::vector<float> leftDest = column;
            shiftSliceBilinear(column, leftDest, range.binLow, range.binHigh, deflection.value, kSilenceFloorDb);

            readColumn(rightSnapshot);
            std::vector<float> rightDest = column;
            shiftSliceBilinear(column, rightDest, range.binLow, range.binHigh, deflection.value, kSilenceFloorDb);

            readColumn(phaseSnapshot);
            std::vector<float> phaseDest = column;
            shiftSliceBilinear(column, phaseDest, range.binLow, range.binHigh, deflection.value, 0.0f);

            column = leftDest;
            writeColumn(content.leftMagnitudeDb);
            column = rightDest;
            writeColumn(content.rightMagnitudeDb);
            column = phaseDest;
            writeColumn(content.sharedPhaseRadians);
        }
    } else {
        const double startFrame = timeToFrameIndex(curveStart.timeSeconds, content.config);
        std::vector<Deflection> deflections =
            buildDeflections(samples, WarpAxis::Time, range.binLow, range.binHigh, startFrame, content.config);

        column.resize(static_cast<std::size_t>(frameCount));
        const int span = range.binHigh - range.binLow;
        for (int bin = range.binLow; bin <= range.binHigh; ++bin) {
            Deflection& deflection = deflections[static_cast<std::size_t>(bin - range.binLow)];
            if (!deflection.covered) {
                continue;
            }
            if (operation.mode() == WarpMode::Stretch) {
                const double scale = (span > 0) ? static_cast<double>(bin - range.binLow) / span : 0.0;
                deflection.value *= scale;
            }
            if (std::abs(deflection.value) < 1e-6) {
                continue;
            }

            const auto readRow = [&](const std::vector<float>& snapshot) {
                for (int frame = 0; frame < frameCount; ++frame) {
                    column[static_cast<std::size_t>(frame)] = snapshot[cellIndex(bin, frame, content.frameCount)];
                }
            };
            const auto writeRow = [&](std::vector<float>& destination) {
                for (int frame = 0; frame < frameCount; ++frame) {
                    destination[cellIndex(bin, frame, content.frameCount)] = column[static_cast<std::size_t>(frame)];
                }
            };

            readRow(leftSnapshot);
            std::vector<float> leftDest = column;
            shiftSliceBilinear(column, leftDest, range.frameLow, range.frameHigh, deflection.value, kSilenceFloorDb);

            readRow(rightSnapshot);
            std::vector<float> rightDest = column;
            shiftSliceBilinear(column, rightDest, range.frameLow, range.frameHigh, deflection.value, kSilenceFloorDb);

            readRow(phaseSnapshot);
            std::vector<float> phaseDest = column;
            shiftSliceBilinear(column, phaseDest, range.frameLow, range.frameHigh, deflection.value, 0.0f);

            column = leftDest;
            writeRow(content.leftMagnitudeDb);
            column = rightDest;
            writeRow(content.rightMagnitudeDb);
            column = phaseDest;
            writeRow(content.sharedPhaseRadians);
        }
    }
}

}  // namespace sound_mind::core
