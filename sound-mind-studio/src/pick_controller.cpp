#include "sound_mind/studio/pick_controller.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

namespace {

/// @brief Whether `point` falls within `bounds`, expanded by `timePadding`/
/// `frequencyPadding` on every side - see pick()'s own docs for why a
/// raw, unpadded bounds() isn't always enough on its own.
bool containsPoint(const sound_mind::core::TimeFrequencyRect& bounds, sound_mind::core::TimeFrequencyPoint point,
                    double timePadding, double frequencyPadding) {
    return point.timeSeconds >= bounds.startTimeSeconds - timePadding &&
           point.timeSeconds <= bounds.endTimeSeconds + timePadding &&
           point.frequencyHz >= bounds.lowFrequencyHz - frequencyPadding &&
           point.frequencyHz <= bounds.highFrequencyHz + frequencyPadding;
}

/// @brief How close (in `frequencyToTimeScaleFor()`'s own seconds-
/// equivalent normalized space - see paint_application.h's own docs on
/// why that's the shared yardstick between the two wildly different-
/// scaled axes) a click needs to land to a node/handle to select it -
/// see selectPathNodeNear()'s own docs. A fixed value, not converted from
/// a screen-pixel radius the way the legacy Studio's own Curve tool did:
/// this codebase has no per-view zoom yet for a pixel radius to be
/// meaningful against, and every other hit-test here already works in
/// this same normalized space (see pick()'s own brush-size padding).
constexpr double kNodeHitToleranceSeconds = 0.03;

/// @brief The minimum click forgiveness `pick()` gives every candidate,
/// regardless of type - in the same normalized space as
/// `kNodeHitToleranceSeconds` above. `PaintOperation`'s own brush-size
/// padding (see `pick()`'s own docs) is usually generous enough on its
/// own, but a `FillOperation`/`PasteOperation`'s bounds() already exactly
/// matches its real footprint, which used to mean *zero* padding for
/// those - fine for a visibly-colored Fill or a visibly-different Paste,
/// where a real mouse click naturally lands well inside the visible
/// shape, but not for a Cut's own silence Fill: with nothing rendered to
/// see, there's no way to click confidently away from the exact edge, and
/// a real mouse is far less precise than an automated test's exact pixel.
constexpr double kMinimumPickPaddingSeconds = 0.015;

/// @brief A 2-D vector in `kNodeHitToleranceSeconds`' own seconds-
/// equivalent normalized space (`dt`, `df / scale`) - see
/// normalizedDistanceSquared()'s own docs. Only ever used for direction
/// math (see smoothedHandleTangent() below); never round-tripped back
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

/// @brief How far a freshly-Smoothed corner's own handles extend from its
/// anchor, as a fraction of the distance to its *nearer* neighboring
/// node - self-scales with however closely-spaced the path's own nodes
/// happen to be, rather than one fixed length looking cramped on a widely
/// spaced path or overshooting a tightly spaced one.
constexpr double kSmoothHandleLengthFraction = 1.0 / 3.0;

/// @brief The floor `kSmoothHandleLengthFraction`'s own result is clamped
/// to - comfortably longer than `kNodeHitToleranceSeconds` (see
/// toggleSelectedPathNodeType()'s own docs), so the anchor and its own
/// freshly-extended handles stay individually clickable even when the
/// neighboring nodes themselves are very close by.
constexpr double kMinimumSmoothHandleLengthSeconds = 0.06;

/// @brief The unit tangent direction a freshly-Smoothed node's own
/// `handleOut` should extend along - perpendicular to the bisector of the
/// corner angle `nodes[index]`'s own two neighbors form, so the curve
/// rounds the corner symmetrically rather than pulling toward one side of
/// it - see toggleSelectedPathNodeType()'s own docs. `handleIn` is always
/// this same direction's own mirror.
///
/// For two unit vectors `u` (toward the previous neighbor) and `v`
/// (toward the next one), `(u + v)` (their bisector) and `(v - u)` are
/// always perpendicular - a standard identity, since equal-length
/// vectors satisfy `dot(u+v, v-u) = |v|^2 - |u|^2 = 0`. That means
/// `v - u` already *is* the perpendicular-to-the-bisector direction,
/// oriented toward the next neighbor - no separate bisector construction
/// or sign check needed, and it degrades gracefully: if the node already
/// sits on a straight line between its neighbors (`u == -v`), this
/// correctly returns the straight-through direction rather than an
/// undefined one.
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

/// @brief A closed, four-corner rectangular outline Path tracing `rect` -
/// the live drag preview for a moving non-`PaintOperation` (no real Path
/// of its own to preview) - see currentPreviewPath()'s own docs.
sound_mind::core::Path outlinePathFor(const sound_mind::core::TimeFrequencyRect& rect) {
    sound_mind::core::Path path;
    const auto addCorner = [&path](double timeSeconds, double frequencyHz) {
        sound_mind::core::PathNode node;
        node.anchor = sound_mind::core::TimeFrequencyPoint{timeSeconds, frequencyHz};
        node.type = sound_mind::core::PathNodeType::Corner;
        path.addNode(node);
    };
    addCorner(rect.startTimeSeconds, rect.lowFrequencyHz);
    addCorner(rect.endTimeSeconds, rect.lowFrequencyHz);
    addCorner(rect.endTimeSeconds, rect.highFrequencyHz);
    addCorner(rect.startTimeSeconds, rect.highFrequencyHz);
    addCorner(rect.startTimeSeconds, rect.lowFrequencyHz);  // closes the loop.
    return path;
}

}  // namespace

