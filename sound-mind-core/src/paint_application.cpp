#include "sound_mind/core/paint_application.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <utility>

#include "sound_mind/core/fill_application.h"
#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/paste_application.h"
#include "sound_mind/core/paste_operation.h"

namespace sound_mind::core {

namespace {

/// @brief How many stamp positions to sample per fraction of the brush's
/// own radius along a segment - dense enough that consecutive stamps
/// overlap into a continuous stroke rather than visible dots.
constexpr double kStepsPerRadius = 4.0;

/// @brief The fewest steps any segment gets sampled at, regardless of how
/// short it is (a segment shorter than the brush radius would otherwise
/// round down to 0-1 steps and paint nothing).
constexpr int kMinStepsPerSegment = 4;

/// @brief A point in the same normalized (seconds, seconds-equivalent-
/// frequency) space `fitPathToPoints()` uses - see paint_application.h's
/// own docs for why brush size needs the same normalization Path capture
/// does.
struct NormalizedPoint {
    double timeSeconds = 0.0;
    double normalizedFrequency = 0.0;
};

NormalizedPoint normalize(const TimeFrequencyPoint& point, double frequencyToTimeScale) {
    return NormalizedPoint{point.timeSeconds, point.frequencyHz / frequencyToTimeScale};
}

double distance(const NormalizedPoint& a, const NormalizedPoint& b) {
    const double dt = a.timeSeconds - b.timeSeconds;
    const double df = a.normalizedFrequency - b.normalizedFrequency;
    return std::sqrt(dt * dt + df * df);
}

/// @brief One stroke sample: a real position, plus its path-parameter `t`
/// (start=0, end=1) for evaluating the path's own Gradient at that point.
/// Approximated as (segment index + local fraction) / segment count -
/// close enough for a brush's own falloff-blended coverage, not an
/// arc-length-exact parametrization.
struct StrokeSample {
    TimeFrequencyPoint point;
    float pathT = 0.0f;
};

/// @brief The dense, evenly-parametrized (in each segment's own `t`, not
/// arc length) sample list every `StampMode` starts from - `Stroke`
/// mode's own final result, and the raw material `sampleStrokeAlongCurve()`/
/// `sampleStrokeAxisCrossings()` below walk/interpolate the real stamp
/// positions from. Dense enough (`kStepsPerRadius`) for `Stroke`
/// mode's own consecutive stamps to overlap into a solid stroke, which
/// - since a stamp mode's own interval is essentially always coarser
/// than that - is more than enough resolution for the other modes to
/// interpolate against too.
std::vector<StrokeSample> sampleStrokeDense(const Path& path, double frequencyToTimeScale) {
    std::vector<StrokeSample> samples;
    const auto& nodes = path.nodes();
    if (nodes.size() < 2) {
        if (nodes.size() == 1) {
            samples.push_back(StrokeSample{nodes.front().anchor, 0.0f});
        }
        return samples;
    }

    const std::size_t segmentCount = nodes.size() - 1;
    for (std::size_t i = 0; i < segmentCount; ++i) {
        const PathNode& start = nodes[i];
        const PathNode& end = nodes[i + 1];
        const TimeFrequencyPoint p0 = start.anchor;
        const TimeFrequencyPoint p1 = start.handleOut.value_or(start.anchor);
        const TimeFrequencyPoint p2 = end.handleIn.value_or(end.anchor);
        const TimeFrequencyPoint p3 = end.anchor;

        const double segmentLength = distance(normalize(p0, frequencyToTimeScale), normalize(p3, frequencyToTimeScale));
        const int steps = std::max(kMinStepsPerSegment,
                                    static_cast<int>(std::ceil(segmentLength * kStepsPerRadius)));

        // Each segment's own [0, 1) contributes its own slice of the whole
        // path's [0, 1] parameter range - the first segment's start (t=0
        // overall) is sampled once, up front; every segment's own end
        // point is sampled as the *next* segment's start, except the
        // final one's, sampled explicitly after the loop.
        if (i == 0) {
            samples.push_back(StrokeSample{p0, 0.0f});
        }
        for (int step = 1; step <= steps; ++step) {
            const double localT = static_cast<double>(step) / static_cast<double>(steps);
            const float globalT =
                static_cast<float>((static_cast<double>(i) + localT) / static_cast<double>(segmentCount));
            samples.push_back(StrokeSample{evaluateCubicBezier(p0, p1, p2, p3, localT), globalT});
        }
    }
    return samples;
}

/// @brief Linear interpolation between two samples - exact (not merely
/// approximate) whenever `a`/`b` sit on a single straight segment, which
/// is exactly the case sampleStrokeAxisCrossings() below ever calls this
/// for (interpolating *between* two already-dense points, never across a
/// real curve's own bend).
StrokeSample lerpSample(const StrokeSample& a, const StrokeSample& b, double t) {
    return StrokeSample{TimeFrequencyPoint{a.point.timeSeconds + (b.point.timeSeconds - a.point.timeSeconds) * t,
                                             a.point.frequencyHz + (b.point.frequencyHz - a.point.frequencyHz) * t},
                          static_cast<float>(a.pathT + (b.pathT - a.pathT) * t)};
}

/// @brief `dense`, re-sampled at fixed steps of `interval` along its own
/// arc length (in the same normalized space `size()` uses) -
/// `StampMode::AlongCurve`'s own placement. Linearly interpolates the
/// exact point at each target arc length, between whichever dense-to-
/// dense hop it falls in (the same technique `sampleStrokeAxisCrossings()`
/// below uses along a straight hop) - snapping to the *nearest* already-
/// dense sample instead would only be exact by coincidence, since `dense`
/// itself is tuned to be fine enough for a stamp's own blended footprint
/// (see its own docs), not for arc length specifically - a requested
/// `interval` finer than one dense hop's own length would otherwise
/// repeat the same nearest vertex for every target that falls within it.
std::vector<StrokeSample> sampleStrokeAlongCurve(const std::vector<StrokeSample>& dense, double frequencyToTimeScale,
                                                   double interval) {
    std::vector<StrokeSample> result;
    if (dense.empty() || interval <= 0.0) {
        return result;
    }
    if (dense.size() == 1) {
        result.push_back(dense.front());
        return result;
    }

    std::vector<double> cumulativeLength(dense.size(), 0.0);
    for (std::size_t i = 1; i < dense.size(); ++i) {
        cumulativeLength[i] = cumulativeLength[i - 1] +
                               distance(normalize(dense[i - 1].point, frequencyToTimeScale),
                                        normalize(dense[i].point, frequencyToTimeScale));
    }
    const double total = cumulativeLength.back();

    std::size_t segment = 0;
    for (double target = 0.0; target <= total + 1e-9; target += interval) {
        while (segment + 2 < dense.size() && cumulativeLength[segment + 1] < target) {
            ++segment;
        }
        const double segmentStart = cumulativeLength[segment];
        const double segmentEnd = cumulativeLength[segment + 1];
        const double t = segmentEnd > segmentStart ? (target - segmentStart) / (segmentEnd - segmentStart) : 0.0;
        result.push_back(lerpSample(dense[segment], dense[segment + 1], std::clamp(t, 0.0, 1.0)));
    }
    return result;
}

/// @brief Which coordinate of a `TimeFrequencyPoint` `StampMode::TimeAxis`/
/// `FrequencyAxis` measure crossings along.
enum class StampAxis { Time, Frequency };

double stampAxisCoordinate(const TimeFrequencyPoint& point, StampAxis axis) {
    return axis == StampAxis::Time ? point.timeSeconds : point.frequencyHz;
}

/// @brief `dense`, re-sampled at every point it crosses `axis`'s own
/// coordinate `start + k * interval` (for every integer `k`, in either
/// direction of travel - a hop can cross more than one such line, and a
/// path that reverses direction can cross the same one more than once,
/// each its own stamp) - `StampMode::TimeAxis`/`FrequencyAxis`'s own
/// placement, `start` being `dense`'s own first point. Always includes
/// `dense`'s own first point itself (`k = 0`), the one crossing no
/// dense-to-dense hop could ever produce on its own.
std::vector<StrokeSample> sampleStrokeAxisCrossings(const std::vector<StrokeSample>& dense, double interval,
                                                      StampAxis axis) {
    std::vector<StrokeSample> result;
    if (dense.empty() || interval <= 0.0) {
        return result;
    }
    result.push_back(dense.front());

    const double start = stampAxisCoordinate(dense.front().point, axis);
    for (std::size_t i = 0; i + 1 < dense.size(); ++i) {
        const double c0 = stampAxisCoordinate(dense[i].point, axis);
        const double c1 = stampAxisCoordinate(dense[i + 1].point, axis);
        if (c0 == c1) {
            continue;  // No crossing possible along a hop parallel to this axis's own gridlines.
        }
        const double lo = std::min(c0, c1);
        const double hi = std::max(c0, c1);
        const int kFirst = static_cast<int>(std::ceil((lo - start) / interval));
        const int kLast = static_cast<int>(std::floor((hi - start) / interval));
        for (int k = kFirst; k <= kLast; ++k) {
            const double target = start + static_cast<double>(k) * interval;
            // Half-open (lo, hi] so a target landing exactly on a shared
            // dense point between two consecutive hops is only ever
            // stamped once - by whichever hop it's the *high* end of.
            if (target <= lo || target > hi) {
                continue;
            }
            result.push_back(lerpSample(dense[i], dense[i + 1], (target - c0) / (c1 - c0)));
        }
    }
    return result;
}

std::vector<StrokeSample> sampleStroke(const Path& path, double frequencyToTimeScale,
                                        const ToolConfiguration& toolConfig) {
    const std::vector<StrokeSample> dense = sampleStrokeDense(path, frequencyToTimeScale);
    switch (toolConfig.stampMode()) {
        case StampMode::AlongCurve:
            return sampleStrokeAlongCurve(dense, frequencyToTimeScale, toolConfig.stampInterval());
        case StampMode::TimeAxis:
            return sampleStrokeAxisCrossings(dense, toolConfig.stampInterval(), StampAxis::Time);
        case StampMode::FrequencyAxis:
            return sampleStrokeAxisCrossings(dense, toolConfig.stampInterval(), StampAxis::Frequency);
        case StampMode::Stroke:
        default:
            return dense;
    }
}

/// @brief The tip footprint's own normalized distance metric - `<= 1.0`
/// means the pixel at `(dt, df)` (already divided by the tip's own frame/
/// bin radius) falls within the stamp.
double footprintDistance(BrushTipShape shape, double normalizedDt, double normalizedDf) {
    switch (shape) {
        case BrushTipShape::Square:
            return std::max(std::abs(normalizedDt), std::abs(normalizedDf));
        case BrushTipShape::Diamond:
            return std::abs(normalizedDt) + std::abs(normalizedDf);
        case BrushTipShape::Circle:
        default:
            // Every tip shape without its own real footprint yet falls
            // back to Circle's - see applyPaintOperation()'s own docs.
            return std::sqrt(normalizedDt * normalizedDt + normalizedDf * normalizedDf);
    }
}

/// @brief `1` at the stamp's own center, fading to `0` at its edge over
/// the outer `falloff` fraction of the radius - a hard edge exactly at
/// the radius when `falloff == 0`.
float falloffWeight(double normalizedDistance, float falloff) {
    if (normalizedDistance >= 1.0) {
        return 0.0f;
    }
    const double softEdgeStart = 1.0 - static_cast<double>(std::clamp(falloff, 0.0f, 1.0f));
    if (normalizedDistance <= softEdgeStart) {
        return 1.0f;
    }
    const double softEdgeWidth = 1.0 - softEdgeStart;
    if (softEdgeWidth <= 0.0) {
        return 1.0f;
    }
    return static_cast<float>(1.0 - (normalizedDistance - softEdgeStart) / softEdgeWidth);
}

/// @brief `ProceduralConfiguration`'s own stamp: the existing 2D
/// footprint-blend algorithm, unchanged since before `ToolConfiguration`
/// became polymorphic - see applyPaintOperation()'s own docs.
void applyProceduralPaintOperation(const PaintOperation& operation, const ProceduralConfiguration& toolConfig,
                                    const std::vector<StrokeSample>& samples, double frequencyToTimeScale,
                                    sound_mind::codec::StreamImage& content) {
    const double frameRadius =
        timeToFrameIndex(toolConfig.size(), content.config) - timeToFrameIndex(0.0, content.config);
    if (frameRadius <= 0.0) {
        return;
    }

    for (const StrokeSample& sample : samples) {
        const GradientStop target = operation.path().gradient().evaluate(sample.pathT);

        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const float binCenter = frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz), content.config);

