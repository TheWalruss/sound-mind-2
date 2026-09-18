#pragma once

#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"

namespace sound_mind::core {

/// @brief Which direction a `WarpOperation` displaces content in - see
///        `docs/sound-mind-design.md`'s "Selection" ("Warp").
enum class WarpAxis {
    Frequency,  ///< Vertical shift - each *column* (time position) is displaced by the curve's own frequency deflection there.
    Time,       ///< Horizontal shift - each *row* (frequency position) is displaced by the curve's own time deflection there.
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(WarpAxis, {
    {WarpAxis::Frequency, "frequency"},
    {WarpAxis::Time, "time"},
})
// clang-format on

/// @brief How far along a column/row `WarpOperation` carries the curve's
///        own deflection - see `docs/sound-mind-design.md`'s "Selection"
///        ("Warp").
enum class WarpMode {
    Displace,  ///< Every column/row shifts by its own full deflection.
    Stretch,   ///< The bounding box's own first column/row is unaffected; its last shifts by the full deflection; every one between scales linearly.
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(WarpMode, {
    {WarpMode::Displace, "displace"},
    {WarpMode::Stretch, "stretch"},
})
// clang-format on

/**
 * @brief Displaces a layer's own content within a bounding box along a
 *        curve, logged non-destructively - see
 *        `docs/sound-mind-design.md`'s "Selection" ("Warp"), and
 *        `docs/sound-mind-architecture.md`'s Core Data Model.
 *
 * The fourth concrete `Operation` subtype (after `PaintOperation`,
 * `FillOperation`, `PasteOperation`). Genuinely a *content* operation, not
 * a selection-boundary one, despite living under "Selection" in the
 * design doc - confirmed with the user directly against the legacy
 * Python Studio's own `WarpTool` (`packages/sound_mind_studio/src/
 * sound_mind_studio/tools/warp_pick_tools.py`), which this class's own
 * `applyWarpOperation()` ports faithfully (algorithm, bilinear
 * interpolation, and the exact "Displace"/"Stretch" mode semantics
 * included) - see that function's own docs for the full mechanism.
 *
 * **Always operates on `bounds()`'s own plain bounding box**, regardless
 * of what shape the selection that produced it actually was (Rectangle,
 * rotated Rectangle, Lasso, Wand, or a boolean-combined result) - matches
 * the legacy tool's own scope exactly (its own selection system was
 * always rectangular). A future installment could narrow this to an
 * arbitrary `SelectionRegion` the same way `FillOperation`/
 * `PasteOperation` already do; not attempted here.
 *
 * **The curve is a plain `Path`, an owned snapshot** (same "owned, not
 * shared" reasoning `PaintOperation::path()` already established) -
 * `docs/sound-mind-design.md` doesn't name a dedicated way to author a
 * warp curve, so this reuses whatever `Path` the user already has: draw
 * it as an ordinary paint stroke, then Pick it (`PickController::
 * selectedPath()`) to supply it to Edit → Warp Selection - see that
 * dialog's own docs for the full workflow. No new canvas interaction mode
 * needed for curve authoring at all.
 */
class WarpOperation : public LayerContentOperation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer's content this warp displaces.
     * @param bounds The bounding box to displace within - an owned
     *        snapshot, not a live reference to whatever selection was
     *        active when this was created (same "owned, not shared"
     *        reasoning every other `Operation` subtype already
     *        establishes).
     * @param curve The warp curve - an owned copy.
     * @param axis Which direction content is displaced.
     * @param mode How far along each column/row the deflection carries.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     */
    WarpOperation(OperationId id, LayerId targetLayer, TimeFrequencyRect bounds, Path curve, WarpAxis axis,
                  WarpMode mode, std::optional<OperationId> supersedes = std::nullopt) noexcept
        : LayerContentOperation(id, targetLayer, supersedes),
          bounds_(bounds),
          curve_(std::move(curve)),
          axis_(axis),
          mode_(mode) {}

    /// @brief This operation's own time/frequency footprint - the
    ///        bounding box content is displaced within.
    /// @return This operation's own bounds.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return bounds_; }

    /// @brief The curve content is displaced along.
    /// @return This operation's own curve.
    [[nodiscard]] const Path& curve() const noexcept { return curve_; }

    /// @brief Which direction content is displaced.
    /// @return This operation's own axis.
    [[nodiscard]] WarpAxis axis() const noexcept { return axis_; }

    /// @brief How far along each column/row the deflection carries.
    /// @return This operation's own mode.
    [[nodiscard]] WarpMode mode() const noexcept { return mode_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override {
        return std::make_unique<WarpOperation>(
            newId, targetLayer_, translated(bounds_, deltaTimeSeconds, deltaFrequencyBins, config),
            curve_.translated(deltaTimeSeconds, deltaFrequencyBins, config), axis_, mode_, id());
    }

private:
    TimeFrequencyRect bounds_;
    Path curve_;
    WarpAxis axis_;
    WarpMode mode_;
};

}  // namespace sound_mind::core