PickController::PickController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void PickController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    clearSelection();
}

bool PickController::pick(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr) {
        clearSelection();
        return false;
    }
    if (pathEditActive_) {
        return selectPathNodeNear(point);
    }

    const auto operations = project_->operationLog().activeOperationsTargeting(layer);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());

    // Every candidate under this point, most-recent-first (an overlapping
    // newer stroke ordinarily wins over an older one underneath it) -
    // activeOperationsTargeting() itself returns them in stack (back-to-
    // front) order. Any concrete Operation kind is a candidate, not just
    // PaintOperation - only PaintOperation gets its own brush-size
    // padding, since every other kind's bounds() already exactly matches
    // its real footprint (see pick()'s own docs).
    std::vector<const sound_mind::core::Operation*> candidates;
    for (auto it = operations.rbegin(); it != operations.rend(); ++it) {
        const sound_mind::core::Operation* operation = *it;
        double timePadding = kMinimumPickPaddingSeconds;
        if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(operation)) {
            timePadding = std::max(timePadding, paint->config().size());
        }
        const double frequencyPadding = timePadding * scale;
        if (containsPoint(operation->bounds(), point, timePadding, frequencyPadding)) {
            candidates.push_back(operation);
        }
    }

    if (candidates.empty()) {
        clearSelection();
        return false;
    }

    // Clicking a fresh spot selects whatever's topmost there, same as
    // always - but if the *currently selected* object is itself among
    // this click's own candidates, clicking it again cycles to the next
    // one underneath instead of re-selecting the same topmost object
    // every time, which is the only way an object entirely occluded by a
    // larger one on top of it could ever be reached at all. Wraps back to
    // the topmost after the last (occluded-most) candidate.
    const sound_mind::core::Operation* toSelect = candidates.front();
    if (pickedOperationId_.has_value()) {
        const auto currentIt = std::find_if(
            candidates.begin(), candidates.end(),
            [this](const sound_mind::core::Operation* op) { return op->id() == *pickedOperationId_; });
        if (currentIt != candidates.end()) {
            const auto nextIt = std::next(currentIt);
            toSelect = (nextIt != candidates.end()) ? *nextIt : candidates.front();
        }
    }

    pickedOperationId_ = toSelect->id();
    pickedLayer_ = layer;
    pickedOperation_ = toSelect;
    dragAnchor_ = point;
    dragCurrent_ = point;
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    emit selectionChanged();
    return true;
}