        // Local bin-radius: the bin index of the same Hz-radius above and
        // below this stamp's own center frequency, averaged - a cheap
        // linearization of the log-scale mapping, accurate enough over
        // one brush stamp's own small span.
        const float frequencyRadiusHz = static_cast<float>(toolConfig.size() * frequencyToTimeScale);
        const float binAbove = frequencyToBinIndex(
            static_cast<float>(sample.point.frequencyHz) + frequencyRadiusHz, content.config);
        const float binBelow = frequencyToBinIndex(
            static_cast<float>(sample.point.frequencyHz) - frequencyRadiusHz, content.config);
        const double binRadius = std::max(1e-6, (std::abs(binAbove - binCenter) + std::abs(binCenter - binBelow)) / 2.0);

        const auto frameLow = std::max(0, static_cast<int>(std::floor(frameCenter - frameRadius)));
        const auto frameHigh =
            std::min(static_cast<int>(content.frameCount) - 1, static_cast<int>(std::ceil(frameCenter + frameRadius)));
        const auto binLow = std::max(0, static_cast<int>(std::floor(binCenter - binRadius)));
        const auto binHigh =
            std::min(static_cast<int>(content.config.binCount) - 1, static_cast<int>(std::ceil(binCenter + binRadius)));

        for (int frame = frameLow; frame <= frameHigh; ++frame) {
            const double normalizedDt = (static_cast<double>(frame) - frameCenter) / frameRadius;
            for (int bin = binLow; bin <= binHigh; ++bin) {
                const double normalizedDf = (static_cast<double>(bin) - binCenter) / binRadius;
                const double dist = footprintDistance(toolConfig.tipShape(), normalizedDt, normalizedDf);
                const float weight = falloffWeight(dist, toolConfig.falloff());
                if (weight <= 0.0f) {
                    continue;
                }

                const std::size_t index = cellIndex(bin, frame, content.frameCount);
                blendTowardStop(content.leftMagnitudeDb[index], content.rightMagnitudeDb[index], target, weight);
            }
        }
    }
}

