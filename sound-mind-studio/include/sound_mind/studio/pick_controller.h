#pragma once

#include <memory>
#include <optional>

#include <QObject>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::studio {

class PaintController;

/**
 * @brief Owns the currently Picked paint object - of *any* concrete
 *        `Operation` subtype, not just `PaintOperation` - and turns move/
 *        modify/delete gestures into new, non-destructive operations that
 *        supersede it - see `docs/sound-mind-design.md`'s "Pick".
 *
 * Every edit (move, a reopened Tool Configuration's own change, delete)
 * appends a brand-new operation pointing its own `supersedes()` back at
 * whichever operation was picked, exactly the same non-mutating "editing
 * is a logged action" model `docs/sound-mind-architecture.md`'s Decisions
 * Made already established for `OperationLog` - nothing here ever mutates
 * a past log entry in place. After a successful edit, the *new* operation
 * becomes the current selection, so further edits keep chaining onto it
 * correctly and a freshly-moved/modified object stays pickable without
 * re-clicking it.
 *
 * **Any pickable operation can be selected, moved, and deleted** - a
 * `FillOperation`/`PasteOperation` is exactly as pickable as a
 * `PaintOperation`, via `Operation::translatedCopy()`'s shared move
 * primitive. **Only a `PaintOperation` can be "modified" via a reopened
 * Tool Configuration** (`applyToolConfiguration()`) - `selectedConfiguration()`
 * returns `std::nullopt` for anything else, which the Tool Configuration
 * Panel already treats as "nothing to load", so no separate UI branching
 * is needed for the other kinds. A moved non-`PaintOperation`'s own live
 * drag preview is a plain rectangular outline of its translated `bounds()`
 * rather than a rich Path preview, for the same reason - see
 * `currentPreviewPath()`'s own docs.
 *
 * **A picked `PaintOperation` can also enter direct node/handle editing**
 * (`beginPathEdit()`) - see `docs/sound-mind-design.md`'s "Placing and
 * Editing": "picking a paint object brings its nodes and handles back
 * onto the canvas exactly as if it were still being drawn". While active,
 * `pick()`/`continueMove()`/`endMove()` are transparently repurposed into
 * node-hit-testing/dragging instead of whole-object picking/moving - see
 * their own docs - so `MainWindow`'s existing mouse wiring needs no
 * changes at all to support it. Edits accumulate in `currentPreviewPath()`
 * live, uncommitted, until `commitPathEdit()` (supersedes the original,
 * same as any other edit) or `cancelPathEdit()` (discards them). This
 * installment's own scope: select/move a node (its handles follow),
 * select/drag a `Smooth` node's own handle (the opposite handle always
 * mirrors - no detach-to-corner gesture yet), delete the selected node,
 * and toggle its type. Inserting a node by clicking a segment, and
 * deleting one by double-clicking it, are deferred - see
 * `docs/sound-mind-architecture.md`'s Decisions Made.
 *
 * Shares `PaintController`'s own per-layer pre-paint base cache rather
 * than keeping a second one: every commit here calls back into
 * `PaintController::rebuildLayerContent()` (public since this
 * installment) so a layer touched by both plain painting and Pick edits
 * in the same session always replays from the same cached base.
 *
 * Purely presentational-adjacent, the same shape as `PaintController`:
 * owns Pick *session* state (what's currently selected, an in-progress
 * move's own live preview) and calls straight into `sound_mind::core`
 * for the actual hit-testing/geometry/log logic, but never reaches into
 * `CanvasWidget` directly. Converting raw mouse input into
 * `sound_mind::core::TimeFrequencyPoint`s is the caller's own job, same
 * as `PaintController`.
 */
