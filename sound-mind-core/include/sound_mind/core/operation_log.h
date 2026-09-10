#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"

namespace sound_mind::core {

/**
 * @brief A Project's single, project-wide, ordered sequence of operations.
 *
 * See `docs/sound-mind-architecture.md`'s Core Data Model and "Composer
 * Mode Fit": this belongs to the Project, not to any one Layer, since a
 * structural operation (reordering the layer stack) doesn't target a
 * single layer's content.
 *
 * Owns every logged `Operation` (non-copyable, matching `Operation`'s own
 * stance - `unique_ptr` is how it's actually held). Two independent
 * mechanisms coexist here, deliberately kept separate (see
 * `docs/sound-mind-architecture.md`'s Decision #34's undo/redo note):
 *
 * - **undo()/redo()** move a "how much of the log is currently active"
 *   high-water mark back and forth. Simple and linear, not a branching
 *   history: appending a new operation after an undo() *does* discard
 *   whatever redo() tail existed (unlike supersedes() below, which never
 *   removes anything) - the same "a fresh edit abandons the old future"
 *   rule any conventional undo stack follows. This doesn't compromise the
 *   append-only replay guarantee described on Operation::supersedes() -
 *   that guarantee is about never changing what a given *active* prefix
 *   of the log replays to, which still holds; a fresh append past an
 *   undo() is a deliberate choice to diverge from an abandoned future,
 *   not a mutation of one that's still reachable.
 * - **supersedes()** (declared on `Operation` itself) is for a
 *   *deliberate* re-edit of one specific past operation (Composer Mode
 *   retiming it, or Pick modifying a picked object) - appending a new
 *   operation whose `supersedes()` points at the old one, which becomes
 *   permanently inactive regardless of the undo high-water mark. It isn't
 *   how plain undo works; plain undo doesn't need a replacement.
 */
class OperationLog {
public:
    /// @brief Reserves a fresh, log-unique id for an operation the caller
    ///        is about to construct - `Operation`'s own concrete subtypes
    ///        (`PaintOperation` and friends) all need one at construction
    ///        time, before they exist to be append()'d.
    /// @return An id no operation in this log has used before.
    [[nodiscard]] OperationId reserveId() noexcept { return nextId_++; }

    /**
     * @brief Appends a newly constructed operation as the new most-recent,
     *        active entry.
     *
     * If undo() had moved the active high-water mark backward, this
     * discards every operation past it first - the same "a fresh edit
     * invalidates the redo tail" rule any conventional undo stack follows.
     *
     * @param operation The operation to append; must not be `nullptr`.
     */
    void append(std::unique_ptr<Operation> operation);

    /// @brief How many operations are currently logged, active or not -
    ///        i.e. every operation ever appended, including ones undo()
    ///        has hidden.
    /// @return The total number of logged operations.
    [[nodiscard]] std::size_t size() const noexcept { return operations_.size(); }

    /**
     * @brief Raw, index-based access to a logged operation, regardless of
     *        whether it's currently active.
     * @param index Which operation to return, in append order.
     * @return The operation at that index.
     */
    [[nodiscard]] const Operation& at(std::size_t index) const { return *operations_.at(index); }

    /// @brief Whether undo() would currently do anything.
    /// @return `true` if at least one active operation exists to undo.
    [[nodiscard]] bool canUndo() const noexcept { return activeCount_ > 0; }

    /// @brief Whether redo() would currently do anything.
    /// @return `true` if the high-water mark sits below the log's own
    ///         total size, i.e. something undone is still available.
    [[nodiscard]] bool canRedo() const noexcept { return activeCount_ < operations_.size(); }

    /// @brief Moves the active high-water mark back by one, hiding the
    ///        most recently active operation - a no-op if canUndo() is `false`.
    void undo() noexcept;

    /// @brief Moves the active high-water mark forward by one, restoring
    ///        the next operation - a no-op if canRedo() is `false`.
    void redo() noexcept;

    /**
     * @brief The operations relevant to rebuilding one layer's cache,
     *        in log order - the exact sequence a real replay needs to
     *        consume.
     *
     * Only operations that are both currently active (not undone) *and*
     * not superseded by a later active operation are included - matching
     * "Composer Mode Fit"'s own replay rule: "Replay honours only the
     * non-superseded operation at each point in a layer's history."
     *
     * @param layer Which layer to filter to, via each operation's own
     *        Operation::targetLayer().
     * @return Every matching operation, oldest first.
     */
    [[nodiscard]] std::vector<const Operation*> activeOperationsTargeting(LayerId layer) const;

    friend void to_json(nlohmann::json& json, const OperationLog& log);
    friend void from_json(const nlohmann::json& json, OperationLog& log);

private:
    std::vector<std::unique_ptr<Operation>> operations_;
    std::size_t activeCount_ = 0;
    OperationId nextId_ = 1;
};

/// @brief Serializes the log to its JSON representation.
void to_json(nlohmann::json& json, const OperationLog& log);

/// @brief Parses the log from its JSON representation.
/// @throws nlohmann::json::exception if the JSON value isn't an array, or
///         an entry's own "kind" isn't a recognized Operation subtype.
void from_json(const nlohmann::json& json, OperationLog& log);

}  // namespace sound_mind::core
