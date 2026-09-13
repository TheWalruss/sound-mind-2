#include "sound_mind/studio/path_edit_session.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::studio {

namespace {

/// @brief How close (in `frequencyToTimeScaleFor()`'s own seconds-
/// equivalent normalized space) a click needs to land to a node/handle to
/// select it - see `selectNodeNear()`'s own docs. A fixed value, not
/// converted from a screen-pixel radius: this codebase has no per-view
/// zoom yet for a pixel radius to be meaningful against.
constexpr double kNodeHitToleranceSeconds = 0.03;

/// @brief A 2-D vector in `kNodeHitToleranceSeconds`' own seconds-
/// equivalent normalized space (`dt`, `df / scale`) - see
/// `normalizedDistanceSquared()`'s own docs. Only ever used for direction
/// math (see `smoothedHandleTangent()` below); never round-tripped back
/// through a raw `TimeFrequencyPoint` without re-applying `scale`.
struct NormalizedVector {
    double dt = 0.0;
    double dfNormalized = 0.0;

    [[nodiscard]] double length() const noexcept { return std::sqrt(dt * dt + dfNormalized * dfNormalized); }

    [[nodiscard]] NormalizedVector normalized() const {
        const double len = length();
        return len > 1e-9 ? NormalizedVector{dt / len, dfNormalized / len} : NormalizedVector{0.0, 0.0};
    }
};

/// @brief `to - from`, as a `NormalizedVector` in `scale`'s own space.
NormalizedVector normalizedDelta(sound_mind::core::TimeFrequencyPoint from, sound_mind::core::TimeFrequencyPoint to,
                                  double scale) {
    return NormalizedVector{to.timeSeconds - from.timeSeconds, (to.frequencyHz - from.frequencyHz) / scale};
}

/// @brief Squared distance between two points in the same normalized
/// space `kNodeHitToleranceSeconds` is measured in.
double normalizedDistanceSquared(sound_mind::core::TimeFrequencyPoint a, sound_mind::core::TimeFrequencyPoint b,
                                  double scale) {
    const double dt = a.timeSeconds - b.timeSeconds;
    const double df = (a.frequencyHz - b.frequencyHz) / scale;
    return dt * dt + df * df;
}

/// @brief The bin-space distance between `from` and `to`'s own
/// frequencies - what a mouse drag's own frequency delta needs to be
/// measured in, not raw Hz, before handing it to `translateFrequencyByBins()`.
/// The frequency axis is log-scaled (see `frequencyToBinIndex()`'s own
/// docs), so a fixed Hz difference between two mouse positions doesn't
/// correspond to the same on-screen distance everywhere in the frequency
/// range - only the bin-space distance does.
///
/// `PickController::continueMove()`'s own whole-object drag needs this
/// exact same formula and keeps its own copy - a two-line formula isn't
/// worth promoting to a shared header over, the same "duplicated, not
/// shared" reasoning `docs/sound-mind-architecture.md`'s Decision #59
/// already gives for `dbToLinearAmplitude()`/`linearAmplitudeToDb()`.
double frequencyBinDelta(sound_mind::core::TimeFrequencyPoint from, sound_mind::core::TimeFrequencyPoint to,
                          const sound_mind::codec::StreamCodecConfig& config) {
    return sound_mind::core::frequencyToBinIndex(static_cast<float>(to.frequencyHz), config) -
           sound_mind::core::frequencyToBinIndex(static_cast<float>(from.frequencyHz), config);
}

/// @brief How far a freshly-Smoothed corner's own handles extend from its
/// anchor, as a fraction of the distance to its *nearer* neighboring
/// node - self-scales with however closely-spaced the path's own nodes
/// happen to be, rather than one fixed length looking cramped on a widely
/// spaced path or overshooting a tightly spaced one.
constexpr double kSmoothHandleLengthFraction = 1.0 / 3.0;

/// @brief The floor `kSmoothHandleLengthFraction`'s own result is clamped
/// to - comfortably longer than `kNodeHitToleranceSeconds` (see
/// `toggleSelectedNodeType()`'s own docs), so the anchor and its own
/// freshly-extended handles stay individually clickable even when the
/// neighboring nodes themselves are very close by.
constexpr double kMinimumSmoothHandleLengthSeconds = 0.06;

/// @brief The unit tangent direction a freshly-Smoothed node's own
/// `handleOut` should extend along - perpendicular to the bisector of the
/// corner angle `nodes[index]`'s own two neighbors form, so the curve
/// rounds the corner symmetrically rather than pulling toward one side of
/// it - see `toggleSelectedNodeType()`'s own docs. `handleIn` is always
/// this same direction's own mirror.
///
/// For two unit vectors `u` (toward the previous neighbor) and `v`
/// (toward the next one), `(u + v)` (their bisector) and `(v - u)` are
/// always perpendicular - a standard identity, since equal-length vectors
/// satisfy `dot(u+v, v-u) = |v|^2 - |u|^2 = 0`. That means `v - u` already
/// *is* the perpendicular-to-the-bisector direction, oriented toward the
/// next neighbor - no separate bisector construction or sign check
/// needed, and it degrades gracefully: if the node already sits on a
/// straight line between its neighbors (`u == -v`), this correctly
/// returns the straight-through direction rather than an undefined one.
///
/// @param index The node to compute a tangent for - must have at least
///        one neighbor in `nodes` (an isolated single-node path has no
///        direction to extend toward at all; the caller handles that
///        case itself).
/// @param nodes The full path's own nodes.
/// @param scale `frequencyToTimeScaleFor()`'s own result, converting
///        frequency deltas into this same normalized space.
NormalizedVector smoothedHandleTangent(std::size_t index, const std::vector<sound_mind::core::PathNode>& nodes,
                                        double scale) {
    const bool hasPrev = index > 0;
    const bool hasNext = index + 1 < nodes.size();
    const sound_mind::core::TimeFrequencyPoint anchor = nodes[index].anchor;

    if (hasPrev && hasNext) {
        const NormalizedVector toPrev = normalizedDelta(anchor, nodes[index - 1].anchor, scale).normalized();
        const NormalizedVector toNext = normalizedDelta(anchor, nodes[index + 1].anchor, scale).normalized();
        return NormalizedVector{toNext.dt - toPrev.dt, toNext.dfNormalized - toPrev.dfNormalized}.normalized();
    }
    if (hasNext) {
        // The path's own first node - handleOut simply continues toward
        // its only neighbor.
        return normalizedDelta(anchor, nodes[index + 1].anchor, scale).normalized();
    }
    if (hasPrev) {
        // The path's own last node - handleOut continues *away* from its
        // only neighbor, the same forward direction the path was already
        // heading in when it arrived here.
        const NormalizedVector toPrev = normalizedDelta(anchor, nodes[index - 1].anchor, scale).normalized();
        return NormalizedVector{-toPrev.dt, -toPrev.dfNormalized};
    }
    return NormalizedVector{0.0, 0.0};
}

}  // namespace

