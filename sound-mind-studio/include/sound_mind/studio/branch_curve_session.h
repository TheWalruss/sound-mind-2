#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/resonant_instrument.h"

namespace sound_mind::studio {

/**
 * @brief Owns the state of an in-progress, hand-drawn branching curve -
 *        the deferred half of `docs/sound-mind-roadmap.md`'s "Resonant
 *        Instruments" item 1, finally built: "letting someone build a
 *        real tree/graph by hand... rather than only resampling an
 *        existing linear stroke."
 *
 * A plain value type, not a `QObject` - the same shape `PathEditSession`'s
 * own docs establish for exactly the same reason: `MainWindow` calls in,
 * reads back a `bool`/accessor, and decides what (if anything) to tell the
 * user; this class emits nothing and knows nothing about `Project`,
 * `OperationLog`, or Qt.
 *
 * **Workflow** (confirmed with the user): draw a stroke with the ordinary
 * Paint tool, switch to Pick and select it, then run an Edit action to
 * declare it this session's trunk (`start()`) or (for every subsequent
 * stroke) a new branch (`addBranch()`). Before adding a branch, Pick a
 * *point* on whichever existing branch it should grow out of - every
 * ordinary Pick click, while a session is active, is reported here via
 * `noteGraftCandidate()` regardless of what it actually selects; only a
 * click that lands on one of this session's own already-added branches
 * (matched by its `OperationId`, not by comparing `Path` values - a stroke
 * can be re-picked any number of times without losing its own identity)
 * actually arms a graft. `addBranch()` consumes whatever was last armed
 * this way and clears it again, so each new branch needs its own fresh
 * graft click - never silently reuses a stale one from several branches
 * ago.
 *
 * Finalizing (building the actual `CurveGraph` via `curveGraphFromBranches()`
 * and storing it as a `NamedResonantProfile`) is `MainWindow`'s own job,
 * not this class's - `branches()` hands over exactly the `BranchGraft`
 * list that function already takes.
 */
class BranchCurveSession {
public:
    /// @brief Whether a session is currently in progress.
    /// @return `true` between a `start()` and its own `end()`.
    [[nodiscard]] bool isActive() const noexcept { return active_; }

    /**
     * @brief Starts a new session with `trunkPath` as its own trunk (the
     *        one branch with no parent) - discards any previous session's
     *        own state first, the same "starting fresh always wins" rule
     *        `PaintController::beginStroke()`'s own docs establish for an
     *        already-in-progress stroke.
     * @param trunkPath The currently-Picked `Path` to use as the trunk.
     * @param trunkOperationId The `Operation::id()` it came from - lets a
     *        later Pick click on this same stroke be recognized as a
     *        graft candidate (see this class's own docs).
     */
    void start(const sound_mind::core::Path& trunkPath, sound_mind::core::OperationId trunkOperationId);

    /// @brief Ends the session, discarding every branch gathered so far -
    ///        a no-op if not currently active.
    void end();

    /**
     * @brief Records `point` as a graft candidate if `operationId` matches
     *        one of this session's own already-added branches - a no-op
     *        (including while not active) otherwise, so an ordinary Pick
     *        click on something unrelated to this session never arms a
     *        graft by accident.
     * @param operationId The just-Picked operation's own id.
     * @param point The click position, in the same raw `TimeFrequencyPoint`
     *        space `BranchGraft::graftPoint` already uses.
     */
    void noteGraftCandidate(sound_mind::core::OperationId operationId, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Whether a graft candidate is currently armed.
    /// @return `true` if `noteGraftCandidate()` has matched a branch since
    ///         the last `addBranch()` (or since `start()`, if none yet).
    [[nodiscard]] bool hasPendingGraft() const noexcept { return pendingParentIndex_.has_value(); }

    /**
     * @brief Appends `path` as a new branch, grafted at whichever graft
     *        candidate is currently armed - clears it afterward either
     *        way, per this class's own docs on why. A no-op (returns
     *        `false`, leaving `branches()` unchanged) if not active or no
     *        graft candidate is currently armed - every branch past the
     *        trunk must be explicitly grafted first.
     * @param path The currently-Picked `Path` to add.
     * @param operationId The `Operation::id()` it came from - recorded the
     *        same way `start()`'s own `trunkOperationId` is, so this
     *        branch can itself be grafted onto later.
     * @return `true` if a branch was actually added.
     */
    bool addBranch(const sound_mind::core::Path& path, sound_mind::core::OperationId operationId);

    /// @brief Every branch gathered so far, trunk first - `MainWindow`'s
    ///        own input to `curveGraphFromBranches()` when finalizing.
    /// @return The current branches; empty if not active.
    [[nodiscard]] const std::vector<sound_mind::core::BranchGraft>& branches() const noexcept { return branches_; }

private:
    bool active_ = false;
    std::vector<sound_mind::core::BranchGraft> branches_;

    /// @brief Parallel to `branches_` - each entry's own source
    ///        `Operation::id()`, so `noteGraftCandidate()` can recognize a
    ///        re-Picked branch without comparing `Path` values.
    std::vector<sound_mind::core::OperationId> branchOperationIds_;

    std::optional<std::size_t> pendingParentIndex_;
    sound_mind::core::TimeFrequencyPoint pendingGraftPoint_;
};

}  // namespace sound_mind::studio