/// @brief `InstrumentConfiguration`'s own stamp: one bin-exact spike per
/// harmonic above each sample's own frequency (see
/// `InstrumentConfiguration::inharmonicity()`'s own docs for the stretched-
/// partial formula), each blended toward the stroke's own gradient target
/// only along the *time* axis (`falloff()`/`size()` reused, the same
/// frame-radius/falloff math `applyProceduralPaintOperation()` uses on that
/// axis alone) and scaled by that harmonic's own strength - no frequency-
/// axis blending, since a harmonic partial is a single exact frequency, not
/// a 2D geometric blob.
void applyInstrumentPaintOperation(const PaintOperation& operation, const InstrumentConfiguration& toolConfig,
                                    const std::vector<StrokeSample>& samples,
                                    sound_mind::codec::StreamImage& content) {
    const double frameRadius =
        timeToFrameIndex(toolConfig.size(), content.config) - timeToFrameIndex(0.0, content.config);
    if (frameRadius <= 0.0) {
        return;
    }

    const std::vector<double>& strengths = toolConfig.harmonicStrengths();
    if (strengths.empty()) {
        return;
    }

    // The same clamp frequencyToBinIndex() itself applies internally - a
    // harmonic stretched past this would otherwise silently clamp to the
    // top bin instead of being skipped, stacking multiple high harmonics
    // onto one bin rather than just fading them out past Nyquist/the
    // configured range.
    const float maxFrequencyHz =
        std::min(content.config.maxFrequencyHz, static_cast<float>(content.config.sampleRateHz) / 2.0f);

    for (const StrokeSample& sample : samples) {
        const GradientStop target = operation.path().gradient().evaluate(sample.pathT);
        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const auto frameLow = std::max(0, static_cast<int>(std::floor(frameCenter - frameRadius)));
        const auto frameHigh =
            std::min(static_cast<int>(content.frameCount) - 1, static_cast<int>(std::ceil(frameCenter + frameRadius)));

        const double fundamentalHz = sample.point.frequencyHz;
        for (std::size_t harmonicIndex = 0; harmonicIndex < strengths.size(); ++harmonicIndex) {
            const double strength = strengths[harmonicIndex];
            if (strength <= 0.0) {
                continue;
            }
            const double n = static_cast<double>(harmonicIndex + 1);
            const double harmonicHz = n * fundamentalHz * std::sqrt(1.0 + toolConfig.inharmonicity() * n * n);
            if (harmonicHz > static_cast<double>(maxFrequencyHz)) {
                continue;
            }
            const int bin = static_cast<int>(
                std::round(frequencyToBinIndex(static_cast<float>(harmonicHz), content.config)));
            if (bin < 0 || bin >= static_cast<int>(content.config.binCount)) {
                continue;
            }

            for (int frame = frameLow; frame <= frameHigh; ++frame) {
                const double normalizedDt = (static_cast<double>(frame) - frameCenter) / frameRadius;
                const float weight = falloffWeight(std::abs(normalizedDt), toolConfig.falloff()) *
                                      static_cast<float>(strength);
                if (weight <= 0.0f) {
                    continue;
                }
                const std::size_t index = cellIndex(bin, frame, content.frameCount);
                blendTowardStop(content.leftMagnitudeDb[index], content.rightMagnitudeDb[index], target, weight);
            }
        }
    }
}

/// @brief `MindShotConfiguration`'s own stamp: a hard, Normal-only
/// overwrite of `clip`'s own cells, centered on `(frameCenter, binCenter)`
/// - the exact same blit `applyPasteOperation()` already uses (see its
/// own docs), just centered on a stamp position instead of an explicit
/// placement rectangle. No falloff/gradient blend at all - "paints back
/// exactly as it was when captured" (`docs/sound-mind-design.md`'s "Mind
/// Shots") means every cell the clip covers is written verbatim.
void blitClipCentered(const Clip& clip, double frameCenter, float binCenter, sound_mind::codec::StreamImage& content) {
    const int frameOrigin = static_cast<int>(std::round(frameCenter)) - static_cast<int>(clip.frameCount / 2);
    const int binOrigin = static_cast<int>(std::round(binCenter)) - static_cast<int>(clip.binCount / 2);

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

            const std::size_t clipIndex = cellIndex(clipBin, clipFrame, clip.frameCount);
            const std::size_t destIndex = cellIndex(destBin, destFrame, content.frameCount);
            content.leftMagnitudeDb[destIndex] = clip.leftMagnitudeDb[clipIndex];
            content.rightMagnitudeDb[destIndex] = clip.rightMagnitudeDb[clipIndex];
            content.sharedPhaseRadians[destIndex] = clip.sharedPhaseRadians[clipIndex];
        }
    }
}

/// @brief `MindShotConfiguration`'s own stamp, applied at every stamp
/// position along the stroke - see `blitClipCentered()`'s own docs for
/// what happens at each one. A no-op if no Mind Shot has ever been
/// selected (`clip.frameCount()`/`binCount()` both `0` - a fresh,
/// never-configured `MindShotConfiguration`).
void applyMindShotPaintOperation(const MindShotConfiguration& toolConfig, const std::vector<StrokeSample>& samples,
                                  sound_mind::codec::StreamImage& content) {
    const Clip& clip = toolConfig.clip();
    if (clip.frameCount == 0 || clip.binCount == 0) {
        return;
    }

    for (const StrokeSample& sample : samples) {
        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const float binCenter = frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz), content.config);
        blitClipCentered(clip, frameCenter, binCenter, content);
    }
}

/// @brief `MindGrainConfiguration`'s own stamp, applied at every stamp
/// position along the stroke - unlike `applyMindShotPaintOperation()`
/// above, `toolConfig` itself holds no pixel content: `resolveLayerContent`
/// is called once here (not once per stamp - the source layer's own
/// content can't change mid-stroke) to fetch the source layer's *current*
/// content, and a fresh `Clip` is captured from `toolConfig.bounds()` of
/// it, then blitted with the exact same `blitClipCentered()` helper Mind
/// Shot uses. A no-op if `resolveLayerContent` is empty, the resolved
/// layer doesn't exist/has no content, or the captured clip turns out
/// empty (`bounds()` outside the source's own current extent).
void applyMindGrainPaintOperation(const MindGrainConfiguration& toolConfig, const std::vector<StrokeSample>& samples,
                                   const LayerContentResolver& resolveLayerContent,
                                   sound_mind::codec::StreamImage& content) {
    if (!resolveLayerContent) {
        return;
    }
    const sound_mind::codec::StreamImage* source = resolveLayerContent(toolConfig.sourceLayerId());
    if (source == nullptr) {
        return;
    }
    const Clip clip = captureClip(*source, toolConfig.bounds());
    if (clip.frameCount == 0 || clip.binCount == 0) {
        return;
    }

    for (const StrokeSample& sample : samples) {
        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const float binCenter = frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz), content.config);
        blitClipCentered(clip, frameCenter, binCenter, content);
    }
}

/// @brief A snapshot of `content`'s own left/right magnitude cells over
/// `[frameLow, frameHigh] x [binLow, binHigh]` (both inclusive, already
/// clamped to the canvas's own valid range by the caller) - what
/// `HealConfiguration`'s/`SoftenConfiguration`'s own per-stamp blur reads
/// from, so a stamp's own blend is computed from what the canvas looked
/// like *before* that stamp touched it, not a partially-already-blended
/// value from earlier in that same stamp's own scanline order (the same
/// "read from a separate buffer, never the one being written" precedent
/// `filter_application.cpp`'s own `gaussianBlur2D()`/`medianBlur2D()`
/// already establish for a whole-layer blur - a naive in-place blur would
/// otherwise bias toward whichever direction it happens to iterate in).
/// Stamps still compound normally across a whole stroke (or repeated
/// strokes) - only a single stamp's own internal blend reads from a stable
/// snapshot; the next stamp snapshots fresh, seeing everything the
/// previous one just wrote.
struct LocalMagnitudeSnapshot {
    int frameLow = 0;
    int binLow = 0;
    int frameSpan = 0;
    int binSpan = 0;
    std::vector<float> left;
    std::vector<float> right;

