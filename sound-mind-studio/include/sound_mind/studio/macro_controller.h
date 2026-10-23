#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <QObject>
#include <QString>
#include <QTimer>

#include "sound_mind/core/macro.h"
#include "sound_mind/core/macro_renderer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/macro_recorder.h"
#include "sound_mind/studio/project_change.h"

class QStatusBar;

namespace sound_mind::studio {

class MacroPlayer;
class ProjectHistory;

/**
 * @brief Macros, in one place (`v0.1.48.8`, split out of `MainWindow`):
 *        recording a performance, building a recorded one's renderer, and
 *        playing it back.
 *
 * Owns the MacroRecorder and the ~33 ms timer that reports the playhead to
 * it while recording (it's also told on every transport change -
 * notePlayhead()). The playhead comes from the window (Context::playhead):
 * Loop Mode's while it runs, otherwise Playback's. The MacroPlayer itself
 * is the window's, since it draws on the canvas and drives Playback; this
 * starts and stops it. Choosing a Macro and the Manage Macros dialog stay
 * with the window's menus.
 *
 * @note UI thread only.
 */
class MacroController : public QObject {
    Q_OBJECT

public:
    /// @brief What the controller needs from its window.
    struct Context {
        /// @brief The open project, or `nullptr`.
        std::function<sound_mind::core::Project*()> project;
        /// @brief The transport now: position in seconds, and whether it runs.
        std::function<std::pair<double, bool>()> playhead;
        /// @brief The master limiter's ceiling, or `std::nullopt` when it's off.
        std::function<std::optional<float>()> limiterCeilingDb;
        /// @brief Called just before a Macro plays - stops Playback.
        std::function<void()> aboutToPlay;
        /// @brief Brings the views up to date - `MainWindow::applyProjectChange()`.
        std::function<void(const ProjectChange&)> applyChange;
    };

    /**
     * @brief Makes the controller.
     * @param context The window's callbacks.
     * @param history The project's History, which the recorder listens to;
     *        must outlive the controller.
     * @param player Plays Macros back; may be null (then play() fails).
     * @param statusBar Where progress and results go; may be null.
     * @param parent The owning QObject.
     */
    MacroController(Context context, ProjectHistory* history, MacroPlayer* player, QStatusBar* statusBar,
                    QObject* parent = nullptr);

    /**
     * @brief Record Macro: on, starts recording a new Macro named "Macro N"
     *        from the playhead now; off, stops it, keeping it in the project
     *        with its duration. Either is a settings-only change. A no-op
     *        with no project, or when already in the requested state.
     * @param enabled Whether to record.
     */
    void setRecordingEnabled(bool enabled);

    /// @brief Whether a Macro is being recorded.
    /// @return The recorder's isRecording().
    [[nodiscard]] bool isRecording() const noexcept { return recorder_.isRecording(); }

    /// @brief Reports the playhead to the recorder while recording - on
    ///        every position update, transport change and timer tick. A
    ///        no-op otherwise.
    void notePlayhead();

    /**
     * @brief A renderer for a recorded Macro: its start state rebuilt from
     *        the project's history, replaying sources as recipes, with the
     *        master limiter as it is now.
     * @param id The Macro.
     * @param errorMessage Receives the reason on failure; may be null.
     * @return The renderer; `nullptr` if the Macro is gone or its project's
     *         history doesn't reach back to its creation.
     */
    [[nodiscard]] std::unique_ptr<sound_mind::core::MacroRenderer> rendererFor(sound_mind::core::MacroId id,
                                                                               QString* errorMessage = nullptr) const;

    /**
     * @brief Plays a recorded Macro on the MacroPlayer, after
     *        Context::aboutToPlay. Rendering happens ahead, on a copy - the
     *        project never changes.
     * @param id The Macro.
     * @param errorMessage Receives the reason on failure; may be null.
     * @return Whether it started.
     */
    bool play(sound_mind::core::MacroId id, QString* errorMessage = nullptr);

    /// @brief Forgets a recording in progress and stops a playing Macro -
    ///        the project is being replaced.
    void projectChanged();

private:
    Context context_;
    MacroPlayer* player_;
    QStatusBar* statusBar_;
    MacroRecorder recorder_;
    /// @brief Reports the playhead to recorder_ every ~33 ms while
    ///        recording, so its clock follows the transport smoothly.
    QTimer playheadTimer_;
};

}  // namespace sound_mind::studio
