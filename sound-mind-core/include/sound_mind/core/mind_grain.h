#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"

namespace sound_mind::core {

class Project;

/// @brief Opaque identifier for a `NamedMindGrain` within a Project - see
/// `MindShotId`'s own docs for the same pattern, applied here instead to
/// `docs/sound-mind-design.md`'s "Mind Grains".
using MindGrainId = std::uint64_t;

/**
 * @brief A named, permanently-stored *reference* to a region on a layer -
 *        `v0.Y.33.1` Installment B, per `docs/sound-mind-design.md`'s
 *        "Mind Grains": "stores a reference to a selection on a layer,
 *        rather than a captured copy... a fresh grain is drawn live from
 *        the referenced selection... so moving or repainting the
 *        selection, or editing the source layer, changes the stamp on
 *        every layer that uses it."
 *
 * **The deliberate opposite of `NamedMindShot`**: where a Mind Shot
 * embeds a `Clip` (a permanent, immutable snapshot), a Mind Grain embeds
 * no pixel content at all - only `sourceLayerId`/`bounds`, resolved fresh
 * from `sourceLayerId`'s own *current* content every time a Mind-Grain-
 * configured stroke is actually rendered (see `MindGrainConfiguration`'s
 * own docs, and `applyPaintOperation()`'s `MindGrainConfiguration`
 * branch). `bounds` itself is fixed once captured (there's no UI yet to
 * move a Mind Grain's own referenced region after the fact - see this
 * class's own "not yet built" note below) - it's the *pixel content*
 * within that fixed region that's live, not the region's own position.
 *
 * **Only usable as a brush on a layer above `sourceLayerId`** - enforced
 * where a Mind-Grain-configured stroke actually starts
 * (`PaintController::beginStroke()`) and, defensively, wherever a layer
 * reorder/removal could break an *already-painted* stroke's own relative
 * ordering (see `mindGrainOperationsBrokenByRemovingLayer()`/
 * `mindGrainOperationsBrokenByReorder()`) - never retroactively enforced
 * against this struct's own definition here, which has no ordering
 * requirement of its own until something actually paints with it.
 *
 * **Not yet built**: repositioning a captured Mind Grain's own `bounds`
 * after the fact (the design doc's own "moving... the selection" case) -
 * there's no Mind Grain library management UI yet, the same "capture-and-
 * pick, nothing else" scope Mind Shots' own Installment A settled for.
 */
struct NamedMindGrain {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addMindGrain()`, not meant to be picked by hand.
    MindGrainId id = 0;
    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;
    /// @brief Which layer this grain reads its live content from.
    LayerId sourceLayerId = 0;
    /// @brief The referenced region, in time/frequency space - resolved
    ///        against `sourceLayerId`'s own *current* content fresh at
    ///        every render, not captured once.
    TimeFrequencyRect bounds;
};

/// @brief Serializes a named Mind Grain to its JSON representation.
void to_json(nlohmann::json& json, const NamedMindGrain& namedMindGrain);

/// @brief Parses a named Mind Grain from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedMindGrain& namedMindGrain);

/**
 * @brief Whether `layer` currently sits above `other` in `project`'s own
 *        stack order (bottom-to-top) - the ordering rule a Mind-Grain-
 *        configured stroke must satisfy against its own grain's
 *        `sourceLayerId` (`docs/sound-mind-design.md`'s "Mind Grains":
 *        "can be used as a brush stamp only on layers above its source").
 *
 * Shared by every place that needs this same check against the
 * *current* stack (`PaintController::beginStroke()`'s own guard, and the
 * Studio UI's own proactive indicators - a picker's red highlight, a
 * Layers Panel row's red mark, the Paint action's own enabled state) -
 * `mindGrainOperationsBrokenByReorder()` below is the equivalent check
 * against a *proposed*, not-yet-applied stack instead.
 *
 * @param project The project both layers belong to.
 * @param layer The candidate "target" layer.
 * @param other The candidate "source" layer.
 * @return `true` if both ids exist in `project` and `layer` is strictly
 *         above `other`; `false` otherwise (including either id not
 *         existing, or `layer == other`).
 */
[[nodiscard]] bool isLayerAbove(const Project& project, LayerId layer, LayerId other) noexcept;

/**
 * @brief Every currently-active `PaintOperation` (by id) whose own
 *        `MindGrainConfiguration` would break if `layerToRemove` were
 *        removed from `project` - its own `sourceLayerId` would cease to
 *        exist. Meant to be checked *before* actually calling
 *        `Project::removeLayer()`, so the caller (Studio's own layer-
 *        deletion UI) can refuse the deletion and explain why instead of
 *        silently leaving a Mind Grain stroke with a dangling source.
 *
 * @param project The project to check.
 * @param layerToRemove The layer a caller is considering removing.
 * @return Every broken operation's own id; empty if removing that layer
 *         would break nothing.
 */
[[nodiscard]] std::vector<OperationId> mindGrainOperationsBrokenByRemovingLayer(const Project& project,
                                                                                  LayerId layerToRemove);

/**
 * @brief Every currently-active `PaintOperation` (by id) whose own
 *        `MindGrainConfiguration` would break under `newOrderBottomToTop`
 *        - its own target layer would no longer be strictly above its
 *        grain's `sourceLayerId`. Meant to be checked *before* actually
 *        calling `Project::reorderLayers()`, the same "ask first" use as
 *        `mindGrainOperationsBrokenByRemovingLayer()`.
 *
 * @param project The project to check (its own *current* layer stack is
 *        only used to resolve which layer each active operation targets -
 *        `newOrderBottomToTop` is what's actually evaluated against).
 * @param newOrderBottomToTop The proposed new stack order - every layer
 *        currently in `project`, exactly once each, bottom-to-top (the
 *        same shape `Project::reorderLayers()` itself takes). An
 *        operation whose target or source layer is missing from this
 *        list is conservatively treated as broken.
 * @return Every broken operation's own id; empty if the proposed reorder
 *         would break nothing.
 */
[[nodiscard]] std::vector<OperationId> mindGrainOperationsBrokenByReorder(
    const Project& project, const std::vector<LayerId>& newOrderBottomToTop);

/**
 * @brief Every layer (by id, each appearing at most once) with at least one
 *        currently-active `PaintOperation` whose own `MindGrainConfiguration`
 *        reads live from `sourceLayer` - the layers a repaint of
 *        `sourceLayer` needs to immediately cascade a rebuild into, so a
 *        Mind Grain stroke reflects its source's own new content right
 *        away rather than only the next time its *own* layer happens to
 *        rebuild for an unrelated reason.
 *
 * Direct dependents only - not transitive. `PaintController::
 * rebuildLayerContent()` is what actually walks the full cascade (calling
 * this again for each dependent layer it just rebuilt, to find *that*
 * layer's own dependents in turn) - this function only ever answers "who
 * depends on this one layer," the same single-hop shape
 * `mindGrainOperationsBrokenByRemovingLayer()` above already has (it
 * likewise only checks a `sourceLayerId` match, not any transitive chain).
 *
 * @param project The project to check.
 * @param sourceLayer The layer whose content just changed.
 * @return Every directly-dependent layer's own id; empty if nothing reads
 *         live from `sourceLayer`.
 */
[[nodiscard]] std::vector<LayerId> layersWithMindGrainOperationsSourcedFrom(const Project& project,
                                                                              LayerId sourceLayer);

}  // namespace sound_mind::core
