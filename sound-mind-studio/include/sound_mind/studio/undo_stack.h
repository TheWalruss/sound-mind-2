#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <QString>

namespace sound_mind::studio {

/**
 * @brief One undoable action: a matched pair of callbacks that reverse and
 *        reapply some change that has *already happened* by the time it's
 *        pushed onto an `UndoStack`, plus a short human-readable label.
 *
 * Originally just the two `std::function<void()>` callbacks; `description`
 * was added for the History Panel (`v0.Y.46.1` Installment D, "Layers
 * Panel & Editing Enhancements v2") - a read-only list of every entry's
 * own `description`, in order, with a way to jump directly to any point
 * among them (see `UndoStack::jumpTo()`). Defaults to an empty string so
 * existing two-field brace-initialization at any call site this
 * installment didn't touch still compiles unchanged.
 */
struct UndoCommand {
    /// @brief Reverts this command's own change.
    std::function<void()> undo;
    /// @brief Reapplies this command's own change.
    std::function<void()> redo;
    /// @brief A short, human-readable label for this command - shown as
    ///        one row of the History Panel's own list. Empty (the
    ///        default) for a command pushed before this field existed to
    ///        care about a label at all.
    QString description;
};

/**
 * @brief A single, linear, in-session undo/redo history spanning every
 *        kind of edit this Studio makes undoable - layer property
 *        mutations (opacity, opacityMindWave, visibility, translation,
 *        rescale - see `LayerController`) and content operations (paint
 *        strokes, Pick's move/modify/delete, Fill, Paste - see
 *        `PaintController::notifyOperationCommitted()`, called from
 *        `PathController`/`PickController`/`SelectionController` too) -
 *        so `MainWindow`'s own single Edit > Undo/Redo pair covers both
 *        correctly, in whatever order they actually happened, rather
 *        than needing two separate mechanisms a user would have to think
 *        about separately.
 *
 * **Deliberately independent from `sound_mind::core::OperationLog`** -
 * see `docs/sound-mind-architecture.md`'s own Decision on this class for
 * why a layer property change isn't logged as a `sound_mind::core::
 * Operation`: `OperationLog::activeOperationsTargeting()` treats *every*
 * `Operation` targeting a layer as pickable canvas content
 * (`PickController`'s own hit-testing docs: "any concrete Operation kind
 * is a candidate, not just PaintOperation") - a property mutation has no
 * real canvas geometry and must never become a pickable object. Content
 * commits still logged to `OperationLog` get *mirrored* onto this stack
 * instead, as a pair of callbacks delegating back into `PaintController::
 * undo()`/`redo()` (which already drives that log's own high-water mark
 * correctly) - one shared, purely additive history, without changing
 * `OperationLog`'s own long-standing pickable-content contract at all.
 *
 * A push() does *not* invoke its own command - unlike some undo-stack
 * designs, the change it describes has always already happened by the
 * time it's pushed (a setter already applied the new value; an Operation
 * is already appended) - this only ever *records* it. Only undo()/redo()
 * ever invoke a stored callback. Pushing past a prior undo() discards
 * whatever redo() tail existed, the same "a fresh edit abandons the old
 * future" rule `OperationLog::append()` already documents for its own,
 * independent history.
 *
 * Session-only, like `PaintController`'s own per-layer paint base cache -
 * not persisted, and explicitly clear()ed whenever the project it refers
 * to is replaced (see `MainWindow::setProject()`), since a previous
 * project's own layer ids/operations mean nothing once it's gone.
 */
class UndoStack {
public:
    /// @brief Records a just-performed action. Discards any redo tail
    ///        first (see the class's own docs).
    /// @param command The already-performed action's own undo/redo pair.
    void push(UndoCommand command);

    /// @brief Whether undo() would currently do anything.
    /// @return `true` if at least one command is currently done.
    [[nodiscard]] bool canUndo() const noexcept { return index_ > 0; }

    /// @brief Whether redo() would currently do anything.
    /// @return `true` if a previously undone command is available to redo.
    [[nodiscard]] bool canRedo() const noexcept { return index_ < commands_.size(); }

    /// @brief Reverts the most recently performed, not-yet-undone command
    ///        by invoking its own `undo` callback - a no-op if canUndo()
    ///        is `false`.
    void undo();

    /// @brief Reapplies the most recently undone command by invoking its
    ///        own `redo` callback - a no-op if canRedo() is `false`.
    void redo();

    /// @brief Discards every recorded command - see the class's own docs
    ///        on why this must happen whenever the project changes.
    void clear();

    /// @brief How many commands are currently recorded, active or not -
    ///        the History Panel's own row count is this plus one (a
    ///        leading "(Start)" row for index `0`).
    /// @return The total number of recorded commands.
    [[nodiscard]] std::size_t count() const noexcept { return commands_.size(); }

    /// @brief This stack's own current high-water mark - see `index_`'s
    ///        own docs. The History Panel highlights the row at this
    ///        index as "where we are now".
    /// @return A value in `[0, count()]`.
    [[nodiscard]] std::size_t currentIndex() const noexcept { return index_; }

    /// @brief The human-readable label of the command at `index` - the
    ///        History Panel's own row text for row `index + 1` (row `0`
    ///        is always "(Start)", not backed by any real command here).
    /// @param index Which command to read; must be `< count()`.
    /// @return That command's own `description`.
    [[nodiscard]] const QString& descriptionAt(std::size_t index) const { return commands_.at(index).description; }

    /**
     * @brief Moves directly to an arbitrary point in this stack's own
     *        linear history - the History Panel's own "jump to here"
     *        action, `v0.Y.46.1` Installment D.
     *
     * Repeatedly calls undo()/redo() (never any other mechanism) until
     * currentIndex() equals `index` - so every intermediate command's own
     * callback still actually runs, in order, exactly as if the user had
     * clicked Undo/Redo that many times themselves. No branching or
     * versioning - "jumping" is just many single steps taken at once.
     *
     * @param index The target index, intended to be in `[0, count()]` -
     *        clamped implicitly by undo()/redo()'s own no-op-at-the-edge
     *        behavior if out of range.
     */
    void jumpTo(std::size_t index);

private:
    std::vector<UndoCommand> commands_;

    /// @brief How many of `commands_`, from the front, are currently
    ///        "done" - this stack's own high-water mark, the same shape
    ///        as `sound_mind::core::OperationLog`'s own `activeCount_`.
    std::size_t index_ = 0;
};

}  // namespace sound_mind::studio