    [[nodiscard]] std::size_t indexOf(int frame, int bin) const noexcept {
        return static_cast<std::size_t>(bin - binLow) * static_cast<std::size_t>(frameSpan) +
               static_cast<std::size_t>(frame - frameLow);
    }
};

LocalMagnitudeSnapshot captureLocalSnapshot(const sound_mind::codec::StreamImage& content, int frameLow,
                                             int frameHigh, int binLow, int binHigh) {
    LocalMagnitudeSnapshot snapshot;
    snapshot.frameLow = frameLow;
    snapshot.binLow = binLow;
    snapshot.frameSpan = frameHigh - frameLow + 1;
    snapshot.binSpan = binHigh - binLow + 1;
    const std::size_t cellCount =
        static_cast<std::size_t>(snapshot.binSpan) * static_cast<std::size_t>(snapshot.frameSpan);
    snapshot.left.resize(cellCount);
    snapshot.right.resize(cellCount);
    for (int bin = binLow; bin <= binHigh; ++bin) {
        for (int frame = frameLow; frame <= frameHigh; ++frame) {
            const std::size_t srcIndex = cellIndex(bin, frame, content.frameCount);
            const std::size_t dstIndex = snapshot.indexOf(frame, bin);
            snapshot.left[dstIndex] = content.leftMagnitudeDb[srcIndex];
            snapshot.right[dstIndex] = content.rightMagnitudeDb[srcIndex];
        }
    }
    return snapshot;
}

/// @brief The plain box average of `snapshot`'s own left/right magnitude
/// cells over a `(2*frameWindow+1) x (2*binWindow+1)` neighborhood centered
/// at `(frame, bin)`, clamped to the snapshot's own edges (never wrapping)
/// - `HealConfiguration`'s (`binWindow == 0`) and `SoftenConfiguration`'s
/// (both axes) own shared per-pixel blur target. `opacitySource`'s own
/// opacity (not intensity - see `HealConfiguration`'s own docs) becomes the
/// returned stop's own opacity, ready to hand straight to
/// `blendTowardStop()`.
GradientStop blurredNeighborhoodStop(const LocalMagnitudeSnapshot& snapshot, int frame, int bin, int frameWindow,
                                      int binWindow, const GradientStop& opacitySource) {
    const int frameLow = std::max(snapshot.frameLow, frame - frameWindow);
    const int frameHigh = std::min(snapshot.frameLow + snapshot.frameSpan - 1, frame + frameWindow);
    const int binLow = std::max(snapshot.binLow, bin - binWindow);
    const int binHigh = std::min(snapshot.binLow + snapshot.binSpan - 1, bin + binWindow);

    float leftSum = 0.0f;
    float rightSum = 0.0f;
    int count = 0;
    for (int f = frameLow; f <= frameHigh; ++f) {
        for (int b = binLow; b <= binHigh; ++b) {
            const std::size_t index = snapshot.indexOf(f, b);
            leftSum += snapshot.left[index];
            rightSum += snapshot.right[index];
            ++count;
        }
    }

    GradientStop stop;
    if (count > 0) {
        stop.leftIntensity = leftSum / static_cast<float>(count);
        stop.rightIntensity = rightSum / static_cast<float>(count);
    }
    stop.leftOpacity = opacitySource.leftOpacity;
    stop.rightOpacity = opacitySource.rightOpacity;
    return stop;
}

/// @brief `HealConfiguration`'s own stamp: within the usual 2D
/// falloff-weighted footprint (identical to
/// `applyProceduralPaintOperation()`'s own), each pixel blends toward the
/// box average of its own neighboring cells along the time axis only, same
/// bin - see `HealConfiguration`'s own docs for why `size()` doubles as
/// both the footprint radius and the blur window's own half-width, and
/// `blurredNeighborhoodStop()`'s own docs for why each stamp reads from a
/// fresh per-stamp snapshot rather than the live, mutating `content`.
void applyHealPaintOperation(const PaintOperation& operation, const HealConfiguration& toolConfig,
                              const std::vector<StrokeSample>& samples, double frequencyToTimeScale,
                              sound_mind::codec::StreamImage& content) {
    const double frameRadius =
        timeToFrameIndex(toolConfig.size(), content.config) - timeToFrameIndex(0.0, content.config);
    if (frameRadius <= 0.0) {
        return;
    }
    const int blurFrameWindow = std::max(1, static_cast<int>(std::lround(frameRadius)));

    for (const StrokeSample& sample : samples) {
        const GradientStop opacitySource = operation.path().gradient().evaluate(sample.pathT);

        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const float binCenter = frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz), content.config);

        const float frequencyRadiusHz = static_cast<float>(toolConfig.size() * frequencyToTimeScale);
        const float binAbove =
            frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz) + frequencyRadiusHz, content.config);
        const float binBelow =
            frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz) - frequencyRadiusHz, content.config);
        const double binRadius = std::max(1e-6, (std::abs(binAbove - binCenter) + std::abs(binCenter - binBelow)) / 2.0);

        const auto frameLow = std::max(0, static_cast<int>(std::floor(frameCenter - frameRadius)));
        const auto frameHigh =
            std::min(static_cast<int>(content.frameCount) - 1, static_cast<int>(std::ceil(frameCenter + frameRadius)));
        const auto binLow = std::max(0, static_cast<int>(std::floor(binCenter - binRadius)));
        const auto binHigh =
            std::min(static_cast<int>(content.config.binCount) - 1, static_cast<int>(std::ceil(binCenter + binRadius)));

        // Snapshot region extended by the blur window itself (frames only -
        // Heal never reaches into a neighboring bin) so every footprint
        // pixel's own blur neighborhood is fully covered.
        const int snapshotFrameLow = std::max(0, frameLow - blurFrameWindow);
        const int snapshotFrameHigh = std::min(static_cast<int>(content.frameCount) - 1, frameHigh + blurFrameWindow);
        const LocalMagnitudeSnapshot snapshot =
            captureLocalSnapshot(content, snapshotFrameLow, snapshotFrameHigh, binLow, binHigh);

        for (int frame = frameLow; frame <= frameHigh; ++frame) {
            const double normalizedDt = (static_cast<double>(frame) - frameCenter) / frameRadius;
            for (int bin = binLow; bin <= binHigh; ++bin) {
                const double normalizedDf = (static_cast<double>(bin) - binCenter) / binRadius;
                const double dist = footprintDistance(BrushTipShape::Circle, normalizedDt, normalizedDf);
                const float weight = falloffWeight(dist, toolConfig.falloff());
                if (weight <= 0.0f) {
                    continue;
                }
                const GradientStop target = blurredNeighborhoodStop(snapshot, frame, bin, blurFrameWindow, 0, opacitySource);
                const std::size_t index = cellIndex(bin, frame, content.frameCount);
                blendTowardStop(content.leftMagnitudeDb[index], content.rightMagnitudeDb[index], target, weight);
            }
        }
    }
}