void PathEditSession::begin(const sound_mind::core::Path& initialPath) {
    active_ = true;
    previewPath_ = initialPath;
    selectedNodeIndex_.reset();
}

void PathEditSession::end() {
    active_ = false;
    previewPath_ = sound_mind::core::Path{};
    selectedNodeIndex_.reset();
}

bool PathEditSession::selectNodeNear(sound_mind::core::TimeFrequencyPoint point, double scale) {
    const double toleranceSquared = kNodeHitToleranceSeconds * kNodeHitToleranceSeconds;

    std::optional<std::size_t> bestIndex;
    NodePart bestPart = NodePart::Anchor;
    double bestDistanceSquared = std::numeric_limits<double>::max();

    const auto& nodes = previewPath_.nodes();
    // Handles first - a near-tie between a handle and some node's own
    // anchor favors the handle, matching the design doc's own handle-
    // first editing emphasis (see this method's own docs).
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const sound_mind::core::PathNode& node = nodes[i];
        if (node.type != sound_mind::core::PathNodeType::Smooth) {
            continue;
        }
        if (node.handleOut.has_value()) {
            const double distanceSquared = normalizedDistanceSquared(*node.handleOut, point, scale);
            if (distanceSquared <= toleranceSquared && distanceSquared < bestDistanceSquared) {
                bestDistanceSquared = distanceSquared;
                bestIndex = i;
                bestPart = NodePart::HandleOut;
            }
        }
        if (node.handleIn.has_value()) {
            const double distanceSquared = normalizedDistanceSquared(*node.handleIn, point, scale);
            if (distanceSquared <= toleranceSquared && distanceSquared < bestDistanceSquared) {
                bestDistanceSquared = distanceSquared;
                bestIndex = i;
                bestPart = NodePart::HandleIn;
            }
        }
    }
    if (!bestIndex.has_value()) {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const double distanceSquared = normalizedDistanceSquared(nodes[i].anchor, point, scale);
            if (distanceSquared <= toleranceSquared && distanceSquared < bestDistanceSquared) {
                bestDistanceSquared = distanceSquared;
                bestIndex = i;
                bestPart = NodePart::Anchor;
            }
        }
    }

    selectedNodeIndex_ = bestIndex;
    if (bestIndex.has_value()) {
        selectedNodePart_ = bestPart;
        dragAnchor_ = point;
        dragStart_ = previewPath_;
    }
    return bestIndex.has_value();
}

