#pragma once

#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/core/playback_engine.h"

class QTimer;

namespace sound_mind::studio {

/**
 * @brief Owns a `PlaybackEngine` and the position-polling timer around it -
 *        extracted out of `MainWindow` as part of the Phase 2.5 Refactor &
 *        Clean Up milestone (`v0.Y.23.1`).
 *
 * Deliberately project/layer-agnostic: it plays whatever audio load() is
 * given and knows nothing about `Project`/`Layer` - "which layer to play"
 * stays its owner's own decision (`MainWindow::topmostLayerWithContent()`,
 * currently), the same separation `PlaybackEngine` itself already draws at
 * the Core level. Purely presentational-adjacent, the same shape as
 * `LayersPanel`/`LoopPanel`/etc.: every position/duration update is a
 * signal the owner connects to whatever needs to reflect it
 * (`PlaybackPanel`/`CanvasWidget`, in `MainWindow`'s case) - this class
 * never reaches into either directly.
 */
class PlaybackController : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs a controller with nothing loaded, attached to the
    ///        system's real default output device (or gracefully falling
    ///        back - see `PlaybackEngine::isDeviceAvailable()`'s docs - if
    ///        none exists).
    /// @param parent The owning object, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit PlaybackController(QObject* parent = nullptr);

    /**
     * @brief Loads new audio to play, replacing anything previously
     *        loaded.
     *
     * Stops playback first if it was running - `PlaybackEngine::
     * loadAudio()` must not be called while playing (see its own docs);
     * this makes that safe to ignore at the call site. Emits
     * durationChanged() with the newly loaded audio's own length.
     *
     * @param audio The audio to play. Playback starts from its beginning.
     */
    void load(sound_mind::codec::AudioBuffer audio);

    /// @brief Whether load() has been called since construction or the
    ///        last stop()/invalidate().
    /// @return `true` if audio is currently loaded.
    [[nodiscard]] bool isLoaded() const noexcept;

    /**
     * @brief Marks the currently loaded audio stale, without stopping
     *        playback - for when the *source* a caller would load from
     *        next (a project's topmost layer, say) changed while nothing
     *        actually told this controller to stop.
     *
     * Playback already in progress keeps playing the old audio,
     * uninterrupted, exactly as if nothing happened - only isLoaded()
     * changes, so the next play() (after a pause, or after an explicit
     * stop()) needs a fresh load() first rather than silently resuming
     * stale content.
     */
    void invalidate() noexcept;

    /// @brief Starts (or resumes) playback from the current position -
    /// does nothing if nothing has been loaded yet.
    void play();

    /// @brief Pauses playback; play() resumes from the same position.
    /// isLoaded() stays `true`.
    void pause();

    /// @brief Stops playback, rewinds to the beginning, and clears
    /// isLoaded() - a fresh load() is needed before play() does anything
    /// again.
    void stop();

    /**
     * @brief Jumps playback to the given position and emits
     *        positionChanged() immediately - not just on the next timer
     *        tick, so seeking while paused still shows the new position
     *        right away. Does nothing if nothing is loaded.
     * @param positionSeconds The position to seek to, in seconds; clamped
     *        to totalSeconds().
     */
    void seek(double positionSeconds);

    /// @brief Whether playback is currently active.
    /// @return `true` if currently playing.
    [[nodiscard]] bool isPlaying() const noexcept;

    /// @brief The loaded audio's own duration, in seconds.
    /// @return `0` if nothing is loaded.
    [[nodiscard]] double totalSeconds() const noexcept;

    /**
     * @brief Switches to the named output device, right now.
     * @param deviceName The device to switch to, from
     *        availableOutputDeviceNames() - an empty string requests the
     *        system default device.
     * @return `true` if the switch succeeded.
     */
    bool setOutputDevice(const QString& deviceName);

    /// @brief The currently open output device's name.
    /// @return Empty if no device is open.
    [[nodiscard]] QString currentOutputDeviceName() const;

    /// @brief The output device names currently available.
    /// @return Device names; empty if none are available.
    [[nodiscard]] std::vector<std::string> availableOutputDeviceNames();

    /**
     * @brief Sets the output gain.
     * @param percent A percentage where `100` is unity gain; allowed above
     *        `100` for a real boost past it (clamped to
     *        `PlaybackEngine::kMaxVolume` internally).
     */
    void setVolume(int percent);

    /// @brief The current output gain.
    /// @return `1.0` is unity.
    [[nodiscard]] float volume() const noexcept;

signals:
    /// @brief Emitted once from load(), with the newly loaded audio's own
    /// duration.
    /// @param totalSeconds The new duration - see totalSeconds().
    void durationChanged(double totalSeconds);

    /// @brief Emitted ~30fps while playing, and once immediately from
    /// seek() - the current elapsed position.
    /// @param positionSeconds The current position, in seconds.
    void positionChanged(double positionSeconds);

private slots:
    /// @brief The polling timer's slot: emits positionChanged(), and stops
    /// the timer once playback has naturally ended (`PlaybackEngine::
    /// isPlaying()` clears itself at the end of the loaded audio - see its
    /// own docs).
    void poll();

private:
    /// @brief Computes the current position from engine_ and emits
    /// positionChanged() - the shared logic behind both poll() and
    /// seek()'s own immediate feedback.
    void emitPosition();

    sound_mind::core::PlaybackEngine engine_;
    QTimer* timer_ = nullptr;
    bool loaded_ = false;
};

}  // namespace sound_mind::studio