/// @brief `SoftenConfiguration`'s own stamp - identical to
/// `applyHealPaintOperation()` above except the blur neighborhood spans
/// both axes (isotropic) rather than frames alone, per
/// `SoftenConfiguration`'s own docs.
void applySoftenPaintOperation(const PaintOperation& operation, const SoftenConfiguration& toolConfig,
                                const std::vector<StrokeSample>& samples, double frequencyToTimeScale,
                                sound_mind::codec::StreamImage& content) {
    const double frameRadius =
        timeToFrameIndex(toolConfig.size(), content.config) - timeToFrameIndex(0.0, content.config);
    if (frameRadius <= 0.0) {
        return;
    }
    const int blurFrameWindow = std::max(1, static_cast<int>(std::lround(frameRadius)));

    for (const StrokeSample& sample : samples) {
        const GradientStop opacitySource = operation.path().gradient().evaluate(sample.pathT);

        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const float binCenter = frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz), content.config);

        const float frequencyRadiusHz = static_cast<float>(toolConfig.size() * frequencyToTimeScale);
        const float binAbove =
            frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz) + frequencyRadiusHz, content.config);
        const float binBelow =
            frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz) - frequencyRadiusHz, content.config);
        const double binRadius = std::max(1e-6, (std::abs(binAbove - binCenter) + std::abs(binCenter - binBelow)) / 2.0);
        const int blurBinWindow = std::max(1, static_cast<int>(std::lround(binRadius)));

        const auto frameLow = std::max(0, static_cast<int>(std::floor(frameCenter - frameRadius)));
        const auto frameHigh =
            std::min(static_cast<int>(content.frameCount) - 1, static_cast<int>(std::ceil(frameCenter + frameRadius)));
        const auto binLow = std::max(0, static_cast<int>(std::floor(binCenter - binRadius)));
        const auto binHigh =
            std::min(static_cast<int>(content.config.binCount) - 1, static_cast<int>(std::ceil(binCenter + binRadius)));

        const int snapshotFrameLow = std::max(0, frameLow - blurFrameWindow);
        const int snapshotFrameHigh = std::min(static_cast<int>(content.frameCount) - 1, frameHigh + blurFrameWindow);
        const int snapshotBinLow = std::max(0, binLow - blurBinWindow);
        const int snapshotBinHigh = std::min(static_cast<int>(content.config.binCount) - 1, binHigh + blurBinWindow);
        const LocalMagnitudeSnapshot snapshot =
            captureLocalSnapshot(content, snapshotFrameLow, snapshotFrameHigh, snapshotBinLow, snapshotBinHigh);

        for (int frame = frameLow; frame <= frameHigh; ++frame) {
            const double normalizedDt = (static_cast<double>(frame) - frameCenter) / frameRadius;
            for (int bin = binLow; bin <= binHigh; ++bin) {
                const double normalizedDf = (static_cast<double>(bin) - binCenter) / binRadius;
                const double dist = footprintDistance(BrushTipShape::Circle, normalizedDt, normalizedDf);
                const float weight = falloffWeight(dist, toolConfig.falloff());
                if (weight <= 0.0f) {
                    continue;
                }
                const GradientStop target =
                    blurredNeighborhoodStop(snapshot, frame, bin, blurFrameWindow, blurBinWindow, opacitySource);
                const std::size_t index = cellIndex(bin, frame, content.frameCount);
                blendTowardStop(content.leftMagnitudeDb[index], content.rightMagnitudeDb[index], target, weight);
            }
        }
    }
}

/// @brief The plain average of `snapshot`'s own left/right magnitude cells
/// sampled along the line through `(frame, bin)` oriented by
/// `(deltaFrame, deltaBin)`, spanning `t` from `-0.5` to `+0.5` of that
/// vector (so the sampled span covers the same distance as one hop between
/// stroke samples, centered on the pixel) - `SmudgeConfiguration`'s own
/// per-pixel blur target. The result is invariant to the sign of
/// `(deltaFrame, deltaBin)` (a `t`-symmetric span visits the same points
/// either way), so callers never need to worry about which of two
/// neighboring samples supplied the direction.
GradientStop lineAverageStop(const LocalMagnitudeSnapshot& snapshot, int frame, int bin, double deltaFrame,
                              double deltaBin, const GradientStop& opacitySource) {
    const double length = std::sqrt(deltaFrame * deltaFrame + deltaBin * deltaBin);
    const int steps = std::max(1, static_cast<int>(std::lround(length)));

    float leftSum = 0.0f;
    float rightSum = 0.0f;
    int count = 0;
    for (int step = 0; step <= steps; ++step) {
        const double t = static_cast<double>(step) / static_cast<double>(steps) - 0.5;
        const int sampleFrame = std::clamp(static_cast<int>(std::lround(static_cast<double>(frame) + t * deltaFrame)),
                                            snapshot.frameLow, snapshot.frameLow + snapshot.frameSpan - 1);
        const int sampleBin = std::clamp(static_cast<int>(std::lround(static_cast<double>(bin) + t * deltaBin)),
                                          snapshot.binLow, snapshot.binLow + snapshot.binSpan - 1);
        const std::size_t index = snapshot.indexOf(sampleFrame, sampleBin);
        leftSum += snapshot.left[index];
        rightSum += snapshot.right[index];
        ++count;
    }

    GradientStop stop;
    if (count > 0) {
        stop.leftIntensity = leftSum / static_cast<float>(count);
        stop.rightIntensity = rightSum / static_cast<float>(count);
    }
    stop.leftOpacity = opacitySource.leftOpacity;
    stop.rightOpacity = opacitySource.rightOpacity;
    return stop;
}