bool PathEditSession::continueDrag(sound_mind::core::TimeFrequencyPoint point,
                                    const sound_mind::codec::StreamCodecConfig& config) {
    if (!selectedNodeIndex_.has_value()) {
        return false;
    }
    const double deltaTimeSeconds = point.timeSeconds - dragAnchor_.timeSeconds;
    const double deltaFrequencyBins = frequencyBinDelta(dragAnchor_, point, config);
    // Shifts one point by the drag's own delta - time linearly, frequency
    // via its own bin position (see frequencyBinDelta()'s/
    // translateFrequencyByBins()'s own docs for why a raw Hz offset isn't
    // enough).
    const auto shifted = [&](sound_mind::core::TimeFrequencyPoint original) {
        return sound_mind::core::TimeFrequencyPoint{
            original.timeSeconds + deltaTimeSeconds,
            sound_mind::core::translateFrequencyByBins(static_cast<float>(original.frequencyHz), deltaFrequencyBins,
                                                          config)};
    };

    const sound_mind::core::PathNode originalNode = dragStart_.nodes().at(*selectedNodeIndex_);
    sound_mind::core::PathNode node = originalNode;

    switch (selectedNodePart_) {
        case NodePart::Anchor:
            // Moving the anchor carries both handles along with it, by
            // the same delta.
            node.anchor = shifted(originalNode.anchor);
            if (originalNode.handleIn.has_value()) {
                node.handleIn = shifted(*originalNode.handleIn);
            }
            if (originalNode.handleOut.has_value()) {
                node.handleOut = shifted(*originalNode.handleOut);
            }
            break;
        case NodePart::HandleOut:
        case NodePart::HandleIn: {
            const bool draggingOut = selectedNodePart_ == NodePart::HandleOut;
            const sound_mind::core::TimeFrequencyPoint originalHandle =
                draggingOut ? originalNode.handleOut.value_or(originalNode.anchor)
                            : originalNode.handleIn.value_or(originalNode.anchor);
            const sound_mind::core::TimeFrequencyPoint draggedHandle = shifted(originalHandle);
            (draggingOut ? node.handleOut : node.handleIn) = draggedHandle;
            // The opposite handle always mirrors through the anchor, to
            // keep the tangent smooth - no detach-to-corner gesture yet.
            const sound_mind::core::TimeFrequencyPoint mirrored{
                2.0 * node.anchor.timeSeconds - draggedHandle.timeSeconds,
                2.0 * node.anchor.frequencyHz - draggedHandle.frequencyHz};
            (draggingOut ? node.handleIn : node.handleOut) = mirrored;
            break;
        }
    }

    previewPath_.setNode(*selectedNodeIndex_, node);
    return true;
}