void PickController::clearSelection() {
    const bool hadSelection = pickedOperationId_.has_value();
    pickedOperationId_.reset();
    pickedOperation_ = nullptr;
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    // Discards any in-progress path edit too, without committing it - a
    // cleared selection has nothing left to be editing the Path of.
    pathEditActive_ = false;
    selectedNodeIndex_.reset();
    if (hadSelection) {
        emit selectionChanged();
    }
}

std::optional<sound_mind::core::ToolConfiguration> PickController::selectedConfiguration() const {
    if (!pickedOperationId_.has_value()) {
        return std::nullopt;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        return std::nullopt;
    }
    return paint->config();
}

std::optional<sound_mind::core::TimeFrequencyRect> PickController::selectionBounds() const {
    if (!pickedOperationId_.has_value()) {
        return std::nullopt;
    }
    return pickedOperation_->bounds();
}

void PickController::continueMove(sound_mind::core::TimeFrequencyPoint point) {
    if (pathEditActive_) {
        continuePathNodeDrag(point);
        return;
    }
    if (!pickedOperationId_.has_value()) {
        return;
    }
    dragMoved_ = true;
    dragCurrent_ = point;
    const double deltaTimeSeconds = point.timeSeconds - dragAnchor_.timeSeconds;
    const double deltaFrequencyHz = point.frequencyHz - dragAnchor_.frequencyHz;

    if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_)) {
        previewPath_ = paint->path().translated(deltaTimeSeconds, deltaFrequencyHz);
    } else {
        previewPath_ =
            outlinePathFor(sound_mind::core::translated(pickedOperation_->bounds(), deltaTimeSeconds, deltaFrequencyHz));
    }
    emit pathChanged();
}

void PickController::endMove() {
    if (pathEditActive_) {
        // continuePathNodeDrag() already applied the drag directly to
        // previewPath_ - nothing further to do until commitPathEdit()/
        // cancelPathEdit() - see this method's own docs.
        return;
    }
    if (!pickedOperationId_.has_value() || !dragMoved_) {
        dragMoved_ = false;
        if (!previewPath_.nodes().empty()) {
            previewPath_ = sound_mind::core::Path{};
            emit pathChanged();
        }
        return;
    }

    const double deltaTimeSeconds = dragCurrent_.timeSeconds - dragAnchor_.timeSeconds;
    const double deltaFrequencyHz = dragCurrent_.frequencyHz - dragAnchor_.frequencyHz;

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();
    commitReplacement(pickedOperation_->translatedCopy(newId, deltaTimeSeconds, deltaFrequencyHz));

    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
    emit selectionChanged();
}

void PickController::applyToolConfiguration(const sound_mind::core::ToolConfiguration& config) {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        return;  // Only a PaintOperation has a tool configuration to reapply - see this method's own docs.
    }

    sound_mind::core::Path newPath = paint->path();
    // Re-seeded from the new configuration's own default, exactly like a
    // fresh stroke - see ToolConfiguration::defaultGradient()'s own docs.
    newPath.gradient() = config.defaultGradient();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();
    commitReplacement(std::make_unique<sound_mind::core::PaintOperation>(newId, pickedLayer_, std::move(newPath),
                                                                            config, pickedOperationId_));
}

void PickController::deleteSelection() {
    if (pathEditActive_ && selectedNodeIndex_.has_value()) {
        deleteSelectedPathNode();
        return;
    }
    if (!pickedOperationId_.has_value()) {
        return;
    }

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();

    std::unique_ptr<sound_mind::core::Operation> tombstone;
    if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_)) {
        tombstone = std::make_unique<sound_mind::core::PaintOperation>(
            newId, pickedLayer_, sound_mind::core::Path{}, paint->config(), pickedOperationId_);
    } else {
        // FillOperation/PasteOperation can't reduce to a literal zero-
        // effect copy of themselves - see this method's own docs.
        tombstone = std::make_unique<sound_mind::core::FillOperation>(
            newId, pickedLayer_, pickedOperation_->bounds(), sound_mind::core::silenceGradient(), pickedOperationId_);
    }

    commitReplacement(std::move(tombstone));
    clearSelection();
}

