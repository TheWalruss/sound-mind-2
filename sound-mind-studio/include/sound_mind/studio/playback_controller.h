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
 * given and knows nothing about `Project`/`Layer` - what to play (the
 * composite) stays its owner's decision, the same separation `PlaybackEngine` itself already draws at
 * the Core level. Purely presentational-adjacent, the same shape as
 * `LayersPanel`/`LoopPanel`/etc.: every position/duration update is a
 * signal the owner connects to whatever needs to reflect it
 * (`PlaybackPanel`/`CanvasWidget`, in `MainWindow`'s case) - this class
 * never reaches into either directly.
 */
class PlaybackController : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs a controller with nothing loaded.
    /// @param parent The owning object, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    /// @param deviceMode `Real` (the default) attaches to the system's
    ///        real default output device (or gracefully falls back - see
    ///        `PlaybackEngine::isDeviceAvailable()`'s docs - if none
    ///        exists). `None` never attaches to one at all - for hermetic
    ///        tests that don't care about real device I/O (most of them:
    ///        load()/play()/pause()/stop()/seek() all work identically
    ///        either way), so they don't pay the real device open/close
    ///        cost on every construction.
    explicit PlaybackController(QObject* parent = nullptr,
                                 sound_mind::core::AudioDeviceMode deviceMode = sound_mind::core::AudioDeviceMode::Real);

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

    /**
     * @brief Loads a stream of `totalSamples` that appendStream() fills as
     *        it plays (`v0.1.44.3`, Play Macro) - see
     *        `PlaybackEngine::beginStream()`. Stops playback first, as
     *        load() does.
     * @param totalSamples The whole length.
     * @param sampleRateHz Its sample rate.
     */
    void beginStream(std::size_t totalSamples, std::uint32_t sampleRateHz);

    /// @brief Appends the next stretch of a stream - see beginStream().
    /// @param audio The samples.
    void appendStream(const sound_mind::codec::AudioBuffer& audio) { engine_.appendStream(audio); }

    /// @brief The current playback position, read straight from the engine.
    /// @return Seconds.
    [[nodiscard]] double positionSeconds() const noexcept;

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

    /**
     * @brief Stages freshly prepared audio to replace the loaded audio at
     *        the engine's next loop-back - see PlaybackEngine::stageAudio().
     *
     * Playback is not interrupted and isLoaded() is unchanged. Used to keep
     * Repeat playback fresh after an edit: the new audio takes effect the
     * moment the loop restarts, with no gap.
     *
     * @param audio The replacement; must match the loaded audio's frame
     *        count and sample rate.
     * @return `false` if nothing was staged (nothing loaded, or length/rate
     *         differs) - the caller should fall back to load().
     */
    [[nodiscard]] bool stageAudio(sound_mind::codec::AudioBuffer&& audio);

    /// @brief Whether staged audio is waiting for the next loop-back.
    /// @return `true` while it is.
    [[nodiscard]] bool hasStagedAudio() const noexcept;

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

    /**
     * @brief Restricts playback to end at `rangeEndSeconds`, looping back
     *        to `loopBackSeconds` or halting there - a thin seconds<->
     *        samples conversion over `PlaybackEngine::setPlaybackRange()`
     *        (see its own docs, including Decision #217's "no separate
     *        polling or later reaction" point), the same pattern seek()
     *        itself already uses. Does nothing if nothing is loaded.
     *
     * @param loopEnabled Whether reaching `rangeEndSeconds` loops back
     *        (`true`) or halts there (`false`).
     * @param rangeEndSeconds Where the restricted range ends, in seconds.
     * @param loopBackSeconds Where a loop-back seeks to, in seconds -
     *        ignored when `!loopEnabled`.
     */
    void setPlaybackRange(bool loopEnabled, double rangeEndSeconds, double loopBackSeconds);

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

    /// @brief Why sound is not currently reaching an output device - see
    ///        `PlaybackEngine::deviceProblem()`.
    /// @return Empty if the device is working (or none is wanted, as with
    ///         `AudioDeviceMode::None`).
    [[nodiscard]] QString outputDeviceProblem() const;

    /**
     * @brief Checks, just before playing, that playback will actually be
     *        heard and at the right speed - and tries to recover first.
     *
     * If the output device isn't working (never opened, unplugged, taken
     * over by another application, ...), re-opens `preferredDeviceName`
     * once (an empty string means the system default) - so merely plugging
     * a device back in, or closing the program that grabbed it, is enough;
     * no extra step is needed. Then reports, in priority order: the device
     * still being unusable; the device running at a different sample rate
     * than the loaded audio (which would play it proportionally faster/
     * slower and higher/lower in pitch, since the engine does not
     * resample); the output gain being 0%.
     *
     * @param preferredDeviceName The device the user configured.
     * @return A short, user-presentable explanation, or empty if nothing is
     *         wrong. Never empty for a real, unusable device; always empty
     *         for `AudioDeviceMode::None` apart from the 0%-gain case.
     */
    [[nodiscard]] QString ensureOutputReady(const QString& preferredDeviceName);

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

    /// @brief Forwards to `PlaybackEngine::setLimiterEnabled()`.
    /// @param enabled Whether to limit.
    void setLimiterEnabled(bool enabled) noexcept { engine_.setLimiterEnabled(enabled); }

    /// @brief Forwards to `PlaybackEngine::setLimiterCeilingDb()`.
    /// @param ceilingDb The ceiling, dBFS.
    void setLimiterCeilingDb(float ceilingDb) noexcept { engine_.setLimiterCeilingDb(ceilingDb); }

    /// @brief Forwards to `PlaybackEngine::setNormalizationGainDb()`.
    /// @param gainDb The gain.
    void setNormalizationGainDb(float gainDb) noexcept { engine_.setNormalizationGainDb(gainDb); }

    /// @brief The engine's current normalization gain.
    /// @return The gain, dB.
    [[nodiscard]] float normalizationGainDb() const noexcept { return engine_.normalizationGainDb(); }

    /// @brief Whether the engine's limiter is on.
    /// @return The flag.
    [[nodiscard]] bool limiterEnabled() const noexcept { return engine_.limiterEnabled(); }

signals:
    /// @brief Emitted from invalidate(): whatever was prepared no longer
    /// matches the project. Lets a Repeat session re-prepare in the
    /// background so the next loop restart is fresh.
    void invalidated();

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
