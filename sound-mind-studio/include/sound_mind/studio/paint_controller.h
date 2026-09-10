#pragma once

#include <unordered_map>
#include <vector>

#include <QObject>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::studio {

/**
 * @brief Owns an in-progress freehand stroke and turns it into a real,
 *        undoable `PaintOperation` - see `docs/sound-mind-design.md`'s
 *        "Freehand Path Capture"/"Tool Configuration"/"Pick".
 *
 * Purely presentational-adjacent, the same shape as `PlaybackController`:
 * it owns painting *session* state (the stroke currently being drawn,
 * each touched layer's own pre-paint base content) and calls straight
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
    /// @param parent The owning object, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit PaintController(QObject* parent = nullptr);

    /**
     * @brief Sets which project painting targets.
     *
     * Clears any in-progress stroke and every layer's own remembered
     * pre-paint base content (see rebuildLayerContent()'s own docs) -
     * both are meaningless once the project they refer to is gone.
     *
     * @param project The project to paint into; may be `nullptr` (nothing
     *        paintable until a real one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /// @brief The tool configuration new strokes are painted with.
    /// @return The configuration currently in effect.
    [[nodiscard]] const sound_mind::core::ToolConfiguration& toolConfiguration() const noexcept {
        return toolConfig_;
    }

    /// @brief Sets the tool configuration new strokes are painted with -
    ///        does not affect a stroke already in progress.
    /// @param config The new configuration.
    void setToolConfiguration(sound_mind::core::ToolConfiguration config) { toolConfig_ = std::move(config); }

    /**
     * @brief Starts a new freehand stroke.
     *
     * Does nothing if a project isn't set, or a stroke is already in
     * progress (call endStroke()/cancelStroke() first).
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
    /// @return `true` if a project is set and its operation log has an
    ///         active operation to undo.
    [[nodiscard]] bool canUndo() const noexcept;

    /// @brief Whether redo() would currently do anything.
    /// @return `true` if a project is set and its operation log has an
    ///         undone operation available to redo.
    [[nodiscard]] bool canRedo() const noexcept;

    /**
     * @brief Undoes the most recent active operation and rebuilds every
     *        layer painting has touched so far this session, emitting
     *        contentChanged() for each - a no-op if canUndo() is `false`.
     */
    void undo();

    /// @brief Redoes the most recently undone operation and rebuilds
    ///        every layer painting has touched so far this session,
    ///        emitting contentChanged() for each - a no-op if canRedo()
    ///        is `false`.
    void redo();

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
    /**
     * @brief Rebuilds one layer's own painted content from scratch and
     *        emits contentChanged() for it.
     *
     * The first time this is called for a given layer, that layer's
     * *current* content() is captured as its permanent pre-paint base -
     * every later rebuild replays on top of that same base, never the
     * result of a previous replay, so undo()/redo() stay correct no
     * matter how many times painting revisits this layer. This base is
     * session-only (not persisted) - see
     * `docs/sound-mind-architecture.md`'s Decisions Made for why that's
     * an acceptable, standard limitation (undo history doesn't survive a
     * save/reload in most creative software either).
     *
     * @param layer Which layer to rebuild.
     */
    void rebuildLayerContent(sound_mind::core::LayerId layer);

    /// @brief The per-project normalization scale `applyPaintOperation()`/
    /// `fitPathToPoints()` both need (see their own docs) - derived from
    /// the current project's own canvas geometry: its total duration in
    /// seconds against its total encoded frequency range in Hz.
    [[nodiscard]] double frequencyToTimeScale() const noexcept;

    sound_mind::core::Project* project_ = nullptr;
    sound_mind::core::ToolConfiguration toolConfig_;

    bool strokeInProgress_ = false;
    sound_mind::core::LayerId strokeTargetLayer_ = 0;
    std::vector<sound_mind::core::TimeFrequencyPoint> strokePoints_;
    sound_mind::core::Path previewPath_;

    std::unordered_map<sound_mind::core::LayerId, sound_mind::codec::StreamImage> baseContent_;
};

}  // namespace sound_mind::studio
