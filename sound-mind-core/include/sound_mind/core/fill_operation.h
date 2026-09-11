#pragma once

#include <memory>
#include <optional>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation.h"

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
 */
class FillOperation : public Operation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer's content this fill writes into.
     * @param bounds The selection's own time/frequency extent - an owned
     *        snapshot, not a live reference to whatever the current
     *        selection happens to be later (the same "owned, not shared"
     *        reasoning `PaintOperation`'s own `Path`/`ToolConfiguration`
     *        already established).
     * @param gradient The color (or gradient) painted across `bounds` -
     *        an owned snapshot, same reasoning as `bounds`.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     */
    FillOperation(OperationId id, LayerId targetLayer, TimeFrequencyRect bounds, Gradient gradient,
                  std::optional<OperationId> supersedes = std::nullopt) noexcept
        : Operation(id, supersedes), targetLayer_(targetLayer), bounds_(bounds), gradient_(std::move(gradient)) {}

    /// @brief This operation's own time/frequency footprint - exactly the
    ///        selection it was filled through, since a fill never affects
    ///        anything outside it.
    /// @return This operation's own bounds.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return bounds_; }

    /// @brief Which layer this fill wrote into.
    /// @return This operation's own target layer id.
    [[nodiscard]] std::optional<LayerId> targetLayer() const noexcept override { return targetLayer_; }

    /// @brief The color (or gradient) this fill was applied with.
    /// @return This operation's own gradient.
    [[nodiscard]] const Gradient& gradient() const noexcept { return gradient_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override {
        return std::make_unique<FillOperation>(
            newId, targetLayer_, translated(bounds_, deltaTimeSeconds, deltaFrequencyBins, config), gradient_, id());
    }

private:
    LayerId targetLayer_;
    TimeFrequencyRect bounds_;
    Gradient gradient_;
};

}  // namespace sound_mind::core