class PickController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a controller with no project set and nothing
     *        picked.
     * @param paintController The controller whose rebuildLayerContent()
     *        this one calls after every commit - see the class's own
     *        docs on why the pre-paint base cache is shared, not
     *        duplicated. Not owned; must outlive this object.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit PickController(PaintController* paintController, QObject* parent = nullptr);

    /**
     * @brief Sets which project Pick targets.
     *
     * Clears the current selection and any in-progress move - both are
     * meaningless once the project they refer to is gone.
     *
     * @param project The project to pick within; may be `nullptr`
     *        (nothing pickable until a real one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Attempts to select whichever active operation - of any
     *        concrete kind - targeting `layer` is under `point`, and
     *        arms a potential drag from this same point (see
     *        continueMove()/endMove()'s own docs).
     *
     * While a path edit is active (see beginPathEdit()'s own docs), this
     * is repurposed entirely: `layer` is ignored, and `point` instead
     * hit-tests the node/handle currently closest to it on the path being
     * edited (handles take priority over anchors on a near-tie, matching
     * `docs/sound-mind-design.md`'s own handle-first editing emphasis),
     * arming a potential node/handle drag instead of a whole-object one.
     * Selects nothing (deselecting whatever node was selected, if any)
     * when nothing is within tolerance - the edit session itself stays
     * active either way; only commitPathEdit()/cancelPathEdit() ends it.
     *
     * Hit-tested against each candidate operation's own `bounds()`, with
     * at least `kMinimumPickPaddingSeconds`' worth of forgiveness on every
     * side - a real mouse click is far less precise than an automated
     * test's exact pixel, and a `FillOperation`'s/`PasteOperation`'s own
     * `bounds()` already exactly matches its real, visible footprint
     * (unlike a Path's deliberately coarser raw node/handle extent - see
     * `Path::bounds()`'s own docs), leaving no margin of its own to spare
     * - especially a Cut's own silence Fill, which renders nothing at all
     * to click confidently inside of. A `PaintOperation` is additionally
     * padded by its own brush size (`ToolConfiguration::size()`, converted
     * via `frequencyToTimeScaleFor()`) when that's larger, so a single-tap
     * stroke - whose raw Path bounds are a single, zero-area point - is
     * still actually clickable, matching how far its stamp really painted.
     *
     * Ordinarily selects whichever candidate is most recent (an
     * overlapping newer stroke wins over an older one underneath it) -
     * *except* when the currently-selected object is itself one of this
     * click's own candidates, in which case this selects the *next* one
     * underneath it instead (wrapping back to the topmost after the
     * occluded-most one). This is the only way to ever reach an object
     * entirely occluded by a larger one on top of it: a plain click alone
     * would otherwise always re-select the same topmost candidate no
     * matter how many times it's clicked, since nothing about a single
     * click's own geometry distinguishes "select the top one" from
     * "cycle to the one under it".
     *
     * Clears the current selection (emitting selectionChanged()) if
     * nothing is hit, or if no project is set.
     *
     * @param layer Which layer's own paint objects to search.
     * @param point The click position, already converted to time/
     *        frequency space.
     * @return `true` if something was picked; `false` otherwise.
     */
    bool pick(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Clears the current selection and any in-progress move,
    ///        without committing anything - a no-op if nothing is
    ///        selected. Also discards (without committing) any active
    ///        path edit session - see cancelPathEdit()'s own docs, though
    ///        this doesn't itself restore the selection the way
    ///        cancelPathEdit() alone does, since there's no selection
    ///        left afterward either way. Emits selectionChanged() if
    ///        something was selected.
    void clearSelection();

    /// @brief Whether a paint object is currently selected.
    /// @return `true` if something is selected; `false` otherwise.
    [[nodiscard]] bool hasSelection() const noexcept { return pickedOperationId_.has_value(); }

    /// @brief The selected object's own tool configuration - exactly
    ///        what it was painted (or last modified) with, for
    ///        pre-filling a reopened Tool Configuration Panel.
    /// @return The selected configuration, or `std::nullopt` if nothing
    ///         is selected, or the selected object isn't a
    ///         `PaintOperation` (a `FillOperation`/`PasteOperation` has
    ///         no tool configuration of its own to reopen).
    [[nodiscard]] std::optional<sound_mind::core::ToolConfiguration> selectedConfiguration() const;

    /// @brief The selected object's own current bounding box, for
    ///        drawing its selection highlight - see `docs/sound-mind-
    ///        design.md`'s "Pick" (hit-testing reuses the same box).
    /// @return The selected bounds, or `std::nullopt` if nothing is
    ///         selected.
    [[nodiscard]] std::optional<sound_mind::core::TimeFrequencyRect> selectionBounds() const;

    /**
     * @brief Continues an in-progress drag, live-previewing the selected
     *        object translated by how far the cursor has moved since
     *        pick(). A no-op if nothing is selected.
     *
     * While a path edit is active, this instead drags whichever node/
     * handle pick() most recently armed: dragging a node's own anchor
     * moves both its handles along with it (matching
     * `docs/sound-mind-design.md`'s "any node can be moved"); dragging a
     * `Smooth` node's own handle reshapes that side of the curve, with
     * the opposite handle always mirroring through the anchor to keep
     * the tangent smooth (no detach-to-corner gesture yet - see the
     * class's own docs). Directly mutates `currentPreviewPath()`, not
     * yet committed to the log either way.
     *
     * Emits pathChanged() so the caller can redraw the live preview, the
     * same live-feedback contract `PaintController::continueStroke()`
     * already established (reusing `CanvasWidget::setPaintPreviewPath()`
     * - see MainWindow's own wiring).
     *
     * @param point The cursor's current position, in time/frequency
     *        space.
     */
    void continueMove(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Ends an in-progress drag.
     *
     * While a path edit is active, this is a no-op - continueMove()
     * already applied the drag directly to `currentPreviewPath()`, and
     * nothing commits to the log until commitPathEdit() (or discards
     * until cancelPathEdit()).
     *
     * Otherwise: if continueMove() was never called since the selecting
     * pick() (a plain click, not a drag), this is a no-op that leaves the
     * selection exactly as it was - no spurious zero-distance edit is
     * ever logged for a click alone. Otherwise, commits the moved object
     * via its own `Operation::translatedCopy()` - whatever concrete kind
     * it is - superseding the one picked (see `OperationLog::append()`'s
     * own docs: this also preserves its exact position in the stack,
     * rather than promoting it to the top), and makes the new operation
     * the current selection.
     *
     * Emits pathChanged() (clearing the live preview), contentChanged()
     * for the affected layer, and selectionChanged() if a move was
     * actually committed.
     */
    void endMove();

    /**
     * @brief Applies a new tool configuration to the selected object -
     *        the actual work behind reopening Tool Configuration and
     *        changing a control while something is Picked. A no-op if
     *        nothing is selected, or the selected object isn't a
     *        `PaintOperation` (see `selectedConfiguration()`'s own docs -
     *        the Tool Configuration Panel never reopens for anything
     *        else in the first place, so this case isn't expected to be
     *        reached via normal UI interaction, only guarded defensively).
     *
     * Commits immediately (no drag/live-preview phase, unlike move): a
     * new `PaintOperation` with the selected object's own unchanged
     * `Path` geometry but `config`'s parameters, its gradient re-seeded
     * from `config.defaultGradient()` exactly like a fresh stroke
     * (`PaintController::endStroke()`'s own docs), superseding the one
     * picked. The new operation becomes the current selection.
     *
     * Emits contentChanged() for the affected layer.
     *
     * @param config The new tool configuration to apply.
     */
    void applyToolConfiguration(const sound_mind::core::ToolConfiguration& config);

    /**
     * @brief Deletes the selected object - a no-op if nothing is
     *        selected.
     *
     * While a path edit is active *and* a node is currently selected,
     * this instead deletes just that node (see deleteSelectedPathNode()'s
     * own docs) - the same context-sensitive Delete-key behavior
     * `docs/sound-mind-architecture.md`'s Decisions Made records as
     * matching the legacy Studio's own Curve tool.
     *
     * A `PaintOperation` is superseded by a new, empty-Path one - a
     * deliberate "tombstone", not a real stroke: an empty Path samples to
     * nothing (`applyPaintOperation()`'s own docs), so it has zero visual
     * effect once replayed. Any other kind (`FillOperation`,
     * `PasteOperation`) can't reduce to a literal "zero-effect copy of
     * itself" the same way - a `PasteOperation` in particular always
     * overwrites outright, with no opacity to zero out - so those are
     * instead superseded by a fresh, fully-opaque `FillOperation` at the
     * silence floor over the same `bounds()`, the identical "clear this
     * region" mechanism Cut's own source-clearing already uses (see
     * `sound_mind::core::silenceGradient()`'s own docs). Either way,
     * nothing is ever actually removed from the log - "deleted" always
     * means "a new entry supersedes it with zero visible effect".
     *
     * Clears the selection afterward - unlike move/modify, there's
     * nothing left to keep selected.
     *
     * Emits contentChanged() for the affected layer and
     * selectionChanged().
     */
    void deleteSelection();

    /**
     * @brief Moves the selected object to the very top of its own layer's
     *        stack - "Bring to Front". A no-op if nothing is selected, or
     *        it's already topmost.
     *
     * Unlike move/modify/delete, this never appends anything to the
     * `OperationLog` - see `OperationLog::bringToFront()`'s own docs for
     * why reordering isn't a logged, `supersedes()`-based edit. The
     * selection itself (which id is picked) never changes, only where it
     * renders.
     *
     * Emits contentChanged() for the affected layer if the stack actually
     * changed.
     */
    void bringToFront();

    /// @brief Moves the selected object to the very bottom of its own
    ///        layer's stack - "Send to Back". See bringToFront()'s own
    ///        docs for the shared reasoning/preconditions.
    void sendToBack();

    /// @brief Swaps the selected object with whichever active object on
    ///        its own layer sits immediately above it - "Bring Forward".
    ///        See bringToFront()'s own docs for the shared reasoning/
    ///        preconditions.
    void bringForward();

    /// @brief Swaps the selected object with whichever active object on
    ///        its own layer sits immediately below it - "Send Backward".
    ///        See bringToFront()'s own docs for the shared reasoning/
    ///        preconditions.
    void sendBackward();

    /**
     * @brief Enters direct node/handle editing of the selected object's
     *        own Path - see the class's own docs.
     *
     * A no-op (returns `false`) if nothing is selected, or the selection
     * isn't a `PaintOperation` (only a stroke has a Path to edit).
     * Otherwise, copies the selected object's own current Path into
     * `currentPreviewPath()` (visible immediately) with no node selected
     * yet, and switches pick()/continueMove()/endMove() into node-editing
     * mode until commitPathEdit()/cancelPathEdit() ends it.
     *
     * Emits pathChanged().
     *
     * @return `true` if a path edit session actually started; `false`
     *         otherwise.
     */
    bool beginPathEdit();

    /// @brief Whether a path edit session is currently active.
    /// @return `true` between a successful beginPathEdit() and its own
    ///         commitPathEdit()/cancelPathEdit().
    [[nodiscard]] bool isPathEditActive() const noexcept { return pathEditActive_; }

    /// @brief Which node of `currentPreviewPath()` is currently selected,
    ///        while a path edit is active - for the caller to highlight
    ///        it distinctly (and show its own handles, if `Smooth`) on
    ///        the canvas.
    /// @return The selected node's own index, or `std::nullopt` if a path
    ///         edit isn't active, or nothing is currently selected within
    ///         it.
    [[nodiscard]] std::optional<std::size_t> selectedPathNodeIndex() const noexcept { return selectedNodeIndex_; }

    /**
     * @brief Deletes the currently selected node from the path being
     *        edited - a no-op if a path edit isn't active, or no node is
     *        selected, or only one node remains (refusing to edit a path
     *        down to nothing mid-session - deleting the *object* outright
     *        is deleteSelection()'s own job, reached from the same
     *        Delete key when no node is selected - see its own docs).
     *
     * Applies directly to `currentPreviewPath()`, not yet committed to
     * the log - see commitPathEdit()'s own docs.
     *
     * Emits pathChanged().
     */
    void deleteSelectedPathNode();

    /**
     * @brief Converts the currently selected node between `Corner` and
     *        `Smooth` - a no-op if a path edit isn't active, or no node
     *        is selected.
     *
     * Converting to `Smooth` seeds both handles collapsed onto the
     * node's own anchor - the same documented `PathNode` convention
     * `PathController`'s own placement already follows for a freshly
     * created `Smooth` node with no curve pulled out of it yet.
     * Converting to `Corner` discards both handles.
     *
     * Applies directly to `currentPreviewPath()`, not yet committed to
     * the log - see commitPathEdit()'s own docs.
     *
     * Emits pathChanged().
     */
    void toggleSelectedPathNodeType();

    /**
     * @brief Commits the path edit session's own accumulated changes,
     *        superseding the original operation - a no-op if a path edit
     *        isn't active.
     *
     * A new `PaintOperation` with `currentPreviewPath()`'s own edited
     * geometry but the original's own gradient and tool configuration
     * unchanged (editing geometry shouldn't silently reset color or
     * opacity), superseding the one edited - the same non-mutating
     * commit every other Pick edit already uses. The new operation
     * becomes the current selection; the path edit session ends.
     *
     * Emits pathChanged() and contentChanged() for the affected layer.
     */
    void commitPathEdit();

    /// @brief Discards the path edit session's own accumulated changes
    ///        without committing anything - a no-op if a path edit isn't
    ///        active. The original operation is left exactly as it was;
    ///        it stays selected (a plain, non-editing Pick selection).
    ///        Emits pathChanged().
    void cancelPathEdit();

    /**
     * @brief The in-progress move's own live preview - or, while a path
     *        edit is active, the edit session's own current (possibly
     *        already-modified, not yet committed) Path.
     *
     * A translated copy of the selected object's own Path, if it's a
     * `PaintOperation` being moved - the same rich, curve-accurate
     * preview this always showed. For any other kind being moved, a
     * plain four-corner rectangular outline of its translated `bounds()`
     * instead - there's no Path to preview, and a moving bounding box is
     * still real, useful feedback for where a Fill or Paste would land.
     * While a path edit is active, this is the edit's own live buffer
     * instead - see beginPathEdit()'s own docs - reflecting every node/
     * handle change made so far, whether or not a drag is in progress
     * right now.
     *
     * @return The current live preview; empty (no nodes) if neither a
     *         move nor a path edit is in progress.
     */
    [[nodiscard]] const sound_mind::core::Path& currentPreviewPath() const noexcept { return previewPath_; }

signals:
    /// @brief Emitted whenever the current selection changes - a new
    ///        pick, a clear, or a committed edit changing which
    ///        operation id is selected.
    void selectionChanged();

    /// @brief Emitted whenever the in-progress move's own live preview
    ///        Path changes (continueMove()), and once more when it's
    ///        cleared (endMove()).
    void pathChanged();

    /// @brief Emitted whenever a layer's own rendered content changes as
    ///        a result of a committed move/modify/delete.
    /// @param layer Which layer's content changed.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    /// @brief Appends `replacement` (already constructed with its own
    ///        `supersedes()` pointing at the current selection) to the
    ///        log, rebuilds the affected layer via `paintController_`,
    ///        emits contentChanged(), and updates `pickedOperationId_`/
    ///        `pickedOperation_` to the newly-appended entry.
    /// @return The new operation's own id.
    sound_mind::core::OperationId commitReplacement(std::unique_ptr<sound_mind::core::Operation> replacement);

    /// @brief The shared implementation behind bringToFront()/
    ///        sendToBack()/bringForward()/sendBackward(): a no-op if
    ///        nothing is selected; otherwise calls `reorder` on the
    ///        project's own OperationLog with the current selection's id,
    ///        and - only if it actually changed anything - rebuilds the
    ///        affected layer and emits contentChanged().
    /// @param reorder Which of `OperationLog`'s own reorder methods to
    ///        call.
    void reorderSelection(bool (sound_mind::core::OperationLog::*reorder)(sound_mind::core::OperationId));

    /// @brief Which part of a `PathNode` a node/handle hit-test or drag
    ///        refers to - see selectPathNodeNear()'s own docs.
    enum class NodePart { Anchor, HandleIn, HandleOut };

    /// @brief pick()'s own node-editing repurposing while a path edit is
    ///        active - see pick()'s own docs. Hit-tests every node's
    ///        anchor and (for a `Smooth` node) both handles in
    ///        `previewPath_` against `point`, selecting whichever is
    ///        closest within a small, fixed tolerance (handles win a
    ///        near-tie over anchors), and arms a potential drag from
    ///        `point` if something was hit.
    /// @param point The click position, in time/frequency space.
    /// @return `true` if a node/handle was selected; `false` (clearing
    ///         the current node selection) otherwise.
    bool selectPathNodeNear(sound_mind::core::TimeFrequencyPoint point);

    /// @brief continueMove()'s own node-editing repurposing while a path
    ///        edit is active - see continueMove()'s own docs. A no-op if
    ///        nothing is currently selected (selectPathNodeNear() never
    ///        hit anything).
    /// @param point The cursor's current position, in time/frequency
    ///        space.
    void continuePathNodeDrag(sound_mind::core::TimeFrequencyPoint point);

    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;

    std::optional<sound_mind::core::OperationId> pickedOperationId_;
    sound_mind::core::LayerId pickedLayer_ = 0;

    /// @brief The currently selected operation, or `nullptr` if nothing
    ///        is. A raw, non-owning pointer straight into the project's
    ///        own `OperationLog` - safe to hold across further append()
    ///        calls, since `OperationLog` only ever grows via
    ///        `std::vector<std::unique_ptr<Operation>>::push_back()`,
    ///        which never moves or destroys an *already-held* Operation,
    ///        only the vector's own unique_ptr slots (a real vector
    ///        reallocation moves the smart pointers, not the heap objects
    ///        they own). Never dereferenced unless `pickedOperationId_`
    ///        also has a value.
    const sound_mind::core::Operation* pickedOperation_ = nullptr;

    sound_mind::core::TimeFrequencyPoint dragAnchor_;
    bool dragMoved_ = false;
    sound_mind::core::TimeFrequencyPoint dragCurrent_;
    sound_mind::core::Path previewPath_;

    /// @brief Whether a path edit session (beginPathEdit()) is currently
    ///        active - see the class's own docs.
    bool pathEditActive_ = false;

    /// @brief Which node of `previewPath_` is currently selected, while
    ///        a path edit is active - `std::nullopt` if none is.
    std::optional<std::size_t> selectedNodeIndex_;

    /// @brief Which part of `selectedNodeIndex_`'s own node
    ///        selectPathNodeNear() most recently armed for dragging.
    NodePart selectedNodePart_ = NodePart::Anchor;

    /// @brief A snapshot of `previewPath_` taken at the start of the
    ///        current node/handle drag, so continuePathNodeDrag() always
    ///        applies the *total* delta from `dragAnchor_` to the
    ///        original position, rather than compounding small per-call
    ///        deltas (the same "always derive from the drag's own start,
    ///        never from the last frame" precedent endMove()'s own
    ///        whole-object drag already follows via `pickedOperation_`
    ///        staying untouched until the drag actually ends).
    sound_mind::core::Path pathEditDragStart_;
};

}  // namespace sound_mind::studio
