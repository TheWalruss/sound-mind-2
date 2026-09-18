#pragma once

#include <memory>
#include <optional>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"

namespace sound_mind::core {

/**
 * @brief A solid fill confined to a selection's own boundary, logged
 *        non-destructively - see `docs/sound-mind-design.md`'s "Fill",
 *        and `docs/sound-mind-architecture.md`'s Core Data Model.
 *
 * The second concrete `Operation` subtype (`PaintOperation` was the
 * first - see `OperationLog`'s own docs on why that meant a real
 * `kind`-based JSON dispatch, rather than a speculative one, only
 * arrived once a second subtype actually existed). Unlike a brush
 * stroke, a fill has no falloff or stamp shape: `applyFillOperation()`
 * writes `gradient()`'s own color across every bin/frame cell strictly
 * within `bounds()`, confined exactly to the selection - "a solid fill",
 * not "a very fat brush".
 *
 * **Lasso-shaped fills** (`v0.Y.35.1` Installment A): `bounds()` alone
 * is always this fill's own bounding box - the same rectangle it would
 * be for a Rectangle-shaped selection - regardless of `boundary()`.
 * Every other bounding-box-consuming caller (`PickController`'s own hit-
 * testing, "Show bounding boxes", the layer-reordering guardrails) keeps
 * working completely unchanged whether or not `boundary()` is present.
 * `boundary()`, when present, additionally *narrows* which cells within
 * that bounding box `applyFillOperation()` actually touches - see its
 * own docs.
 */
class FillOperation : public LayerContentOperation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer's content this fill writes into.
     * @param bounds The selection's own time/frequency extent - an owned
     *        snapshot, not a live reference to whatever the current
     *        selection happens to be later (the same "owned, not shared"
     *        reasoning `PaintOperation`'s own `Path`/`ToolConfiguration`
     *        already established). Always this fill's own bounding box,
     *        even when `boundary` narrows it to a non-rectangular shape.
     * @param gradient The color (or gradient) painted across `bounds` -
     *        an owned snapshot, same reasoning as `bounds`.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     * @param boundary A Lasso selection's own closed curve, narrowing
     *        which cells within `bounds` this fill actually touches - see
     *        `boundary()`'s own docs. `std::nullopt` (the default) for a
     *        plain Rectangle-shaped fill, unchanged from every fill this
     *        class supported before `v0.Y.35.1`.
     */
    FillOperation(OperationId id, LayerId targetLayer, TimeFrequencyRect bounds, Gradient gradient,
                  std::optional<OperationId> supersedes = std::nullopt,
                  std::optional<Path> boundary = std::nullopt) noexcept
        : LayerContentOperation(id, targetLayer, supersedes),
          bounds_(bounds),
          gradient_(std::move(gradient)),
          boundary_(std::move(boundary)) {}

    /// @brief This operation's own time/frequency footprint - exactly the
    ///        selection it was filled through, since a fill never affects
    ///        anything outside it. Always the *bounding box*, even for a
    ///        Lasso-shaped fill - see boundary()'s own docs and this
    ///        class's own docs on why.
    /// @return This operation's own bounds.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return bounds_; }

    /// @brief The color (or gradient) this fill was applied with.
    /// @return This operation's own gradient.
    [[nodiscard]] const Gradient& gradient() const noexcept { return gradient_; }

    /**
     * @brief A Lasso selection's own closed curve, if this fill was made
     *        through one - narrows `applyFillOperation()`'s own edit to
     *        cells `sound_mind::core::containsPoint()` actually places
     *        inside it, rather than every cell in `bounds()`'s own
     *        rectangle.
     * @return The boundary curve, or `std::nullopt` for a plain
     *         Rectangle-shaped fill (every cell in `bounds()` is
     *         touched, exactly as before this field existed).
     */
    [[nodiscard]] const std::optional<Path>& boundary() const noexcept { return boundary_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override {
        std::optional<Path> translatedBoundary;
        if (boundary_) {
            translatedBoundary = boundary_->translated(deltaTimeSeconds, deltaFrequencyBins, config);
        }
        return std::make_unique<FillOperation>(
            newId, targetLayer_, translated(bounds_, deltaTimeSeconds, deltaFrequencyBins, config), gradient_, id(),
            std::move(translatedBoundary));
    }

private:
    TimeFrequencyRect bounds_;
    Gradient gradient_;
    std::optional<Path> boundary_;
};

}  // namespace sound_mind::core
