#pragma once

#include <optional>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::studio {

/**
 * @brief Owns direct node/handle editing of a single `Path` - the state
 *        and geometry behind `PickController::beginPathEdit()`'s own
 *        session (see its own class docs for the feature itself),
 *        extracted out of `PickController` as part of the Refactor &
 *        Clean Up milestone (`v0.Y.29.1`, Installment E): `PickController`
 *        does two largely independent jobs (whole-object picking/moving,
 *        and - only once something Pickable is selected - this node-level
 *        editing of its own Path), and every one of `pick()`/
 *        `continueMove()`/`endMove()`/`deleteSelection()` opened with the
 *        same `if (path edit active) { delegate here instead; return; }`
 *        prologue - the tell that this half belonged in its own class.
 *
 * A plain value type, not a `QObject` - it emits nothing itself;
 * `PickController` calls in, reads back what happened (a `bool` success/
 * no-op result, or the state accessors below), and emits its own
 * `pathChanged()` signal accordingly. Knows nothing about `Operation`/
 * `PaintOperation`/`OperationLog`/`Project` at all - `begin()` takes the
 * starting `Path` directly (not the `Operation` it came from), and every
 * other method takes exactly the inputs it needs (a point, already-
 * resolved codec config/settings) rather than reaching for `Project`
 * itself - keeping this class committing to nothing about *why* a Path is
 * being edited, only *how*.
 */
class PathEditSession {
public:
    /// @brief Starts a session editing a copy of `initialPath`, with no
    ///        node selected yet.
    /// @param initialPath The Path to copy into this session's own
    ///        `previewPath()` - typically the picked `PaintOperation`'s
    ///        own current `path()`.
    void begin(const sound_mind::core::Path& initialPath);

    /// @brief Ends the session (whether committed or cancelled elsewhere -
    ///        this class doesn't distinguish the two, since it doesn't
    ///        know about `Operation`/logging at all): clears
    ///        `isActive()`, `previewPath()`, and `selectedNodeIndex()`. A
    ///        no-op if not currently active.
    void end();

    /// @brief Whether a session is currently active.
    /// @return `true` between a `begin()` and its own `end()`.
    [[nodiscard]] bool isActive() const noexcept { return active_; }

    /// @brief The session's own current (possibly already-modified, not
    ///        yet committed by whatever owns this session) Path.
    /// @return The current preview; empty (no nodes) if not active.
    [[nodiscard]] const sound_mind::core::Path& previewPath() const noexcept { return previewPath_; }

    /// @brief Which node of `previewPath()` is currently selected.
    /// @return The selected node's own index, or `std::nullopt` if
    ///         nothing is currently selected (including whenever the
    ///         session isn't active at all).
    [[nodiscard]] std::optional<std::size_t> selectedNodeIndex() const noexcept { return selectedNodeIndex_; }

    /**
     * @brief Hit-tests every node's anchor and (for a `Smooth` node) both
     *        handles in `previewPath()` against `point`, selecting
     *        whichever is closest within a small, fixed tolerance
     *        (handles win a near-tie over anchors, matching `docs/sound-
     *        mind-design.md`'s own handle-first editing emphasis), and
     *        arms a potential drag from `point` if something was hit.
     *
     * @param point The click position, in time/frequency space.
     * @param scale `frequencyToTimeScaleFor()`'s own result - the shared
     *        normalized-space yardstick between the time and frequency
     *        axes (see `paint_application.h`'s own docs).
     * @return `true` if a node/handle was selected; `false` (clearing the
     *         current node selection) otherwise.
     */
    bool selectNodeNear(sound_mind::core::TimeFrequencyPoint point, double scale);

    /**
     * @brief Continues the drag `selectNodeNear()` armed, applying the
     *        total delta from its own anchor point directly to
     *        `previewPath()` - a no-op if nothing is currently selected.
     *
     * Dragging a node's own anchor moves both its handles along with it;
     * dragging a `Smooth` node's own handle reshapes that side of the
     * curve, with the opposite handle always mirroring through the anchor
     * to keep the tangent smooth (no detach-to-corner gesture yet).
     *
     * @param point The cursor's current position, in time/frequency
     *        space - already grid-snapped by the caller, if applicable
     *        (this class has no concept of Snap to Grid of its own).
     * @param config Converts the drag's own frequency delta into bins
     *        (the frequency axis is log-scaled - see `frequencyBinDelta()`'s
     *        own docs in `path_edit_session.cpp`) and translates a shifted
     *        point back (`translateFrequencyByBins()`).
     * @return `true` if a node/handle was actually dragged; `false`
     *         (nothing changed) if nothing is currently selected.
     */
    bool continueDrag(sound_mind::core::TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config);

    /// @brief Deletes the currently selected node - a no-op (returns
    ///        `false`) if nothing is selected, or only one node remains
    ///        (refusing to edit a path down to nothing mid-session).
    /// @return `true` if a node was actually deleted.
    bool deleteSelectedNode();

    /**
     * @brief Converts the currently selected node between `Corner` and
     *        `Smooth` - a no-op (returns `false`) if nothing is selected.
     *
     * Converting to `Smooth` extends both handles a comfortable distance
     * from the node's own anchor, along the tangent that rounds the
     * corner its own two neighbors form symmetrically between them (an
     * isolated single-node path, with no neighbor to take a direction
     * from, collapses the handles onto the anchor instead, for lack of a
     * better direction). Converting to `Corner` discards both handles.
     *
     * @param settings Supplies `frequencyToTimeScaleFor()`'s own
     *        normalized-space scale, for measuring handle length/
     *        direction against the path's own neighboring nodes.
     * @return `true` if a node was actually toggled.
     */
    bool toggleSelectedNodeType(const sound_mind::core::ProjectSettings& settings);

private:
    /// @brief Which part of a `PathNode` a node/handle hit-test or drag
    ///        refers to - see `selectNodeNear()`'s own docs.
    enum class NodePart { Anchor, HandleIn, HandleOut };

    bool active_ = false;
    sound_mind::core::Path previewPath_;
    std::optional<std::size_t> selectedNodeIndex_;
    NodePart selectedNodePart_ = NodePart::Anchor;

    /// @brief Where the current node/handle drag started, in time/
    ///        frequency space - set by `selectNodeNear()`.
    sound_mind::core::TimeFrequencyPoint dragAnchor_;

    /// @brief A snapshot of `previewPath_` taken at the start of the
    ///        current node/handle drag, so `continueDrag()` always
    ///        applies the *total* delta from `dragAnchor_` to the
    ///        original position, rather than compounding small per-call
    ///        deltas.
    sound_mind::core::Path dragStart_;
};

}  // namespace sound_mind::studio
