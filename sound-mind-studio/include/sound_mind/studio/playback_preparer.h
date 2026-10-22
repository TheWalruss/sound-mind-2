#pragma once

#include <functional>
#include <optional>

#include <QObject>
#include <QString>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/playback_panel.h"

class QPushButton;
class QStatusBar;

namespace sound_mind::studio {

/**
 * @brief Gets the project's audio ready for Playback and keeps it current
 *        (`v0.1.48.4`, split out of `MainWindow`): the first composite when
 *        Play is pressed, Repeat's background refresh after edits, and
 *        Delta/Review's jump to an edit - with the Repeat/Scope settings and
 *        the playing range they imply.
 *
 * All three composite and decode a render snapshot on a BackgroundJob, so
 * the window never waits:
 * - **prepareAndPlay()** - Play with nothing loaded: composite, load, play
 *   the whole track. Cancellable from the status bar.
 * - **scheduleRepeatRefresh()** - Repeat on, Track scope, while playing:
 *   the refreshed mix is staged and swapped in at the loop-back (or, if
 *   its length changed, reloaded in place). Edits meanwhile coalesce.
 * - **contentChanged()** - Delta/Review: playback restarts on the edited
 *   region (Delta) or from it to the end (Review), even from a stop and
 *   with Repeat off. A newer edit replaces a pending one.
 *
 * The transport itself (Play/Pause/Stop and what may play when - Loop,
 * Recording, Macros, the filter preview) stays with the window; this only
 * prepares and positions audio on its `PlaybackController`.
 *
 * @note UI thread only.
 */
class PlaybackPreparer : public QObject {
    Q_OBJECT

public:
    /// @brief What the preparer needs from its window.
    struct Context {
        /// @brief The open project, or `nullptr`.
        std::function<const sound_mind::core::Project*()> project;
        /// @brief Where playback is now, in seconds (the window tracks it).
        std::function<double()> positionSeconds;
        /// @brief A mix was loaded or staged: its integrated loudness, for
        ///        output normalization.
        std::function<void(double)> loadedLoudness;
        /// @brief Playback started from a fresh composite (Macro recording
        ///        notes the playhead).
        std::function<void()> started;
        /// @brief Shows a failure (title, message); a dialog in the Studio.
        std::function<void(const QString&, const QString&)> reportError;
    };

    /**
     * @brief Makes the preparer and adds its cancel button to `statusBar`.
     * @param context The window's callbacks.
     * @param playback The playback it prepares audio for.
     * @param statusBar Where progress, results and the cancel button go.
     * @param parent The owning QObject.
     */
    PlaybackPreparer(Context context, PlaybackController* playback, QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Cancels running preparation and waits; results are dropped.
    ~PlaybackPreparer() override;

    /// @brief Starts compositing the project for Play, then loads and plays
    ///        it (the whole track). A no-op when nothing contributes, and a
    ///        status message while a composite is already running.
    void prepareAndPlay();

    /// @brief Whether prepareAndPlay()'s composite is running.
    /// @return `true` until it has been played (or dropped).
    [[nodiscard]] bool isCompositing() const noexcept { return compositeJob_.isRunning(); }

    /// @brief Cancels prepareAndPlay()'s composite; nothing plays.
    void cancelComposite() { compositeJob_.requestCancel(); }

    /// @brief Whether Delta/Review audio for an edit is being prepared.
    /// @return `true` until it has been played (or dropped).
    [[nodiscard]] bool isPreparingEdit() const noexcept { return editJob_.isRunning(); }

    /// @brief Re-prepares the audio in the background while Repeat plays,
    ///        so the next loop-back is fresh. A no-op unless Repeat is on and
    ///        playback runs; a request during a run coalesces into one more.
    void scheduleRepeatRefresh();

    /**
     * @brief Reacts to an edit of `layer` per Repeat and Scope: Track with
     *        Repeat refreshes in the background; Delta/Review prepare
     *        playback of the edit (the layer's newest active operation's
     *        bounds) and play it; otherwise nothing.
     * @param layer The edited layer.
     */
    void contentChanged(sound_mind::core::LayerId layer);

    /// @brief Turns Repeat on or off; off widens the range back to the whole
    ///        track at once.
    /// @param enabled Whether to loop.
    void setRepeat(bool enabled);

    /// @brief Whether Repeat is on.
    /// @return It.
    [[nodiscard]] bool repeat() const noexcept { return repeat_; }

    /// @brief Sets how an edit affects playback.
    /// @param scope Track, Delta or Review.
    void setScope(PlaybackScope scope) { scope_ = scope; }

    /// @brief How an edit affects playback.
    /// @return The scope.
    [[nodiscard]] PlaybackScope scope() const noexcept { return scope_; }

    /// @brief After a manual seek: back to the whole track (the user moved
    ///        away from any Delta/Review range deliberately).
    void widenToWholeTrack();

    /// @brief A different project: pending edit playback is dropped.
    void projectChanged();

private:
    /// @brief Starts (or, if running, cancels and re-runs) edit playback.
    void startEditPreparation();
    /// @brief Loads edit audio and positions it per scope, then plays.
    void playEdit(sound_mind::codec::AudioBuffer audio);
    /// @brief Plays a finished prepareAndPlay() composite, or reports.
    void finishComposite(bool cancelled, const QString& error, std::optional<sound_mind::codec::AudioBuffer> audio,
                         double loudnessLufs);
    /// @brief Stages a finished Repeat refresh, or re-runs it.
    void finishRefresh(std::optional<sound_mind::codec::AudioBuffer> audio, double loudnessLufs);
    /// @brief Pushes the playing range (and Repeat) down to the engine; an
    ///        empty range (a single-click edit) means no restriction.
    void syncRange();
    /// @brief The range becomes the whole loaded track.
    void wholeTrackRange();

    Context context_;
    PlaybackController* playback_;
    QStatusBar* statusBar_;
    QPushButton* cancelButton_ = nullptr;
    bool repeat_ = false;
    PlaybackScope scope_ = PlaybackScope::Track;
    double rangeStartSeconds_ = 0.0;
    double rangeEndSeconds_ = 0.0;
    double loopBackSeconds_ = 0.0;
    bool refreshDirty_ = false;
    bool editDirty_ = false;
    std::optional<sound_mind::core::TimeFrequencyRect> pendingEditBounds_;
    /// @brief Declared last, so they're cancelled and waited for first.
    BackgroundJob compositeJob_;
    BackgroundJob refreshJob_;
    BackgroundJob editJob_{nullptr, std::chrono::milliseconds(15)};
};

}  // namespace sound_mind::studio
