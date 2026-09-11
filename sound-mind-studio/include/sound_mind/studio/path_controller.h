#pragma once

#include <optional>

#include <QObject>

#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::studio {

class PaintController;

/**
 * @brief Owns an in-progress, deliberately node-by-node-placed Path and
 *        turns it into a real, undoable `PaintOperation` - see
 *        `docs/sound-mind-design.md`'s "Placing and Editing".
 *
 * The deliberate counterpart to `PaintController`'s own freehand capture:
 * where a freehand stroke's Path is curve-fit from continuous raw cursor
 * motion, this controller builds one from a sequence of discrete node
 * placements (placeNode(), one per click), each appended to the path
 * exactly as given - no curve-fitting, no simplification. Once placement
 * finishes (finishPath()), the result becomes a fresh, non-superseding
 * `PaintOperation`, exactly like a freehand stroke's own endStroke() -
 * both end up the same kind of paint object, editable identically once
 * Picked (see the design doc's "Pick").
 *
 * **This installment's own scope: placement only.** Every newly placed
 * node uses defaultNodeType()'s own standing default (`Corner` unless
 * flipped via setDefaultNodeType()); a `Smooth` node placed this way
 * starts with both handles collapsed onto its own anchor, per `PathNode`'s
 * own docs ("even if collapsed onto anchor itself, for a freshly placed
 * node with no curve pulled out of it yet"). Re-opening a path already
 * placed (or already Picked) for node/handle editing - moving a node,
 * dragging a handle to actually pull a curve out of it, inserting or
 * deleting a node, converting a node's type after the fact - is a
 * deliberately deferred follow-up installment (see
 * `docs/sound-mind-architecture.md`'s Decisions Made for the full
 * reasoning).
 *
 * Purely presentational-adjacent, the same shape as `PaintController`/
 * `PickController`/`SelectionController`: owns Path-tool *session* state
 * and calls straight into `sound_mind::core` for the actual log logic,
 * never reaching into `CanvasWidget` directly - `pathChanged()`/
 * `contentChanged()` are what `MainWindow` connects to whatever needs to
 * reflect them. Converting raw mouse input into
 * `sound_mind::core::TimeFrequencyPoint`s is the caller's own job, same as
 * every other controller here. Shares `PaintController`'s own per-layer
 * pre-paint base cache (see its own docs) rather than keeping a second
 * one, so it's constructed after `paintController_` and holds a pointer to
 * it.
 */
