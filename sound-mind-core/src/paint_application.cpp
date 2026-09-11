#include "sound_mind/core/paint_application.h"

#include <algorithm>
#include <cmath>

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

/// @brief Evaluates a cubic Bézier curve at parameter `t` in `[0, 1]`.
TimeFrequencyPoint evaluateCubicBezier(const TimeFrequencyPoint& p0, const TimeFrequencyPoint& p1,
                                        const TimeFrequencyPoint& p2, const TimeFrequencyPoint& p3, double t) {
    const double u = 1.0 - t;
    const double w0 = u * u * u;
    const double w1 = 3.0 * u * u * t;
    const double w2 = 3.0 * u * t * t;
    const double w3 = t * t * t;
    return TimeFrequencyPoint{w0 * p0.timeSeconds + w1 * p1.timeSeconds + w2 * p2.timeSeconds + w3 * p3.timeSeconds,
                               w0 * p0.frequencyHz + w1 * p1.frequencyHz + w2 * p2.frequencyHz +
                                   w3 * p3.frequencyHz};
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

std::vector<StrokeSample> sampleStroke(const Path& path, double frequencyToTimeScale) {
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

}  // namespace

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

void applyPaintOperation(const PaintOperation& operation, double frequencyToTimeScale,
                          sound_mind::codec::StreamImage& content) {
    if (frequencyToTimeScale <= 0.0 || content.frameCount == 0 || content.config.binCount == 0) {
        return;
    }

    const ToolConfiguration& toolConfig = operation.config();
    const std::vector<StrokeSample> samples = sampleStroke(operation.path(), frequencyToTimeScale);
    if (samples.empty()) {
        return;
    }

    // The tip's own radius, converted from its seconds-equivalent
    // normalized size (see ToolConfiguration::size()'s own docs) into
    // real frame/bin units - the frame radius is a fixed conversion
    // (timeToFrameIndex() is linear), but the bin radius is only
    // meaningful *locally*, re-derived at each stamp's own center
    // frequency below, since the frequency axis is log-scaled (equal Hz
    // spans don't cover equal bin counts everywhere on it).
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

                const std::size_t index = static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
                float& left = content.leftMagnitudeDb[index];
                float& right = content.rightMagnitudeDb[index];
                left += (target.leftIntensity - left) * (target.leftOpacity * weight);
                right += (target.rightIntensity - right) * (target.rightOpacity * weight);
            }
        }
    }
}

sound_mind::codec::StreamImage rebuildPaintedContent(const sound_mind::codec::StreamImage& base,
                                                       const std::vector<const Operation*>& operations,
                                                       double frequencyToTimeScale) {
    sound_mind::codec::StreamImage result = base;
    for (const Operation* operation : operations) {
        if (const auto* paint = dynamic_cast<const PaintOperation*>(operation)) {
            applyPaintOperation(*paint, frequencyToTimeScale, result);
        } else if (const auto* fill = dynamic_cast<const FillOperation*>(operation)) {
            applyFillOperation(*fill, result);
        } else if (const auto* paste = dynamic_cast<const PasteOperation*>(operation)) {
            applyPasteOperation(*paste, result);
        }
    }
    return result;
}

}  // namespace sound_mind::core
