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
 *
 * A third, independent mechanism - **stack order** - decides *rendering*
 * order (which of two overlapping active operations on the same layer
 * paints on top), deliberately kept separate from both of the above:
 *
 * - `operations_`'s own append order is a strict, append-only history
 *   record - it exists so undo()/redo()'s high-water mark and
 *   `supersedes()` chains stay simple and correct, and it must never be
 *   reinterpreted as anything else.
 * - `stackOrder_` is a separate, independently-ordered list of every
 *   operation id ever appended, used *only* to decide the order
 *   `activeOperationsTargeting()` returns its own (already-filtered)
 *   results in. A fresh, non-superseding append places its own id at the
 *   end of `stackOrder_` (newest paints on top, same as before). A
 *   *superseding* append - Pick's own move/modify/delete - inserts its
 *   own id immediately next to the operation it supersedes' own entry,
 *   rather than at the end: the whole reason this exists is so editing an
 *   object preserves its position in the stack instead of always
 *   promoting it to the top (a real, reported bug in the very first
 *   version of this mechanism, which just used append order for both
 *   purposes at once - see `docs/sound-mind-architecture.md`'s Decisions
 *   Made). Entries for ids that are no longer active (superseded, or
 *   undone) are simply skipped when read, never pruned - undo()/redo()
 *   needs no awareness of `stackOrder_` at all as a result. A user can
 *   also deliberately rearrange `stackOrder_` directly -
 *   bringToFront()/sendToBack()/bringForward()/sendBackward() - without
 *   appending anything: reordering isn't an edit to *what* an operation
 *   does, only to where it renders relative to others on the same layer,
 *   so it doesn't need (and doesn't get) a logged, `supersedes()`-based
 *   history entry of its own.
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
     * Also places the new operation's own id into `stackOrder_` (see the
     * class's own docs): at the very end if `operation->supersedes()` is
     * `std::nullopt` (a genuinely new object, painted on top of
     * everything else so far); immediately next to its own superseded
     * id's entry otherwise, so editing an object never changes where it
     * sits in the stack.
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
     * @brief The operations relevant to rebuilding one layer's cache, in
     *        stack order (back to front) - the exact sequence a real
     *        replay needs to consume, and the same order Pick's own
     *        hit-testing reads (most-recent-in-the-stack-first, via
     *        reverse iteration) to decide which overlapping object a
     *        click actually reaches first.
     *
     * Only operations that are both currently active (not undone) *and*
     * not superseded by a later active operation are included - matching
     * "Composer Mode Fit"'s own replay rule: "Replay honours only the
     * non-superseded operation at each point in a layer's history." The
     * *order* they're returned in is `stackOrder_`'s own (see the class's
     * own docs) - deliberately not raw append order, which would put a
     * just-edited object back at the very end (the top) regardless of
     * where it actually sits in the stack.
     *
     * @param layer Which layer to filter to, via each operation's own
     *        Operation::targetLayer().
     * @return Every matching operation, back of the stack first.
     */
    [[nodiscard]] std::vector<const Operation*> activeOperationsTargeting(LayerId layer) const;

    /**
     * @brief Moves `id` to the very top (front) of its own layer's stack -
     *        "Bring to Front".
     *
     * Only ever reorders *among* `id`'s own layer's currently active
     * operations - other layers' own entries, and any of this layer's own
     * entries that aren't currently active, are entirely undisturbed.
     * Every other reorder method here follows the same scoping.
     *
     * @param id The operation to move; must currently be active (see
     *        activeOperationsTargeting()) and target a real layer.
     * @return `true` and reorders if `id` is active and not already
     *         topmost; `false` (no change) otherwise.
     */
    bool bringToFront(OperationId id);

    /// @brief Moves `id` to the very bottom (back) of its own layer's
    ///        stack - "Send to Back". See bringToFront()'s own docs for
    ///        the shared scoping/preconditions.
    /// @param id The operation to move.
    /// @return `true` and reorders if `id` is active and not already at
    ///         the back; `false` (no change) otherwise.
    bool sendToBack(OperationId id);

    /// @brief Swaps `id` with whichever active operation on its own layer
    ///        sits immediately above it - "Bring Forward". See
    ///        bringToFront()'s own docs for the shared scoping/
    ///        preconditions.
    /// @param id The operation to move.
    /// @return `true` and reorders if `id` is active and not already
    ///         topmost; `false` (no change) otherwise.
    bool bringForward(OperationId id);

    /// @brief Swaps `id` with whichever active operation on its own layer
    ///        sits immediately below it - "Send Backward". See
    ///        bringToFront()'s own docs for the shared scoping/
    ///        preconditions.
    /// @param id The operation to move.
    /// @return `true` and reorders if `id` is active and not already at
    ///         the back; `false` (no change) otherwise.
    bool sendBackward(OperationId id);

    friend void to_json(nlohmann::json& json, const OperationLog& log);
    friend void from_json(const nlohmann::json& json, OperationLog& log);

private:
    /// @brief Which direction reorderActiveOperation() should move an
    ///        operation in - the shared implementation behind
    ///        bringToFront()/sendToBack()/bringForward()/sendBackward().
    enum class ReorderDirection { ToFront, ToBack, Forward, Backward };

    /// @brief The shared implementation behind bringToFront()/
    ///        sendToBack()/bringForward()/sendBackward() - see their own
    ///        docs for the exact per-direction behavior.
    bool reorderActiveOperation(OperationId id, ReorderDirection direction);

    std::vector<std::unique_ptr<Operation>> operations_;
    std::size_t activeCount_ = 0;
    OperationId nextId_ = 1;

    /// @brief The current rendering (stack) order of every operation ever
    ///        appended - see the class's own docs for why this is kept
    ///        entirely separate from `operations_`'s own append order.
    std::vector<OperationId> stackOrder_;
};

/// @brief Serializes the log to its JSON representation.
void to_json(nlohmann::json& json, const OperationLog& log);

/// @brief Parses the log from its JSON representation.
/// @throws nlohmann::json::exception if the JSON value isn't an array, or
///         an entry's own "kind" isn't a recognized Operation subtype.
void from_json(const nlohmann::json& json, OperationLog& log);

}  // namespace sound_mind::core