class PathController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a controller with no project set and no placement
     *        in progress.
     * @param paintController The controller whose rebuildLayerContent()
     *        this one calls after finishPath() commits - see the class's
     *        own docs on why the pre-paint base cache is shared, not
     *        duplicated. Not owned; must outlive this object.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit PathController(PaintController* paintController, QObject* parent = nullptr);

    /**
     * @brief Sets which project the Path tool targets.
     *
     * Cancels any in-progress placement - meaningless once the project it
     * refers to is gone.
     *
     * @param project The project to place paths into; may be `nullptr`
     *        (nothing placeable until a real one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /// @brief The tool configuration a finished path is committed with -
    ///        same role as `PaintController::toolConfiguration()`, for the
    ///        same reason: `finishPath()` seeds the new path's own
    ///        gradient from `config.defaultGradient()`.
    /// @return The configuration currently in effect.
    [[nodiscard]] const sound_mind::core::ToolConfiguration& toolConfiguration() const noexcept {
        return toolConfig_;
    }

    /// @brief Sets the tool configuration a finished path is committed
    ///        with - does not affect a placement already in progress
    ///        (only applied at finishPath() time).
    /// @param config The new configuration.
    void setToolConfiguration(sound_mind::core::ToolConfiguration config) { toolConfig_ = std::move(config); }

    /// @brief Which node type placeNode() places next - the design doc's
    ///        own "standing default that can be flipped at any time".
    /// @return The type new nodes are currently placed as.
    [[nodiscard]] sound_mind::core::PathNodeType defaultNodeType() const noexcept { return defaultNodeType_; }

    /// @brief Sets which node type placeNode() places next - affects only
    ///        nodes placed after this call, not ones already placed.
    /// @param type The new standing default.
    void setDefaultNodeType(sound_mind::core::PathNodeType type) noexcept { defaultNodeType_ = type; }

    /**
     * @brief Places one more node, appending it to the path currently
     *        being built.
     *
     * The first call starts a new placement session, capturing
     * `targetLayer` as the path's own eventual target - every subsequent
     * call until finishPath()/cancelPath() appends to that same path,
     * regardless of what's passed as `targetLayer` again (matching
     * `PaintController::beginStroke()`'s own "target captured once, at the
     * start" precedent). The new node is defaultNodeType()'s own current
     * type; a `Smooth` node starts with both handles collapsed onto its
     * own anchor (see the class's own docs).
     *
     * Emits pathChanged() so the caller can redraw the live preview.
     *
     * @param targetLayer Which layer this path will eventually paint into
     *        - only consulted on the first call of a new session.
     * @param point The new node's own anchor, already converted to time/
     *        frequency space.
     */
    void placeNode(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Updates the cursor position currentPreviewPath() draws a
     *        live rubber-band segment out to, from the most recently
     *        placed node. A no-op if no placement is in progress.
     *
     * Emits pathChanged() - cheap enough to call on every real mouse-move
     * event, the same "recompute and redraw on every sample" precedent
     * `PaintController::continueStroke()` already establishes.
     *
     * @param point The cursor's current position, in time/frequency space.
     */
    void updateCursor(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Finishes the in-progress placement: builds the final Path
     *        (seeded with the current tool configuration's own default
     *        gradient, exactly like `PaintController::endStroke()`'s own
     *        docs), appends a new, non-superseding `PaintOperation` to the
     *        project's `OperationLog`, and rebuilds the target layer's own
     *        painted content.
     *
     * A no-op if no placement is in progress, or if no node has been
     * placed yet (nothing to commit).
     *
     * Emits contentChanged() for the target layer, and pathChanged() to
     * clear the now-finished live preview.
     */
    void finishPath();

    /// @brief Abandons the in-progress placement without painting
    ///        anything - a no-op if no placement is in progress. Emits
    ///        pathChanged() to clear the live preview.
    void cancelPath();

    /// @brief Whether a path is currently being placed.
    /// @return `true` between a first placeNode() and its own
    ///         finishPath()/cancelPath().
    [[nodiscard]] bool isPlacementInProgress() const noexcept { return placementActive_; }

    /**
     * @brief The in-progress placement's own live preview Path - every
     *        node placed so far, plus (while updateCursor() has a
     *        position to show) one extra transient `Corner` node at the
     *        cursor's own current position, for a live rubber-band segment
     *        out to it - never part of the real, committed path itself.
     * @return The current live preview; empty (no nodes) if no placement
     *         is in progress.
     */
    [[nodiscard]] sound_mind::core::Path currentPreviewPath() const;

signals:
    /// @brief Emitted whenever the in-progress placement's own live
    ///        preview Path changes (placeNode(), updateCursor()), and once
    ///        more when it's cleared (finishPath()/cancelPath()).
    void pathChanged();

    /// @brief Emitted whenever a layer's own rendered content changes as
    ///        a result of a committed finishPath().
    /// @param layer Which layer's content changed.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;
    sound_mind::core::ToolConfiguration toolConfig_;
    sound_mind::core::PathNodeType defaultNodeType_ = sound_mind::core::PathNodeType::Corner;

    bool placementActive_ = false;
    sound_mind::core::LayerId targetLayer_ = 0;
    sound_mind::core::Path path_;
    std::optional<sound_mind::core::TimeFrequencyPoint> lastCursor_;
};

}  // namespace sound_mind::studio
