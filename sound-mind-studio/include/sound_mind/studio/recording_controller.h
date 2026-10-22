#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>
#include <QTimer>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/stream_incremental_encoder.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/record_engine.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/project_change.h"

class QStatusBar;

namespace sound_mind::studio {

class RecordPanel;

/**
 * @brief Record, in one place (`v0.1.48.6`, split out of `MainWindow`):
 *        starting and stopping the take, draining the input while it
 *        runs (the live-encoding view and the moving playback position),
 *        and saving it into the project in the background.
 *
 * The RecordEngine itself stays with the window, which also hands it the
 * configured input device and gain (Configure Devices); this drives it.
 * A stopped take is encoded and written to the project's sources on a
 * BackgroundJob, then added - over the Record panel's chosen recorded
 * layer, or as a new one - on the UI thread, with one History entry.
 *
 * @note UI thread only; the save runs on a BackgroundJob.
 */
class RecordingController : public QObject {
    Q_OBJECT

public:
    /// @brief What the controller needs from its window.
    struct Context {
        /// @brief The open project, or `nullptr`.
        std::function<sound_mind::core::Project*()> project;
        /// @brief Whether a take may start now - `false` while Loop Mode
        ///        runs (both want the input device).
        std::function<bool()> mayStart;
        /// @brief Called just before a take starts - stops Playback.
        std::function<void()> aboutToStart;
        /// @brief Called whenever capture starts or stops - the window
        ///        locks the input device choice meanwhile.
        std::function<void()> recordingChanged;
        /// @brief Shows (or, with `std::nullopt`, hides) the take as it's
        ///        encoded live - `CanvasWidget::setLiveOverlay()`.
        std::function<void(std::optional<sound_mind::codec::StreamImage>)> setLiveOverlay;
        /// @brief Moves the playback position bar - `CanvasWidget::setPlayheadFraction()`.
        std::function<void(std::optional<double>)> setPlayheadFraction;
        /// @brief Commits a History entry.
        std::function<void(const QString&)> commitHistory;
        /// @brief Brings the views up to date - `MainWindow::applyProjectChange()`.
        std::function<void(const ProjectChange&)> applyChange;
        /// @brief Shows a failure (title, message); a dialog in the Studio.
        std::function<void(const QString&, const QString&)> reportError;
    };

    /// @brief What a recording becomes once encoded and stored - the part
    ///        of commit() that's safe off the UI thread.
    struct Prepared {
        sound_mind::codec::StreamImage content;                ///< The encoded take.
        std::optional<sound_mind::core::ProjectSource> source;  ///< Its FLAC source, unregistered; empty if saving failed.
    };

    /**
     * @brief Makes the controller and connects the Record panel's Start/Stop
     *        and Live-encoding view.
     * @param context The window's callbacks.
     * @param engine The capture engine; must outlive the controller.
     * @param panel The Record panel; must outlive the controller.
     * @param statusBar Where progress and results go.
     * @param parent The owning QObject.
     */
    RecordingController(Context context, sound_mind::core::RecordEngine& engine, RecordPanel* panel,
                        QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Waits for a pending save and drops its result - the project
    ///        it was for is going away with the window.
    ~RecordingController() override;

    /**
     * @brief Starts a take, or stops the running one and saves it
     *        (commitAsync()).
     *
     * Starting does nothing with no project open, or while
     * Context::mayStart says no (Loop Mode); otherwise it calls
     * Context::aboutToStart (stopping Playback) first. A start with no input
     * device still "records" - nothing is captured, which the status bar
     * says - so stopping then leaves the project unchanged.
     */
    void toggle();

    /// @brief Whether a take is being captured.
    /// @return The engine's isRecording().
    [[nodiscard]] bool isRecording() const noexcept { return engine_.isRecording(); }

    /// @brief Stops a running take without saving it - when the project it
    ///        was for is replaced.
    void abandon();

    /**
     * @brief Stores a finished recording where the Record panel says:
     *        replacing the content of the chosen recorded layer (one
     *        undoable step), or as a new layer with the panel's own name,
     *        marked as recorded so it's offered as a target next time.
     * @param captured The recorded audio.
     * @return `false` (nothing changes) with no project open or nothing
     *         captured; a failure is reported through Context::reportError.
     */
    bool commit(const sound_mind::codec::AudioBuffer& captured);

    /**
     * @brief commit() with the slow part - encoding the take and writing it
     *        to the project's sources as FLAC - on a background task, so
     *        stopping a long recording doesn't freeze the window. The layer
     *        appears when it's done. Saves run one at a time, in order.
     * @param captured The recorded audio. Ignored (with a status message)
     *        if nothing was captured or no project is open.
     */
    void commitAsync(sound_mind::codec::AudioBuffer captured);

    /// @brief Whether commitAsync()'s save is still pending.
    /// @return `true` until its result has been applied.
    [[nodiscard]] bool isSaving() const noexcept { return saveJob_.isRunning(); }

    /// @brief Waits for a pending commitAsync() save and applies it - before
    ///        New/Open/Close, so the take is part of the project they ask
    ///        about. A no-op when none is pending.
    void flushSave() { saveJob_.finishNow(); }

    /// @brief Gives the Record panel the project's current recorded layers
    ///        as targets.
    void refreshTargets();

    /// @brief Encodes `captured` and writes it as a source - thread-safe;
    ///        touches no controller state.
    /// @param captured The recorded audio.
    /// @param config The project's codec settings.
    /// @param sourcesFolder The project's sources folder, if it has one.
    /// @return The prepared recording.
    /// @throws std::exception if encoding fails (a failed source write is
    ///         not an error: the take is kept without a source).
    [[nodiscard]] static Prepared prepare(const sound_mind::codec::AudioBuffer& captured,
                                          const sound_mind::codec::StreamCodecConfig& config,
                                          const std::optional<std::filesystem::path>& sourcesFolder);

private:
    /// @brief The drain timer's slot: moves whatever's newly captured out of
    ///        the engine's ring buffer (see RecordEngine::drainAvailable()),
    ///        feeds the live view and moves the playback position bar, from
    ///        the canvas's start until it reaches the end.
    void drain();

    /// @brief Stops the engine, the timer and the live view.
    void stopCapture();

    /// @brief Adds a prepared recording to the project, where the Record
    ///        panel says - the UI-thread half of commit().
    /// @param prepared The encoded take and its source.
    /// @return `true` on success; a failure is reported.
    bool apply(Prepared prepared);

    Context context_;
    sound_mind::core::RecordEngine& engine_;
    RecordPanel* panel_;
    QStatusBar* statusBar_;
    /// @brief Drains the engine while recording - often enough that its
    ///        ring buffer (~370 ms) never fills, and the position bar moves
    ///        smoothly.
    QTimer drainTimer_;
    /// @brief The Record panel's "Live-encoding view" choice.
    bool liveView_ = false;
    /// @brief Encodes the take in progress for the live view; liveFed_ is
    ///        how many captured samples it has seen.
    std::unique_ptr<sound_mind::codec::StreamIncrementalEncoder> liveEncoder_;
    std::size_t liveFed_ = 0;
    /// @brief Saving a stopped take. Declared last, so it's destroyed (and
    ///        waited for) first.
    BackgroundJob saveJob_;
};

}  // namespace sound_mind::studio
