#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QObject>
#include <QString>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::studio {

class ProjectHistory;

/**
 * @brief Owns an in-progress freehand stroke and turns it into a real,
 *        undoable `PaintOperation` - see `docs/sound-mind-design.md`'s
 *        "Freehand Path Capture"/"Tool Configuration"/"Pick".
 *
 * Purely presentational-adjacent, the same shape as `PlaybackController`:
 * it owns painting *session* state (the stroke currently being drawn,
 * each layer's replay cache) and calls straight
 * into `sound_mind::core` for the actual curve-fitting/DSP/undo-redo
 * logic, but never reaches into `CanvasWidget` directly - `pathChanged()`/
 * `contentChanged()` are what `MainWindow` connects to whatever needs to
 * reflect them. Converting raw mouse/tablet input into
 * `sound_mind::core::TimeFrequencyPoint`s is the caller's own job (a
 * pixel-to-domain conversion that depends on `CanvasWidget`'s own layout,
 * not painting-session logic) - this class only ever receives points
 * already in that space.
 */
class PaintController : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs a controller with no project set and no stroke
    ///        in progress.
    /// @param history Non-owning; notifyOperationCommitted() commits to it,
    ///        and its undo/redo re-renders the layers they change. Nullable:
    ///        without one, the controller keeps a private history of its
    ///        own, so undo()/redo() still work on their own (every test in
    ///        `test_paint_controller.cpp` relies on that). Must outlive this
    ///        controller if given.
    /// @param parent The owning object, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit PaintController(ProjectHistory* history = nullptr, QObject* parent = nullptr);

    /**
     * @brief Sets which project painting targets.
     *
     * Clears any in-progress stroke and every layer's replay cache - both
     * are meaningless once the project they refer to is gone.
     *
     * @param project The project to paint into; may be `nullptr` (nothing
     *        paintable until a real one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Invalidates every layer's own cached paint-content replay -
     *        `v0.1.8.1` (`docs/sound-mind-roadmap.md`'s within-a-layer
     *        operation-replay cache). Call whenever any MindWave's own
     *        value changes anywhere in the project's library (an
     *        `InstrumentConfiguration`'s vibrato/tremolo binding, or a
     *        `FilterOperation`'s own bound parameter, might be using it)
     *        - see `sound_mind::core::PaintContentCache`'s own docs for
     *        why this is deliberately coarse (every layer, not just ones
     *        actually bound to the edited MindWave).
     *
     * Does *not* force an immediate rebuild of anything - only the *next*
     * `rebuildLayerContent()` call for each layer, whenever that happens
     * for any reason, replays fully rather than reusing a stale cached
     * result. Each layer's base content is untouched - nothing about it
     * changes just because a MindWave's value did.
     */
    void invalidateContentCaches();

    /// @brief The tool configuration new strokes are painted with.
    /// @return The configuration currently in effect.
    [[nodiscard]] const sound_mind::core::ToolConfiguration& toolConfiguration() const noexcept {
        return *toolConfig_;
    }

    /// @brief Sets the tool configuration new strokes are painted with -
    ///        does not affect a stroke already in progress. Takes
    ///        ownership - `ToolConfiguration` is abstract, so it can only
    ///        ever be passed around via `unique_ptr`.
    /// @param config The new configuration; must not be `nullptr`.
    void setToolConfiguration(std::unique_ptr<sound_mind::core::ToolConfiguration> config) {
        toolConfig_ = std::move(config);
    }

    /**
     * @brief Starts a new freehand stroke.
     *
     * Does nothing if a project isn't set, or a stroke is already in
     * progress (call endStroke()/cancelStroke() first). Also refuses
     * silently whenever `targetLayer` is a Filter or Equalizer layer (see
     * `sound_mind::core::isFilterLayerType()`) - neither is meant to hold
     * its own paintable amplitude content, only to composite-and-transform
     * the layers beneath it (`docs/sound-mind-design.md`'s "Filter Layer").
     * Also refuses silently - the stroke simply never starts - if the
     * current tool configuration is a `MindGrainConfiguration` and
     * `targetLayer` isn't above its own source layer (see
     * `sound_mind::core::isLayerAbove()`'s own docs): this is the last-line
     * backstop of `docs/sound-mind-design.md`'s "Mind Grains" ordering
     * rule - callers are expected to already prevent reaching this call in
     * the first place (Tool Configuration's own red highlight, the Layers
     * Panel's red X, and the Paint button's own disabled state all exist so
     * a user can't even attempt this), but this guard is what actually
     * makes an invalid Mind Grain stroke impossible rather than merely
     * discouraged.
     *
     * @param targetLayer Which layer this stroke will paint into.
     * @param point The stroke's own first point, already converted to
     *        time/frequency space.
     */
    void beginStroke(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Adds another raw sample to the in-progress stroke and
     *        re-fits its live preview Path - does nothing if no stroke is
     *        in progress.
     *
     * Emits pathChanged() so the caller can redraw the live preview -
     * cheap enough to call on every real mouse-move event, per the design
     * doc's own responsiveness requirement (see path.h's
     * fitPathToPoints()'s own docs).
     *
     * @param point The next raw sample, in time/frequency space.
     */
    void continueStroke(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Finishes the in-progress stroke: builds its final Path
     *        (seeded with the current tool configuration's own default
     *        gradient - see `ToolConfiguration::defaultGradient()`'s own
     *        docs), appends a new `PaintOperation` to the project's
     *        `OperationLog`, and rebuilds the target layer's own painted
     *        content.
     *
     * A stroke with only a single point (a tap, not a drag - see the
     * design doc's own "clicking and dragging... or just tapping the
     * mouse") still paints, as a single stamp. Does nothing if no stroke
     * is in progress.
     *
     * Emits contentChanged() for the target layer, and pathChanged() to
     * clear the now-finished live preview.
     */
    void endStroke();

    /// @brief Abandons the in-progress stroke without painting anything -
    ///        does nothing if no stroke is in progress. Emits
    ///        pathChanged() to clear the live preview.
    void cancelStroke();

    /// @brief Whether a stroke is currently being drawn.
    /// @return `true` between a beginStroke() and its own endStroke()/cancelStroke().
    [[nodiscard]] bool isStrokeInProgress() const noexcept { return strokeInProgress_; }

    /// @brief The in-progress stroke's own live preview Path - see
    ///        `docs/sound-mind-design.md`'s "Freehand Path Capture".
    /// @return The current live preview; empty (no nodes) if no stroke is
    ///         in progress, or fewer than two points have been sampled
    ///         yet.
    [[nodiscard]] const sound_mind::core::Path& currentPreviewPath() const noexcept { return previewPath_; }

    /// @brief Whether undo() would currently do anything.
    /// @return `true` if a project is set and its history has something
    ///         to undo.
    [[nodiscard]] bool canUndo() const noexcept;

    /// @brief Whether redo() would currently do anything.
    /// @return `true` if a project is set and its history has an undone
    ///         entry to redo.
    [[nodiscard]] bool canRedo() const noexcept;

    /**
     * @brief Undoes the history's latest entry (`v0.1.43.1`: whatever it
     *        was, not only content operations); every layer whose content
     *        it changes is re-rendered, emitting contentChanged() for each.
     *        A no-op if canUndo() is `false`.
     */
    void undo();

    /// @brief Redoes the history's next undone entry - see undo().
    void redo();

    /**
     * @brief Rebuilds one layer's own painted content from scratch and
     *        emits contentChanged() for it.
     *
     * Replays the layer's active operations on top of its base content
     * (`Layer::baseContent()`, `v0.1.43.1`: what an import, recording,
     * generator or phase cleanup made it, saved with the project)
     * - or, if it has none (the Background layer, which
     * `Project::createNew()` leaves content-less), on a silent,
     * project-sized base (`sound_mind::core::silentContentFor()`), so every
     * layer is a paintable canvas. Never on the result of a previous
     * replay, so undo/redo stay correct however often painting revisits
     * the layer, and a reopened project keeps painting on the right base.
     *
     * Public since `v0.Y.24.1`'s Pick installment: `PickController`
     * appends its own new (move/modify/delete) operations directly to the
     * project's `OperationLog`, then calls back into this same method -
     * through this same controller, so the replay uses the same base and
     * cache a plain paint stroke would - rather than
     * duplicating the rebuild logic in a second place.
     *
     * Also supplies `rebuildPaintedContent()`'s own
     * `sound_mind::core::LayerContentResolver`, reading straight from the
     * live `Project` - the mechanism that gives a Mind Grain stroke its own
     * liveness (see `docs/sound-mind-design.md`'s "Mind Grains"): every call
     * to this method re-resolves whatever a Mind Grain stroke's own source
     * layer currently holds, rather than replaying a stale copy from
     * whenever the stroke was first drawn.
     *
     * **Cascades immediately to every dependent Mind Grain layer.** After
     * rebuilding `layer` itself, this also calls itself (internally) for
     * every other layer with an active Mind Grain stroke sourced from
     * `layer` (`sound_mind::core::layersWithMindGrainOperationsSourcedFrom()`),
     * and so on transitively - so repainting a layer that something else
     * reads from live is reflected everywhere that reads it *immediately*,
     * not only the next time each dependent layer happens to rebuild for
     * some unrelated reason of its own. Every dependent layer is rebuilt
     * (and its own `contentChanged()` emitted) at most once per outer call,
     * even if it's reachable through more than one dependency chain (a
     * diamond: two different layers both feeding a third) - tracked via an
     * internal visited set for the duration of one `rebuildLayerContent()`
     * call. This cascade can only ever walk *upward* through the layer
     * stack (a Mind Grain's own source is always below wherever it's
     * painted - see `sound_mind::core::isLayerAbove()`'s own docs), so it's
     * bounded by the stack's own height and can never cycle back down to a
     * layer already visited on its way up.
     *
     * **As of `v0.1.8.1`, reuses a per-layer `sound_mind::core::
     * PaintContentCache` wherever it's still valid** (see that class's
     * own docs) - replaying only newly-appended operations on top of a
     * cached intermediate result, rather than this layer's *entire*
     * history from `base`, whenever possible. A Mind Grain cascade
     * target's own cache is explicitly invalidated right before it's
     * rebuilt here (see this method's own cascade paragraph below) -
     * `PaintContentCache`'s own docs cover why that's needed and what it
     * narrows relative to `MindGrainConfiguration`'s pre-`v0.1.8.1` docs.
     *
     * @param layer Which layer to rebuild.
     */
    void rebuildLayerContent(sound_mind::core::LayerId layer);

    /**
     * @brief Commits a just-made content change (an appended operation,
     *        usually) to the project's History with `description`
     *        (`v0.1.43.1`), so Edit > Undo/Redo, the History panel and
     *        Macro recording all see it.
     *
     * Public: `endStroke()` calls this after its own append, and so do
     * `PathController::finishPath()`, `PickController::commitReplacement()`,
     * `SelectionController::fill()`/`cutSelection()`/`pasteInto()` and the
     * other controllers that append content operations.
     *
     * @param description A short, human-readable label (e.g. "Painted
     *        stroke") - the History panel's row text for it.
     */
    void notifyOperationCommitted(const QString& description);

    /**
     * @brief notifyOperationCommitted() for `count` log entries appended
     *        together - one History entry, so one undo/redo rolls back or
     *        replays all of them.
     * @param count How many entries were just appended; `0` records nothing.
     * @param description The History Panel's row text for the batch.
     */
    void notifyOperationsCommitted(std::size_t count, const QString& description);

    /**
     * @brief Paints the current tool configuration along each of `paths`
     *        onto `layer`, as one undoable step - "Stroke Path with Current
     *        Tool" (Pre-Beta Plan `v0.Y.77.1`). Each path becomes its own
     *        `PaintOperation`, its gradient seeded from the tool's default
     *        exactly like a freshly drawn stroke (endStroke()).
     *
     * Refuses exactly where beginStroke() does: a Filter/Equalizer layer,
     * or a Mind Grain tool on or below its own source layer.
     *
     * @param layer The layer to paint onto.
     * @param paths The paths to stroke; empty ones are skipped.
     * @param description The History Panel's row text.
     * @return How many strokes were painted (`0` if refused).
     * @note GUI thread only.
     */
    std::size_t strokePaths(sound_mind::core::LayerId layer, const std::vector<sound_mind::core::Path>& paths,
                            const QString& description);

    /**
     * @brief Deletes every listed operation that is still active, as one
     *        undoable step.
     *
     * Each deletion appends a tombstone that supersedes the operation (see
     * makeDeletionTombstone()), exactly as deleting a single Picked object
     * does. One History entry covers the whole batch, so a single undo
     * restores all of them. Ids that are unknown or no longer active are
     * skipped; if none are left, nothing is appended or recorded.
     *
     * @param ids The operations to delete.
     * @param description The History Panel's row text for the batch.
     * @note GUI thread only.
     */
    void deleteOperations(const std::vector<sound_mind::core::OperationId>& ids, const QString& description);

    /**
     * @brief Drops everything cached for `layer` (its replay cache). Call once a layer has been removed from the
     *        project, so the memory is released and nothing stale lingers.
     * @param layer The removed layer's id.
     */
    void forgetLayer(sound_mind::core::LayerId layer);

    /**
     * @brief Builds the zero-effect replacement that "deletes" `operation`
     *        by superseding it.
     *
     * A PaintOperation becomes a copy with an empty path. Any other kind
     * becomes a silence FillOperation over the same bounds (a Fill/Paste
     * can't reduce to a literal zero-effect copy of itself).
     *
     * @param operation The operation to delete; must target a layer.
     * @param newId A fresh id from OperationLog::reserveId().
     * @return The tombstone, ready to append.
     */
    [[nodiscard]] static std::unique_ptr<sound_mind::core::Operation> makeDeletionTombstone(
        const sound_mind::core::Operation& operation, sound_mind::core::OperationId newId);

signals:
    /// @brief Emitted whenever the in-progress stroke's own live preview
    ///        Path changes (continueStroke()), and once more when it's
    ///        cleared (endStroke()/cancelStroke()).
    void pathChanged();

    /// @brief Emitted whenever a layer's own rendered content changes as
    ///        a result of painting, undo(), or redo().
    /// @param layer Which layer's content changed.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    /// @brief Whether a stroke may be painted onto `layer` with the current
    ///        tool - the shared refusal rules of beginStroke()/strokePaths().
    [[nodiscard]] bool canPaintOn(sound_mind::core::LayerId layer) const;

    /// @brief The actual rebuild + cascade recursion behind
    ///        rebuildLayerContent() - see its own docs. `visited` is shared
    ///        across one whole outer call's own recursion, so a layer
    ///        reachable through more than one dependency chain (a diamond)
    ///        is still only ever rebuilt once.
    /// @param layer Which layer to rebuild.
    /// @param visited Every layer already rebuilt so far this outer call -
    ///        mutated in place as the recursion proceeds.
    void rebuildLayerContentAndCascade(sound_mind::core::LayerId layer,
                                        std::unordered_set<sound_mind::core::LayerId>& visited);

    ProjectHistory* history_;
    bool ownsHistory_ = false;
    sound_mind::core::Project* project_ = nullptr;
    std::unique_ptr<sound_mind::core::ToolConfiguration> toolConfig_ =
        std::make_unique<sound_mind::core::ProceduralConfiguration>();

    bool strokeInProgress_ = false;
    sound_mind::core::LayerId strokeTargetLayer_ = 0;
    std::vector<sound_mind::core::TimeFrequencyPoint> strokePoints_;
    sound_mind::core::Path previewPath_;

    /// @brief The base revision each layer's replay cache was built on -
    ///        a new base invalidates it (`v0.1.43.1`).
    std::unordered_map<sound_mind::core::LayerId, std::uint64_t> cachedBaseRevision_;
    /// @brief `v0.1.8.1` (`docs/sound-mind-roadmap.md`'s within-a-layer
    ///        operation-replay cache) - see `rebuildLayerContent()`'s/
    ///        `invalidateContentCaches()`'s own docs.
    std::unordered_map<sound_mind::core::LayerId, sound_mind::core::PaintContentCache> contentCache_;
};

}  // namespace sound_mind::studio