void PickController::bringToFront() { reorderSelection(&sound_mind::core::OperationLog::bringToFront); }

void PickController::sendToBack() { reorderSelection(&sound_mind::core::OperationLog::sendToBack); }

void PickController::bringForward() { reorderSelection(&sound_mind::core::OperationLog::bringForward); }

void PickController::sendBackward() { reorderSelection(&sound_mind::core::OperationLog::sendBackward); }

void PickController::reorderSelection(bool (sound_mind::core::OperationLog::*reorder)(sound_mind::core::OperationId)) {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    if ((project_->operationLog().*reorder)(*pickedOperationId_)) {
        paintController_->rebuildLayerContent(pickedLayer_);
        emit contentChanged(pickedLayer_);
    }
}

bool PickController::beginPathEdit() {
    if (!pickedOperationId_.has_value()) {
        return false;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        return false;
    }
    pathEditActive_ = true;
    previewPath_ = paint->path();
    selectedNodeIndex_.reset();
    emit pathChanged();
    return true;
}

bool PickController::selectPathNodeNear(sound_mind::core::TimeFrequencyPoint point) {
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());
    const double toleranceSquared = kNodeHitToleranceSeconds * kNodeHitToleranceSeconds;

    std::optional<std::size_t> bestIndex;
    NodePart bestPart = NodePart::Anchor;
    double bestDistanceSquared = std::numeric_limits<double>::max();

    const auto& nodes = previewPath_.nodes();
    // Handles first - a near-tie between a handle and some node's own
    // anchor favors the handle, matching the design doc's own handle-
    // first editing emphasis (see selectPathNodeNear()'s own docs).
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
        pathEditDragStart_ = previewPath_;
    }
    emit pathChanged();
    return bestIndex.has_value();
}

void PickController::continuePathNodeDrag(sound_mind::core::TimeFrequencyPoint point) {
    if (!selectedNodeIndex_.has_value()) {
        return;
    }
    const double deltaTimeSeconds = point.timeSeconds - dragAnchor_.timeSeconds;
    const double deltaFrequencyHz = point.frequencyHz - dragAnchor_.frequencyHz;

    const sound_mind::core::PathNode originalNode = pathEditDragStart_.nodes().at(*selectedNodeIndex_);
    sound_mind::core::PathNode node = originalNode;

    switch (selectedNodePart_) {
        case NodePart::Anchor:
            // Moving the anchor carries both handles along with it, by
            // the same delta - see continueMove()'s own docs.
            node.anchor.timeSeconds = originalNode.anchor.timeSeconds + deltaTimeSeconds;
            node.anchor.frequencyHz = originalNode.anchor.frequencyHz + deltaFrequencyHz;
            if (originalNode.handleIn.has_value()) {
                node.handleIn = sound_mind::core::TimeFrequencyPoint{
                    originalNode.handleIn->timeSeconds + deltaTimeSeconds,
                    originalNode.handleIn->frequencyHz + deltaFrequencyHz};
            }
            if (originalNode.handleOut.has_value()) {
                node.handleOut = sound_mind::core::TimeFrequencyPoint{
                    originalNode.handleOut->timeSeconds + deltaTimeSeconds,
                    originalNode.handleOut->frequencyHz + deltaFrequencyHz};
            }
            break;
        case NodePart::HandleOut:
        case NodePart::HandleIn: {
            const bool draggingOut = selectedNodePart_ == NodePart::HandleOut;
            const sound_mind::core::TimeFrequencyPoint originalHandle =
                draggingOut ? originalNode.handleOut.value_or(originalNode.anchor)
                            : originalNode.handleIn.value_or(originalNode.anchor);
            const sound_mind::core::TimeFrequencyPoint draggedHandle{originalHandle.timeSeconds + deltaTimeSeconds,
                                                                       originalHandle.frequencyHz + deltaFrequencyHz};
            (draggingOut ? node.handleOut : node.handleIn) = draggedHandle;
            // The opposite handle always mirrors through the anchor, to
            // keep the tangent smooth - no detach-to-corner gesture yet,
            // see the class's own docs.
            const sound_mind::core::TimeFrequencyPoint mirrored{
                2.0 * node.anchor.timeSeconds - draggedHandle.timeSeconds,
                2.0 * node.anchor.frequencyHz - draggedHandle.frequencyHz};
            (draggingOut ? node.handleIn : node.handleOut) = mirrored;
            break;
        }
    }

    previewPath_.setNode(*selectedNodeIndex_, node);
    emit pathChanged();
}

