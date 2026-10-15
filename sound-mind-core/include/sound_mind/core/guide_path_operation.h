#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/resonant_instrument.h"

namespace sound_mind::core {

/**
 * @brief A silent path object on a layer - drawn by the Path tool, or made
 *        by merging paths - that paints nothing at all (Pre-Beta Plan
 *        `v0.Y.77.1`).
 *
 * Holds one or more strokes as `BranchGraft`s, the same shape the
 * branching-curve session and `curveGraphFromBranches()` already use:
 * - **one branch** - a regular guide path, which can be Picked, moved,
 *   node-edited, stroked with a brush, or turned into a Resonance Profile;
 * - **several branches** - a branching path, whose later branches graft
 *   onto earlier ones (see `BranchGraft::parentIndex`).
 *
 * Replaying the layer's content skips it entirely (`rebuildPaintedContent()`
 * only applies the kinds that paint), so it never changes what's heard or
 * shown beneath it; the canvas draws it as an overlay instead.
 *
 * A guide path with no branches at all is a deletion tombstone - see
 * `PaintController::makeDeletionTombstone()` - and isn't Pickable.
 */
class GuidePathOperation : public LayerContentOperation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer the guide path belongs to.
     * @param branches The strokes, trunk first; empty for a tombstone.
     * @param supersedes The prior operation this one replaces, if any.
     */
    GuidePathOperation(OperationId id, LayerId targetLayer, std::vector<BranchGraft> branches,
                       std::optional<OperationId> supersedes = std::nullopt) noexcept
        : LayerContentOperation(id, targetLayer, supersedes), branches_(std::move(branches)) {}

    /// @brief Every stroke, trunk first.
    /// @return The branches; empty for a deletion tombstone.
    [[nodiscard]] const std::vector<BranchGraft>& branches() const noexcept { return branches_; }

    /// @brief Whether this is a branching path (more than one stroke).
    /// @return `true` with two or more branches.
    [[nodiscard]] bool isBranching() const noexcept { return branches_.size() > 1; }

    /// @brief Whether this is a deletion tombstone (no strokes at all).
    /// @return `true` with no branches.
    [[nodiscard]] bool isEmpty() const noexcept { return branches_.empty(); }

    /// @brief The union of every branch's own `Path::bounds()`.
    /// @return The footprint; all zero for a tombstone.
    [[nodiscard]] TimeFrequencyRect bounds() const override;

    /// @copydoc Operation::translatedCopy()
    /// @note Every branch's path and graft point move together.
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override;

private:
    std::vector<BranchGraft> branches_;
};

/// @brief Serializes one branch: its path, parent index (if any) and graft point.
void to_json(nlohmann::json& json, const BranchGraft& branch);

/// @brief Parses one branch written by the `to_json()` above.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, BranchGraft& branch);

}  // namespace sound_mind::core