bool PathEditSession::deleteSelectedNode() {
    if (!selectedNodeIndex_.has_value()) {
        return false;
    }
    if (previewPath_.nodes().size() <= 1) {
        return false;  // refuses to edit a path down to nothing mid-session - see this method's own docs.
    }
    previewPath_.removeNode(*selectedNodeIndex_);
    selectedNodeIndex_.reset();
    return true;
}

bool PathEditSession::toggleSelectedNodeType(const sound_mind::core::ProjectSettings& settings) {
    if (!selectedNodeIndex_.has_value()) {
        return false;
    }
    const auto& nodes = previewPath_.nodes();
    sound_mind::core::PathNode node = nodes.at(*selectedNodeIndex_);
    if (node.type == sound_mind::core::PathNodeType::Corner) {
        node.type = sound_mind::core::PathNodeType::Smooth;
        // Extended a comfortable distance from the anchor, along the
        // tangent that rounds this corner symmetrically between its own
        // two neighbors - see smoothedHandleTangent()'s own docs. Handles
        // collapsed onto the anchor itself (PathNode's own prior
        // precedent for a freshly-placed Smooth node - see path.h) are
        // invisible until dragged out, and impossible to grab precisely
        // since they sit exactly on top of the anchor's own, larger dot;
        // an isolated single-node path (no neighbor to take a direction
        // from) is the one case that still falls back to that collapsed
        // placement, for lack of any better direction to extend toward.
        const double scale = sound_mind::core::frequencyToTimeScaleFor(settings);
        const NormalizedVector tangent = smoothedHandleTangent(*selectedNodeIndex_, nodes, scale);
        if (tangent.dt == 0.0 && tangent.dfNormalized == 0.0) {
            node.handleIn = node.anchor;
            node.handleOut = node.anchor;
        } else {
            // kSmoothHandleLengthFraction of the distance to the *nearer*
            // of the two neighbors (whichever neighbors actually exist),
            // floored at kMinimumSmoothHandleLengthSeconds.
            double nearestNeighborDistance = std::numeric_limits<double>::max();
            if (*selectedNodeIndex_ > 0) {
                nearestNeighborDistance = std::min(
                    nearestNeighborDistance,
                    normalizedDelta(node.anchor, nodes[*selectedNodeIndex_ - 1].anchor, scale).length());
            }
            if (*selectedNodeIndex_ + 1 < nodes.size()) {
                nearestNeighborDistance = std::min(
                    nearestNeighborDistance,
                    normalizedDelta(node.anchor, nodes[*selectedNodeIndex_ + 1].anchor, scale).length());
            }
            const double handleLength =
                std::max(kMinimumSmoothHandleLengthSeconds, kSmoothHandleLengthFraction * nearestNeighborDistance);
            node.handleOut = sound_mind::core::TimeFrequencyPoint{
                node.anchor.timeSeconds + tangent.dt * handleLength,
                node.anchor.frequencyHz + tangent.dfNormalized * handleLength * scale};
            node.handleIn = sound_mind::core::TimeFrequencyPoint{
                node.anchor.timeSeconds - tangent.dt * handleLength,
                node.anchor.frequencyHz - tangent.dfNormalized * handleLength * scale};
        }
    } else {
        node.type = sound_mind::core::PathNodeType::Corner;
        node.handleIn.reset();
        node.handleOut.reset();
    }
    previewPath_.setNode(*selectedNodeIndex_, node);
    return true;
}

}  // namespace sound_mind::studio