/// @brief `SmudgeConfiguration`'s own stamp - see its own docs for the
/// design (confirmed with the user over a stateful "brush load" carried
/// across the whole stroke). Each stroke sample's own direction/length
/// comes from its own neighboring sample (the previous one, or - for the
/// very first sample, which has none - the next one instead, so the first
/// stamp of a multi-point stroke still smears); a single-point stroke (no
/// neighbor at all) is a no-op. Within the usual 2D falloff-weighted
/// footprint, every pixel blends toward `lineAverageStop()`'s own
/// per-pixel line average, from a fresh per-stamp snapshot (the same
/// "never read from the buffer being written" precedent
/// `blurredNeighborhoodStop()` already established).
void applySmudgePaintOperation(const PaintOperation& operation, const SmudgeConfiguration& toolConfig,
                                const std::vector<StrokeSample>& samples, double frequencyToTimeScale,
                                sound_mind::codec::StreamImage& content) {
    if (samples.size() < 2) {
        return;  // No neighboring sample to smear toward - see this tool's own docs.
    }
    const double frameRadius =
        timeToFrameIndex(toolConfig.size(), content.config) - timeToFrameIndex(0.0, content.config);
    if (frameRadius <= 0.0) {
        return;
    }

    // Every sample's own (frameCenter, binCenter), computed once - each
    // sample's own smear direction needs its neighbor's position too.
    std::vector<double> frameCenters(samples.size());
    std::vector<double> binCenters(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        frameCenters[i] = timeToFrameIndex(samples[i].point.timeSeconds, content.config);
        binCenters[i] = static_cast<double>(frequencyToBinIndex(static_cast<float>(samples[i].point.frequencyHz), content.config));
    }

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const std::size_t neighbor = (i == 0) ? 1 : i - 1;
        const double deltaFrame = frameCenters[i] - frameCenters[neighbor];
        const double deltaBin = binCenters[i] - binCenters[neighbor];
        if (deltaFrame == 0.0 && deltaBin == 0.0) {
            continue;  // No movement between these two samples - nothing to smear along.
        }

        const GradientStop opacitySource = operation.path().gradient().evaluate(samples[i].pathT);
        const double frameCenter = frameCenters[i];
        const float binCenter = static_cast<float>(binCenters[i]);

        const float frequencyRadiusHz = static_cast<float>(toolConfig.size() * frequencyToTimeScale);
        const float binAbove =
            frequencyToBinIndex(static_cast<float>(samples[i].point.frequencyHz) + frequencyRadiusHz, content.config);
        const float binBelow =
            frequencyToBinIndex(static_cast<float>(samples[i].point.frequencyHz) - frequencyRadiusHz, content.config);
        const double binRadius = std::max(1e-6, (std::abs(binAbove - binCenter) + std::abs(binCenter - binBelow)) / 2.0);

        const auto frameLow = std::max(0, static_cast<int>(std::floor(frameCenter - frameRadius)));
        const auto frameHigh =
            std::min(static_cast<int>(content.frameCount) - 1, static_cast<int>(std::ceil(frameCenter + frameRadius)));
        const auto binLow = std::max(0, static_cast<int>(std::floor(binCenter - binRadius)));
        const auto binHigh =
            std::min(static_cast<int>(content.config.binCount) - 1, static_cast<int>(std::ceil(binCenter + binRadius)));

        // The snapshot needs to reach half the smear vector's own extent
        // beyond the footprint on every side, plus one cell of slack.
        const int reachFrame = static_cast<int>(std::ceil(std::abs(deltaFrame) * 0.5)) + 1;
        const int reachBin = static_cast<int>(std::ceil(std::abs(deltaBin) * 0.5)) + 1;
        const int snapshotFrameLow = std::max(0, frameLow - reachFrame);
        const int snapshotFrameHigh = std::min(static_cast<int>(content.frameCount) - 1, frameHigh + reachFrame);
        const int snapshotBinLow = std::max(0, binLow - reachBin);
        const int snapshotBinHigh = std::min(static_cast<int>(content.config.binCount) - 1, binHigh + reachBin);
        const LocalMagnitudeSnapshot snapshot =
            captureLocalSnapshot(content, snapshotFrameLow, snapshotFrameHigh, snapshotBinLow, snapshotBinHigh);

        for (int frame = frameLow; frame <= frameHigh; ++frame) {
            const double normalizedDt = (static_cast<double>(frame) - frameCenter) / frameRadius;
            for (int bin = binLow; bin <= binHigh; ++bin) {
                const double normalizedDf = (static_cast<double>(bin) - binCenter) / binRadius;
                const double dist = footprintDistance(BrushTipShape::Circle, normalizedDt, normalizedDf);
                const float weight = falloffWeight(dist, toolConfig.falloff());
                if (weight <= 0.0f) {
                    continue;
                }
                const GradientStop target = lineAverageStop(snapshot, frame, bin, deltaFrame, deltaBin, opacitySource);
                const std::size_t index = cellIndex(bin, frame, content.frameCount);
                blendTowardStop(content.leftMagnitudeDb[index], content.rightMagnitudeDb[index], target, weight);
            }
        }
    }
}

/// @brief One footprint pixel eligible for `OrderChaosConfiguration`'s own
/// swap/reorder, alongside the falloff weight it should be blended back
/// with.
struct OrderChaosPoolEntry {
    int frame = 0;
    int bin = 0;
    float weight = 0.0f;
};

/// @brief `OrderChaosConfiguration`'s own Chaos branch (`amount() < 0`):
/// randomly selects `fraction` of `pool`'s own entries and permutes their
/// own (left, right) magnitude values among themselves - a pure
/// permutation, so at full opacity the selected subset's own total,
/// average, and histogram are exactly preserved (values only change
/// position, never value). Each permuted pixel's own new value is blended
/// back via `blendTowardStop()`, weighted by its own falloff weight and
/// `opacitySource`'s own opacity.
void applyChaos(const std::vector<OrderChaosPoolEntry>& pool, double fraction, const LocalMagnitudeSnapshot& snapshot,
                const GradientStop& opacitySource, sound_mind::codec::StreamImage& content, std::mt19937& rng) {
    const auto poolSize = pool.size();
    const auto k = static_cast<std::size_t>(std::lround(fraction * static_cast<double>(poolSize)));
    if (k < 2) {
        return;  // Nothing to swap with fewer than two selected pixels.
    }

    // A uniformly random subset of size k, then an independent random
    // shuffle of that same subset - together, a random permutation
    // restricted to k of the pool's own poolSize entries (identity for the
    // rest, left untouched).
    std::vector<std::size_t> selected(poolSize);
    std::iota(selected.begin(), selected.end(), std::size_t{0});
    std::shuffle(selected.begin(), selected.end(), rng);
    selected.resize(k);

    std::vector<std::size_t> shuffled = selected;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    for (std::size_t i = 0; i < k; ++i) {
        const OrderChaosPoolEntry& destination = pool[selected[i]];
        const OrderChaosPoolEntry& source = pool[shuffled[i]];
        const std::size_t sourceIndex = snapshot.indexOf(source.frame, source.bin);

        GradientStop target;
        target.leftIntensity = snapshot.left[sourceIndex];
        target.rightIntensity = snapshot.right[sourceIndex];
        target.leftOpacity = opacitySource.leftOpacity;
        target.rightOpacity = opacitySource.rightOpacity;

        const std::size_t destinationIndex = cellIndex(destination.bin, destination.frame, content.frameCount);
        blendTowardStop(content.leftMagnitudeDb[destinationIndex], content.rightMagnitudeDb[destinationIndex], target,
                         destination.weight);
    }
}