void PickController::deleteSelectedPathNode() {
    if (!pathEditActive_ || !selectedNodeIndex_.has_value()) {
        return;
    }
    if (previewPath_.nodes().size() <= 1) {
        return;  // refuses to edit a path down to nothing mid-session - see this method's own docs.
    }
    previewPath_.removeNode(*selectedNodeIndex_);
    selectedNodeIndex_.reset();
    emit pathChanged();
}

void PickController::toggleSelectedPathNodeType() {
    if (!pathEditActive_ || !selectedNodeIndex_.has_value()) {
        return;
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
        const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());
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
                    nearestNeighborDistance, normalizedDelta(node.anchor, nodes[*selectedNodeIndex_ - 1].anchor, scale).length());
            }
            if (*selectedNodeIndex_ + 1 < nodes.size()) {
                nearestNeighborDistance = std::min(
                    nearestNeighborDistance, normalizedDelta(node.anchor, nodes[*selectedNodeIndex_ + 1].anchor, scale).length());
            }
            const double handleLength = std::max(kMinimumSmoothHandleLengthSeconds,
                                                   kSmoothHandleLengthFraction * nearestNeighborDistance);
            node.handleOut = sound_mind::core::TimeFrequencyPoint{node.anchor.timeSeconds + tangent.dt * handleLength,
                                                                     node.anchor.frequencyHz + tangent.dfNormalized * handleLength * scale};
            node.handleIn = sound_mind::core::TimeFrequencyPoint{node.anchor.timeSeconds - tangent.dt * handleLength,
                                                                    node.anchor.frequencyHz - tangent.dfNormalized * handleLength * scale};
        }
    } else {
        node.type = sound_mind::core::PathNodeType::Corner;
        node.handleIn.reset();
        node.handleOut.reset();
    }
    previewPath_.setNode(*selectedNodeIndex_, node);
    emit pathChanged();
}

void PickController::commitPathEdit() {
    if (!pathEditActive_) {
        return;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        // Defensive: beginPathEdit() only ever activates for a
        // PaintOperation, and nothing else changes pickedOperation_
        // while a path edit is active.
        cancelPathEdit();
        return;
    }

    sound_mind::core::Path editedPath = previewPath_;
    // The original's own gradient carries over unchanged - editing
    // geometry shouldn't silently reset color or opacity.
    editedPath.gradient() = paint->path().gradient();
    const sound_mind::core::ToolConfiguration config = paint->config();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();

    pathEditActive_ = false;
    selectedNodeIndex_.reset();

    commitReplacement(std::make_unique<sound_mind::core::PaintOperation>(newId, pickedLayer_, std::move(editedPath),
                                                                            config, pickedOperationId_));
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
}

void PickController::cancelPathEdit() {
    if (!pathEditActive_) {
        return;
    }
    pathEditActive_ = false;
    selectedNodeIndex_.reset();
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
}

sound_mind::core::OperationId PickController::commitReplacement(
    std::unique_ptr<sound_mind::core::Operation> replacement) {
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = replacement->id();
    log.append(std::move(replacement));

    // The just-appended entry is always the log's own new last element -
    // re-resolved from there rather than kept from the moved-from local,
    // since ownership now belongs to the log.
    pickedOperationId_ = newId;
    pickedOperation_ = &log.at(log.size() - 1);
    pickedLayer_ = *pickedOperation_->targetLayer();

    paintController_->rebuildLayerContent(pickedLayer_);
    emit contentChanged(pickedLayer_);
    return newId;
}

}  // namespace sound_mind::studio
