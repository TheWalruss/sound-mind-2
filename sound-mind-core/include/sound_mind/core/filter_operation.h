#pragma once

#include <memory>
#include <optional>

#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/selection_region.h"

namespace sound_mind::core {

/**
 * @brief A filter effect confined to a selection's own boundary, logged
 *        non-destructively - real-world testing pass finding #33
 *        ("Layers Panel & Editing Enhancements v2" Installment E, `v0.Y.46.1`).
 *
 * The fifth concrete `Operation` subtype (`PaintOperation`/`FillOperation`/
 * `PasteOperation`/`SequenceOperation` came before it). Unlike a Filter
 * *layer* (`sound_mind::core::isFilterLayerType()`), which recomposits and
 * re-applies its own filter live, every time the canvas changes -
 * `FilterOperation` is a one-shot bake: `applyFilterOperation()` runs
 * `config()`'s own filter once, across the *whole* layer (so spatially-
 * aware filters like blur/convolve read real neighboring cells, not
 * silence past the selection's own edge), then keeps the result only for
 * cells actually inside `bounds()`/`boundary()` - every cell outside the
 * selection is left completely untouched, exactly the same "confined to
 * a boundary" contract `FillOperation`/`PasteOperation` already establish.
 */
class FilterOperation : public LayerContentOperation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer's content this filter writes into.
     * @param bounds The selection's own time/frequency extent - an owned
     *        snapshot, not a live reference to whatever the current
     *        selection happens to be later (the same "owned, not shared"
     *        reasoning `FillOperation`'s own `bounds` already establishes).
     *        Always this operation's own bounding box, even when
     *        `boundary` narrows it to a non-rectangular shape.
     * @param config The filter configuration applied - an owned snapshot,
     *        same reasoning as `bounds`. Its own bindable parameters (if
     *        any are MindWave-bound) still resolve fresh on every replay,
     *        the same "live reference, not a snapshot" contract every
     *        other MindWave binding in this codebase already keeps - only
     *        the filter *type* and its own scalar/binding configuration
     *        are frozen at the moment this operation was created.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     * @param boundary A non-rectangular selection's own precise shape,
     *        narrowing which cells within `bounds` this filter actually
     *        touches - see `boundary()`'s own docs. `std::nullopt` (the
     *        default) for a plain Rectangle-shaped selection.
     */
    FilterOperation(OperationId id, LayerId targetLayer, TimeFrequencyRect bounds, FilterConfiguration config,
                    std::optional<OperationId> supersedes = std::nullopt,
                    std::optional<SelectionRegion> boundary = std::nullopt) noexcept
        : LayerContentOperation(id, targetLayer, supersedes),
          bounds_(bounds),
          config_(std::move(config)),
          boundary_(std::move(boundary)) {}

    /// @brief This operation's own time/frequency footprint - exactly the
    ///        selection it was applied through, since a filter confined
    ///        to a selection never affects anything outside it. Always
    ///        the *bounding box*, even for a non-rectangular selection -
    ///        see boundary()'s own docs.
    /// @return This operation's own bounds.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return bounds_; }

    /// @brief The filter configuration this operation was applied with.
    /// @return This operation's own configuration.
    [[nodiscard]] const FilterConfiguration& config() const noexcept { return config_; }

    /**
     * @brief A non-rectangular selection's own precise shape, if this
     *        filter was applied through one - narrows `applyFilterOperation()`'s
     *        own edit to cells `boundary()->containsCell()` actually
     *        places inside it, rather than every cell in `bounds()`'s own
     *        rectangle.
     * @return The boundary, or `std::nullopt` for a plain Rectangle-shaped
     *         selection (every cell in `bounds()` is touched).
     */
    [[nodiscard]] const std::optional<SelectionRegion>& boundary() const noexcept { return boundary_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override {
        std::optional<SelectionRegion> translatedBoundary;
        if (boundary_) {
            translatedBoundary = boundary_->translated(deltaTimeSeconds, deltaFrequencyBins, config);
        }
        return std::make_unique<FilterOperation>(
            newId, targetLayer_, translated(bounds_, deltaTimeSeconds, deltaFrequencyBins, config), config_, id(),
            std::move(translatedBoundary));
    }

private:
    TimeFrequencyRect bounds_;
    FilterConfiguration config_;
    std::optional<SelectionRegion> boundary_;
};

}  // namespace sound_mind::core