/// @brief `OrderChaosConfiguration`'s own Order branch (`amount() > 0`):
/// builds a horizontal (per-frame, summed over `pool`'s own bin range) and
/// a vertical (per-bin, summed over `pool`'s own frame range) energy
/// profile from `snapshot`, finds each one's own loudest position, then
/// reassigns `fraction` of `pool`'s own entries so the brightest end up
/// closest to those two peak lines and the darkest end up farthest -
/// concentrating energy into an emergent horizontal/vertical cross (see
/// `OrderChaosConfiguration`'s own docs for the audible intent). Distance
/// to the cross is `min(|frame - peakFrame|, |bin - peakBin|)` - however
/// close a point comes to *either* line. Deterministic (no randomness at
/// all) whenever `fraction` selects the entire pool, since both sorts below
/// are then applied to every entry rather than a randomly-drawn subset.
void applyOrder(const std::vector<OrderChaosPoolEntry>& pool, double fraction, const LocalMagnitudeSnapshot& snapshot,
                const GradientStop& opacitySource, sound_mind::codec::StreamImage& content, std::mt19937& rng) {
    const auto poolSize = pool.size();
    const auto k = static_cast<std::size_t>(std::lround(fraction * static_cast<double>(poolSize)));
    if (k < 2) {
        return;
    }

    // A pixel's own single scalar "value" for both the histogram and the
    // sort below - the plain average of its own two channels, the same
    // "treat dB values directly, don't convert to linear energy first"
    // simplicity precedent blurredNeighborhoodStop() already uses.
    const auto valueAt = [&snapshot](int frame, int bin) {
        const std::size_t index = snapshot.indexOf(frame, bin);
        return (snapshot.left[index] + snapshot.right[index]) / 2.0f;
    };

    int peakFrame = pool.front().frame;
    int peakBin = pool.front().bin;
    {
        // Horizontal (per-frame) and vertical (per-bin) profiles - summed
        // only over the pool's own actual extent, not the whole canvas.
        std::vector<float> frameProfile(snapshot.frameSpan, 0.0f);
        std::vector<float> binProfile(snapshot.binSpan, 0.0f);
        for (const OrderChaosPoolEntry& entry : pool) {
            const float value = valueAt(entry.frame, entry.bin);
            frameProfile[static_cast<std::size_t>(entry.frame - snapshot.frameLow)] += value;
            binProfile[static_cast<std::size_t>(entry.bin - snapshot.binLow)] += value;
        }
        const auto peakFrameIt = std::max_element(frameProfile.begin(), frameProfile.end());
        const auto peakBinIt = std::max_element(binProfile.begin(), binProfile.end());
        peakFrame = snapshot.frameLow + static_cast<int>(std::distance(frameProfile.begin(), peakFrameIt));
        peakBin = snapshot.binLow + static_cast<int>(std::distance(binProfile.begin(), peakBinIt));
    }

    // A uniformly random subset of size k (this is the only randomness
    // Order itself uses - at k == poolSize, selecting "the entire pool"
    // needs no randomness at all, making this branch fully deterministic).
    std::vector<std::size_t> selected(poolSize);
    std::iota(selected.begin(), selected.end(), std::size_t{0});
    std::shuffle(selected.begin(), selected.end(), rng);
    selected.resize(k);

    // Sort the selected positions by their own distance to the cross
    // (closest first) and, separately, their own current values (loudest
    // first) - zipping the two together hands the loudest value to the
    // closest position, down to the quietest value at the farthest one.
    std::vector<std::size_t> byDistance = selected;
    std::sort(byDistance.begin(), byDistance.end(), [&](std::size_t a, std::size_t b) {
        const int distanceA = std::min(std::abs(pool[a].frame - peakFrame), std::abs(pool[a].bin - peakBin));
        const int distanceB = std::min(std::abs(pool[b].frame - peakFrame), std::abs(pool[b].bin - peakBin));
        return distanceA < distanceB;
    });
    std::vector<float> valuesLoudestFirst;
    valuesLoudestFirst.reserve(k);
    for (const std::size_t index : selected) {
        valuesLoudestFirst.push_back(valueAt(pool[index].frame, pool[index].bin));
    }
    std::sort(valuesLoudestFirst.begin(), valuesLoudestFirst.end(), std::greater<>());

    // The exact (left, right) pair each selected position's own current
    // value maps to, read once from the snapshot before any blending below
    // touches `content` - the loudest-first values above are averages, but
    // the actual blend still needs each source pixel's own real per-channel
    // pair, not a reconstructed mono value.
    std::vector<std::size_t> byValueDescending = selected;
    std::sort(byValueDescending.begin(), byValueDescending.end(), [&](std::size_t a, std::size_t b) {
        return valueAt(pool[a].frame, pool[a].bin) > valueAt(pool[b].frame, pool[b].bin);
    });

    for (std::size_t i = 0; i < k; ++i) {
        const OrderChaosPoolEntry& destination = pool[byDistance[i]];
        const OrderChaosPoolEntry& source = pool[byValueDescending[i]];
        const std::size_t sourceIndex = snapshot.indexOf(source.frame, source.bin);

        GradientStop target;
        target.leftIntensity = snapshot.left[sourceIndex];
        target.rightIntensity = snapshot.right[sourceIndex];
        target.leftOpacity = opacitySource.leftOpacity;
        target.rightOpacity = opacitySource.rightOpacity;

        const std::size_t destinationIndex = cellIndex(destination.bin, destination.frame, content.frameCount);
        blendTowardStop(content.leftMagnitudeDb[destinationIndex], content.rightMagnitudeDb[destinationIndex], target,
                         destination.weight);
    }
}

/// @brief `OrderChaosConfiguration`'s own stamp - dispatches to
/// `applyChaos()`/`applyOrder()` by the sign of `toolConfig.amount()`
/// within the usual 2D falloff-weighted footprint (a no-op at `amount() ==
/// 0`). Each stamp draws its own fresh per-stamp snapshot first, the same
/// "never read from the buffer being written" precedent every other
/// blur/rearrange tool type already establishes.
void applyOrderChaosPaintOperation(const PaintOperation& operation, const OrderChaosConfiguration& toolConfig,
                                    const std::vector<StrokeSample>& samples, double frequencyToTimeScale,
                                    sound_mind::codec::StreamImage& content) {
    if (toolConfig.amount() == 0.0) {
        return;
    }
    const double frameRadius =
        timeToFrameIndex(toolConfig.size(), content.config) - timeToFrameIndex(0.0, content.config);
    if (frameRadius <= 0.0) {
        return;
    }

    // One shared engine for every stamp/sample this call processes -
    // reseeded fresh only once per applyPaintOperation() call (heap
    // allocation/randomness are both fine here - this runs on the UI/main
    // thread from a paint stroke, never inside a real-time audio callback).
    thread_local std::mt19937 rng{std::random_device{}()};

    for (const StrokeSample& sample : samples) {
        const GradientStop opacitySource = operation.path().gradient().evaluate(sample.pathT);

        const double frameCenter = timeToFrameIndex(sample.point.timeSeconds, content.config);
        const float binCenter = frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz), content.config);

        const float frequencyRadiusHz = static_cast<float>(toolConfig.size() * frequencyToTimeScale);
        const float binAbove =
            frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz) + frequencyRadiusHz, content.config);
        const float binBelow =
            frequencyToBinIndex(static_cast<float>(sample.point.frequencyHz) - frequencyRadiusHz, content.config);
        const double binRadius = std::max(1e-6, (std::abs(binAbove - binCenter) + std::abs(binCenter - binBelow)) / 2.0);

        const auto frameLow = std::max(0, static_cast<int>(std::floor(frameCenter - frameRadius)));
        const auto frameHigh =
            std::min(static_cast<int>(content.frameCount) - 1, static_cast<int>(std::ceil(frameCenter + frameRadius)));
        const auto binLow = std::max(0, static_cast<int>(std::floor(binCenter - binRadius)));
        const auto binHigh =
            std::min(static_cast<int>(content.config.binCount) - 1, static_cast<int>(std::ceil(binCenter + binRadius)));

        const LocalMagnitudeSnapshot snapshot = captureLocalSnapshot(content, frameLow, frameHigh, binLow, binHigh);

        std::vector<OrderChaosPoolEntry> pool;
        for (int frame = frameLow; frame <= frameHigh; ++frame) {
            const double normalizedDt = (static_cast<double>(frame) - frameCenter) / frameRadius;
            for (int bin = binLow; bin <= binHigh; ++bin) {
                const double normalizedDf = (static_cast<double>(bin) - binCenter) / binRadius;
                const double dist = footprintDistance(BrushTipShape::Circle, normalizedDt, normalizedDf);
                const float weight = falloffWeight(dist, toolConfig.falloff());
                if (weight > 0.0f) {
                    pool.push_back(OrderChaosPoolEntry{frame, bin, weight});
                }
            }
        }
        if (pool.size() < 2) {
            continue;
        }

        const double fraction = std::min(1.0, std::abs(toolConfig.amount()));
        if (toolConfig.amount() < 0.0) {
            applyChaos(pool, fraction, snapshot, opacitySource, content, rng);
        } else {
            applyOrder(pool, fraction, snapshot, opacitySource, content, rng);
        }
    }
}

}  // namespace

TimeFrequencyPoint evaluateCubicBezier(const TimeFrequencyPoint& p0, const TimeFrequencyPoint& p1,
                                        const TimeFrequencyPoint& p2, const TimeFrequencyPoint& p3,
                                        double t) noexcept {
    const double u = 1.0 - t;
    const double w0 = u * u * u;
    const double w1 = 3.0 * u * u * t;
    const double w2 = 3.0 * u * t * t;
    const double w3 = t * t * t;
    return TimeFrequencyPoint{w0 * p0.timeSeconds + w1 * p1.timeSeconds + w2 * p2.timeSeconds + w3 * p3.timeSeconds,
                               w0 * p0.frequencyHz + w1 * p1.frequencyHz + w2 * p2.frequencyHz +
                                   w3 * p3.frequencyHz};
}

