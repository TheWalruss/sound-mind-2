#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>
#include <QTimer>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/loop_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/project_change.h"

class QStatusBar;

namespace sound_mind::studio {

class LoopPanel;

/**
 * @brief Loop Mode, in one place (`v0.1.48.7`, split out of `MainWindow`):
 *        starting and stopping a session, preparing the project audio it
 *        plays along with, and turning each finished take into the
 *        "Loop Input" layer's content.
 *
 * Owns the project's LoopEngine (a new one per project, since a loop is
 * the project's own length - see setEngine()). Starting reuses the
 * project's "Loop Input" layer, or adds one with a silent placeholder
 * image, then prepares the backing audio - the whole project composited
 * as Playback would, minus the Loop layer, decoded and fitted to one loop
 * - on a BackgroundJob; the engine starts once that's ready. Every edit
 * while it runs (projectEdited()) prepares it again, coalescing a request
 * that arrives mid-run into one follow-up; the engine applies the new
 * audio at its next pass. A 33 ms timer copies each finished take into
 * the layer (one History entry each), follows the layer's gain, balance,
 * visibility and mute, and moves the playback position bar.
 *
 * @note UI thread only; the backing audio is prepared on a BackgroundJob.
 */
class LoopController : public QObject {
    Q_OBJECT

public:
    /// @brief What the controller needs from its window.
    struct Context {
        /// @brief The open project, or `nullptr`.
        std::function<sound_mind::core::Project*()> project;
        /// @brief Whether a session may start now - `false` while Record
        ///        runs (both want the input device).
        std::function<bool()> mayStart;
        /// @brief Called just before a session starts - stops Playback.
        std::function<void()> aboutToStart;
        /// @brief Called whenever a session starts or stops - the window
        ///        locks the device choices meanwhile.
        std::function<void()> runningChanged;
        /// @brief Shows (or, with `std::nullopt`, hides) the take as it's
        ///        encoded live - `CanvasWidget::setLiveOverlay()`.
        std::function<void(std::optional<sound_mind::codec::StreamImage>)> setLiveOverlay;
        /// @brief Moves the playback position bar - `CanvasWidget::setPlayheadFraction()`.
        std::function<void(std::optional<double>)> setPlayheadFraction;
        /// @brief Commits a History entry.
        std::function<void(const QString&)> commitHistory;
        /// @brief Brings the views up to date - `MainWindow::applyProjectChange()`.
        std::function<void(const ProjectChange&)> applyChange;
        /// @brief A finished take replaced the Loop layer's content: redraw
        ///        it and its Layers panel row.
        std::function<void(sound_mind::core::LayerId)> takeApplied;
    };

    /**
     * @brief Makes the controller and connects the Loop panel's Start/Stop,
     *        Keep Looping and Live-encoding view.
     * @param context The window's callbacks.
     * @param panel The Loop panel; must outlive the controller.
     * @param statusBar Where progress goes.
     * @param parent The owning QObject.
     */
    LoopController(Context context, LoopPanel* panel, QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Stops the session and waits for a running preparation; its
    ///        result is dropped.
    ~LoopController() override;

    /// @brief Replaces the engine - one per project. Stops any session first
    ///        (abandon()).
    /// @param engine The new project's engine, already configured with the
    ///        devices, gain and limiter; may be null.
    void setEngine(std::unique_ptr<sound_mind::core::LoopEngine> engine);

    /// @brief The current project's engine.
    /// @return `nullptr` until the first setEngine().
    [[nodiscard]] sound_mind::core::LoopEngine* engine() const noexcept { return engine_.get(); }

    /**
     * @brief Starts a session, or stops the running (or still preparing)
     *        one, leaving the Loop layer's content as the last take.
     *
     * Starting does nothing with no engine or project, or while
     * Context::mayStart says no (Record); otherwise it calls
     * Context::aboutToStart (stopping Playback), reuses or adds the "Loop
     * Input" layer (an added one is a History entry), and prepares the
     * backing audio - isRunning() is `true` from here on, and the engine
     * starts once that's ready.
     */
    void toggle();

    /// @brief Stops a session without a status message - when the project
    ///        it was for is replaced.
    void abandon();

    /// @brief Whether a session is running or preparing to.
    /// @return `true` from toggle()'s start until it's stopped.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Whether the backing audio is being prepared.
    /// @return `true` while the preparation job runs.
    [[nodiscard]] bool isPreparingBacking() const noexcept { return backingJob_.isRunning(); }

    /**
     * @brief Keep Looping: replays the last take unchanged instead of
     *        recording over it (`LoopEngine::setFrozen()`). Also syncs the
     *        panel's checkbox, so a direct call never leaves it stale.
     * @param keepLooping The new state; ignored by a missing engine.
     */
    void setKeepLooping(bool keepLooping);

    /// @brief Whether Keep Looping is on.
    /// @return The engine's frozen(), or `false` with no engine.
    [[nodiscard]] bool keepLooping() const noexcept { return engine_ && engine_->frozen(); }

    /// @brief Something audible changed: prepares the backing audio again
    ///        while a session runs. A no-op otherwise.
    void projectEdited();

private:
    /// @brief Prepares the backing audio on backingJob_, or marks a
    ///        follow-up if a run is in flight.
    void scheduleBacking();

    /// @brief Finishes a scheduleBacking() run: re-runs it if another edit
    ///        arrived meanwhile; otherwise hands the audio to the engine and
    ///        starts it if this was the start-up preparation.
    /// @param result The mix, or nothing if the run failed.
    /// @param cancelled Whether the run was cancelled (the session stopped).
    void finishBacking(std::optional<sound_mind::codec::AudioBuffer> result, bool cancelled);

    /// @brief The update timer's slot - see the class docs.
    void update();

    /// @brief Stops the engine, the timer and the session's state.
    void stopSession();

    /// @brief Shows `message` on the status bar, if there is one.
    /// @param message The text.
    /// @param timeoutMs How long; 0 keeps it.
    void showStatus(const QString& message, int timeoutMs = 0);

    Context context_;
    LoopPanel* panel_;
    QStatusBar* statusBar_;
    std::unique_ptr<sound_mind::core::LoopEngine> engine_;
    /// @brief Refreshes the Loop layer while a session runs.
    QTimer updateTimer_;
    /// @brief The layer being captured into, while a session runs. An id,
    ///        not a pointer: adding layers can reallocate the project's list.
    std::optional<sound_mind::core::LayerId> layerId_;
    /// @brief An edit arrived while backingJob_ was running.
    bool backingDirty_ = false;
    /// @brief Started, and waiting for the first backing audio before the
    ///        engine actually starts.
    bool startPending_ = false;
    /// @brief The Loop panel's "Live-encoding view" choice.
    bool liveView_ = false;
    /// @brief The last LoopEngine::takesCompleted() copied into the layer.
    std::uint64_t takesShown_ = 0;
    /// @brief Preparing the backing audio. Declared last, so it's destroyed
    ///        (and waited for) first.
    BackgroundJob backingJob_;
};

}  // namespace sound_mind::studio
