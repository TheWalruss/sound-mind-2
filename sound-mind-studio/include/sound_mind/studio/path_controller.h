#pragma once

#include <optional>
#include <vector>

#include <QObject>

#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class PaintController;

/**
 * @brief The Path tool: draws a silent guide path freehand, exactly like a
 *        Paint stroke but stamping nothing (Pre-Beta Plan `v0.Y.77.1`).
 *
 * A drag (beginPath(), continuePath() per mouse move, endPath()) is
 * curve-fitted live with the same `fitPathToPoints()` call
 * `PaintController` uses, so the preview grows as the cursor moves, and the
 * finished stroke is committed as a `sound_mind::core::GuidePathOperation`
 * with a single branch - which paints nothing, but can be Picked, moved,
 * node-edited (the same edit session a painted stroke gets), stroked with a
 * brush, merged into a branching path, or turned into a Resonance Profile.
 * This replaced the earlier click-one-node-at-a-time placement.
 *
 * Owns only the in-progress drag; `PaintController` is used solely for its
 * undo bookkeeping (`notifyOperationCommitted()`), since a guide path never
 * changes a layer's content and needs no rebuild. Converting mouse input to
 * `TimeFrequencyPoint`s is the caller's job, as for every controller here.
 */
class PathController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a controller with no project set and no stroke in
     *        progress.
     * @param paintController Provides the undo-stack hookup for a commit -
     *        not owned; must outlive this object.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit PathController(PaintController* paintController, QObject* parent = nullptr);

    /// @brief Sets which project the Path tool draws into, cancelling any
    ///        stroke in progress.
    /// @param project The project; may be `nullptr` (nothing drawable).
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Starts a new stroke at `point` on `targetLayer` - a no-op with
     *        no project, or while a stroke is already in progress.
     * @param targetLayer The layer the finished guide path will belong to.
     * @param point The press position, in time/frequency space.
     */
    void beginPath(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Adds `point` to the stroke in progress and refits the live
     *        preview - a no-op with no stroke in progress. Emits
     *        pathChanged().
     * @param point The cursor's new position, in time/frequency space.
     */
    void continuePath(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Finishes the stroke: commits the fitted path as a new
     *        single-branch `GuidePathOperation` (one undoable step) and
     *        clears the preview.
     *
     * A press with no drag (fewer than two distinct points) commits
     * nothing - a guide path needs a direction. Emits pathChanged(), and
     * contentChanged() for the target layer when something was committed.
     *
     * @return The new operation's id, or `std::nullopt` if nothing was
     *         committed.
     */
    std::optional<sound_mind::core::OperationId> endPath();

    /// @brief Abandons the stroke in progress without committing anything -
    ///        a no-op if there isn't one. Emits pathChanged().
    void cancelPath();

    /// @brief Whether a stroke is being drawn.
    /// @return `true` between beginPath() and endPath()/cancelPath().
    [[nodiscard]] bool isPathInProgress() const noexcept { return inProgress_; }

    /// @brief The live, curve-fitted preview of the stroke in progress.
    /// @return The preview; empty (no nodes) when nothing is being drawn.
    [[nodiscard]] const sound_mind::core::Path& currentPreviewPath() const noexcept { return previewPath_; }

signals:
    /// @brief The live preview changed (continuePath()), or was cleared
    ///        (endPath()/cancelPath()).
    void pathChanged();

    /// @brief A guide path was committed onto `layer`. Its content is
    ///        unchanged - this is the cue to redraw and mark the project
    ///        modified.
    /// @param layer The layer the guide path belongs to.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    /// @brief Fits `points_` into a Path, as `PaintController` does.
    [[nodiscard]] sound_mind::core::Path fittedPath() const;

    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;

    bool inProgress_ = false;
    sound_mind::core::LayerId targetLayer_ = 0;
    std::vector<sound_mind::core::TimeFrequencyPoint> points_;
    sound_mind::core::Path previewPath_;
};

}  // namespace sound_mind::studio