float frequencyToBinIndex(float frequencyHz, const sound_mind::codec::StreamCodecConfig& config) noexcept {
    const float maxFrequencyHz = std::min(config.maxFrequencyHz, static_cast<float>(config.sampleRateHz) / 2.0f);
    const float clamped = std::clamp(frequencyHz, config.minFrequencyHz, maxFrequencyHz);
    const float logRange = std::log(maxFrequencyHz / config.minFrequencyHz);
    if (logRange <= 0.0f || config.binCount <= 1) {
        return 0.0f;
    }
    const float t = std::log(clamped / config.minFrequencyHz) / logRange;
    return t * static_cast<float>(config.binCount - 1);
}

double timeToFrameIndex(double timeSeconds, const sound_mind::codec::StreamCodecConfig& config) noexcept {
    if (config.hopLength == 0) {
        return 0.0;
    }
    return timeSeconds * static_cast<double>(config.sampleRateHz) / static_cast<double>(config.hopLength);
}

float binIndexToFrequency(float binIndex, const sound_mind::codec::StreamCodecConfig& config) noexcept {
    const float maxFrequencyHz = std::min(config.maxFrequencyHz, static_cast<float>(config.sampleRateHz) / 2.0f);
    if (config.binCount <= 1) {
        return config.minFrequencyHz;
    }
    const float clamped = std::clamp(binIndex, 0.0f, static_cast<float>(config.binCount - 1));
    const float t = clamped / static_cast<float>(config.binCount - 1);
    const float logRange = std::log(maxFrequencyHz / config.minFrequencyHz);
    return config.minFrequencyHz * std::exp(t * logRange);
}

float translateFrequencyByBins(float frequencyHz, double deltaBins,
                                 const sound_mind::codec::StreamCodecConfig& config) noexcept {
    const float bin = frequencyToBinIndex(frequencyHz, config);
    return binIndexToFrequency(static_cast<float>(static_cast<double>(bin) + deltaBins), config);
}

double frameIndexToTime(double frameIndex, const sound_mind::codec::StreamCodecConfig& config) noexcept {
    if (config.sampleRateHz == 0) {
        return 0.0;
    }
    return frameIndex * static_cast<double>(config.hopLength) / static_cast<double>(config.sampleRateHz);
}

FrameBinRange rangeFor(const TimeFrequencyRect& bounds, const sound_mind::codec::StreamCodecConfig& config,
                        std::uint32_t frameCount) noexcept {
    const double frameLowD = timeToFrameIndex(bounds.startTimeSeconds, config);
    const double frameHighD = timeToFrameIndex(bounds.endTimeSeconds, config);
    // The frequency axis is log-scaled (see frequencyToBinIndex()'s own
    // docs), but its own direction still agrees with Hz - lowFrequencyHz
    // always maps to the smaller bin index - so no separate min/max
    // ordering is needed here beyond what std::minmax already gives.
    const float binAtLow = frequencyToBinIndex(static_cast<float>(bounds.lowFrequencyHz), config);
    const float binAtHigh = frequencyToBinIndex(static_cast<float>(bounds.highFrequencyHz), config);

    FrameBinRange range;
    range.frameLow = std::clamp(static_cast<int>(std::round(std::min(frameLowD, frameHighD))), 0,
                                 static_cast<int>(frameCount) - 1);
    range.frameHigh = std::clamp(static_cast<int>(std::round(std::max(frameLowD, frameHighD))), 0,
                                  static_cast<int>(frameCount) - 1);
    range.binLow = std::clamp(static_cast<int>(std::round(std::min(binAtLow, binAtHigh))), 0,
                               static_cast<int>(config.binCount) - 1);
    range.binHigh = std::clamp(static_cast<int>(std::round(std::max(binAtLow, binAtHigh))), 0,
                                static_cast<int>(config.binCount) - 1);
    return range;
}

void applyPaintOperation(const PaintOperation& operation, double frequencyToTimeScale,
                          sound_mind::codec::StreamImage& content, const LayerContentResolver& resolveLayerContent) {
    if (frequencyToTimeScale <= 0.0 || content.frameCount == 0 || content.config.binCount == 0) {
        return;
    }

    const ToolConfiguration& toolConfig = operation.config();
    const std::vector<StrokeSample> samples = sampleStroke(operation.path(), frequencyToTimeScale, toolConfig);
    if (samples.empty()) {
        return;
    }

    // Dispatch on concrete subtype - see each helper's own docs for what a
    // stamp actually means for that tool type. A plain if/else-if chain,
    // not a visitor, matching every other dynamic_cast-based dispatch in
    // this codebase (operation_log.cpp's Operation subtypes,
    // rebuildPaintedContent() below) - revisit if a third real tool type
    // makes this unwieldy.
    if (const auto* procedural = dynamic_cast<const ProceduralConfiguration*>(&toolConfig)) {
        applyProceduralPaintOperation(operation, *procedural, samples, frequencyToTimeScale, content);
    } else if (const auto* instrument = dynamic_cast<const InstrumentConfiguration*>(&toolConfig)) {
        applyInstrumentPaintOperation(operation, *instrument, samples, content);
    } else if (const auto* mindShot = dynamic_cast<const MindShotConfiguration*>(&toolConfig)) {
        applyMindShotPaintOperation(*mindShot, samples, content);
    } else if (const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(&toolConfig)) {
        applyMindGrainPaintOperation(*mindGrain, samples, resolveLayerContent, content);
    } else if (const auto* heal = dynamic_cast<const HealConfiguration*>(&toolConfig)) {
        applyHealPaintOperation(operation, *heal, samples, frequencyToTimeScale, content);
    } else if (const auto* soften = dynamic_cast<const SoftenConfiguration*>(&toolConfig)) {
        applySoftenPaintOperation(operation, *soften, samples, frequencyToTimeScale, content);
    } else if (const auto* smudge = dynamic_cast<const SmudgeConfiguration*>(&toolConfig)) {
        applySmudgePaintOperation(operation, *smudge, samples, frequencyToTimeScale, content);
    } else if (const auto* orderChaos = dynamic_cast<const OrderChaosConfiguration*>(&toolConfig)) {
        applyOrderChaosPaintOperation(operation, *orderChaos, samples, frequencyToTimeScale, content);
    }
    // Any other/future ToolType (Clone) paints nothing yet - the same
    // "groundwork, not yet functional" state ToolConfiguration's own docs
    // describe for that tool type.
}

sound_mind::codec::StreamImage rebuildPaintedContent(const sound_mind::codec::StreamImage& base,
                                                       const std::vector<const Operation*>& operations,
                                                       double frequencyToTimeScale,
                                                       const LayerContentResolver& resolveLayerContent) {
    sound_mind::codec::StreamImage result = base;
    for (const Operation* operation : operations) {
        if (const auto* paint = dynamic_cast<const PaintOperation*>(operation)) {
            applyPaintOperation(*paint, frequencyToTimeScale, result, resolveLayerContent);
        } else if (const auto* fill = dynamic_cast<const FillOperation*>(operation)) {
            applyFillOperation(*fill, result);
        } else if (const auto* paste = dynamic_cast<const PasteOperation*>(operation)) {
            applyPasteOperation(*paste, result);
        }
    }
    return result;
}

}  // namespace sound_mind::core
