#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::core {

/**
 * @brief Whether a PlaybackEngine should attach to a real audio device.
 *
 * `None` exists specifically for tests that call renderBlock() directly:
 * with a real device attached, JUCE's own background callback thread
 * would *also* be calling renderBlock() concurrently (whenever the OS
 * audio subsystem wants a buffer), racing against the test's own direct
 * calls on the same shared atomics - `position_` in particular ends up
 * advanced by whichever caller runs first, which is exactly the kind of
 * hard-to-reproduce flakiness this sidesteps entirely rather than papering
 * over with locks or timing assumptions.
 */
enum class AudioDeviceMode {
    Real,  ///< Attaches to the system's real default output device (normal use).
    None,  ///< Never attaches to a real device - for hermetic, device-independent tests.
};

/**
 * @brief Plays back a fixed, pre-decoded audio buffer through the system's
 *        default output device.
 *
 * A first, minimal playback engine per `docs/sound-mind-roadmap.md`'s
 * Playback milestone (`v0.0.4.1`): the audio to play is decoded once, in
 * full, before playback starts (see loadAudio()) - the audio callback
 * itself just reads sequentially from that fixed buffer via an atomic
 * position index, satisfying `CLAUDE.md`'s non-negotiable real-time
 * constraint (no allocation, no locking, on the audio thread) without
 * needing the design doc's eventual always-current, no-separate-render-
 * step model. That fuller model is Live Mode's job (`v0.Y.7.1`) - see the
 * roadmap for why.
 *
 * **As of `v0.Y.16.1` (Transport Panels):** output device selection
 * (availableOutputDeviceNames()/setPreferredOutputDevice()/
 * currentOutputDeviceName()) and a gain-boost-capable volume control
 * (setVolume()/volume(), allowed above unity - see kMaxVolume) - both
 * previously-deferred scope, per the Playback panel this milestone adds.
 *
 * **As of Decision #217 (Gapless Repeat Playback):** `setPlaybackRange()`
 * lets a caller restrict playback to `[0, rangeEndSamples)` and choose
 * what happens on reaching that end - loop back to `loopBackSamples`, or
 * halt right there. Both `renderBlock()` and `seek()` apply this
 * sample-accurately and entirely within this class: a loop-back that
 * falls mid-block keeps filling the rest of that same block with real
 * audio from the new position, with no silence at the seam. This
 * replaces `sound_mind::studio::MainWindow`'s former approach of
 * noticing a range had been crossed from its own ~33ms position-polling
 * timer and reacting with a separate `seek()`/`play()` call afterward -
 * which always left an audible gap between the natural end and that
 * later reaction. See `docs/sound-mind-architecture.md`'s own Decision
 * #217 for the full reasoning.
 *
 * @note Thread-safety: `loadAudio()`/`play()`/`pause()`/`stop()`/
 *       `setPreferredOutputDevice()` are meant to be called from the UI
 *       thread only. `loadAudio()` must not be called while playing
 *       (`stop()` first) - swapping the buffer while the audio thread
 *       might be reading it isn't safe, and this first pass doesn't build
 *       the lock-free double-buffering that would allow it. `renderBlock()`
 *       is the one method actually called from the audio callback thread
 *       (indirectly, via JUCE) - it is real-time-safe (no allocation, no
 *       locking, only atomics and array indexing); `setVolume()`/`volume()`/
 *       `setPlaybackRange()` are real-time-safe too (plain atomics),
 *       safely callable from either thread.
 */
class PlaybackEngine : private juce::AudioIODeviceCallback {
public:
    /// @brief Constructs a PlaybackEngine.
    /// @param deviceMode `Real` (the default) attaches to the system's
    ///        real output device, falling back gracefully
    ///        (isDeviceAvailable() == false) if none exists; `None` never
    ///        attaches to one at all - see AudioDeviceMode's docs.
    explicit PlaybackEngine(AudioDeviceMode deviceMode = AudioDeviceMode::Real);
    ~PlaybackEngine() override;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    /**
     * @brief Loads the audio to play, replacing anything previously loaded.
     *
     * Also clears any previously set setPlaybackRange() restriction back to
     * its default (the whole of the newly loaded audio, not looping) - a
     * caller that wants a restriction on the new audio too must call
     * setPlaybackRange() again after this.
     *
     * @param audio The audio to play. Playback starts from its beginning.
     */
    void loadAudio(sound_mind::codec::AudioBuffer audio);

    /// @brief Starts (or resumes) playback from the current position.
    void play();

    /// @brief Pauses playback; play() resumes from the same position.
    void pause();

    /// @brief Stops playback and rewinds to the beginning.
    void stop();

    /// @brief Whether playback is currently active.
    /// @return `true` between a play() and the next pause()/stop() (or the
    ///         end of the loaded audio), `false` otherwise.
    [[nodiscard]] bool isPlaying() const noexcept;

    /**
     * @brief Whether a real output device was actually opened.
     *
     * `false` means playback state (play()/pause()/isPlaying()) still
     * works, but nothing will actually be heard - a defensive fallback for
     * environments with no audio hardware (CI runners, in particular),
     * rather than throwing or crashing when none is found.
     *
     * @return `true` if a real output device is open.
     */
    [[nodiscard]] bool isDeviceAvailable() const noexcept;

    /**
     * @brief Fills one block of output with the next samples to play.
     *
     * This is the engine's actual real-time-safe rendering logic, exposed
     * as its own method - separately from the `AudioIODeviceCallback`
     * interface it's normally invoked through - specifically so it's
     * testable without a real audio device.
     *
     * @param outputChannelData Pointers to each output channel's sample
     *        array, each at least `numSamples` long. Channel 0 gets the
     *        loaded audio's left channel, channel 1 (if present) gets
     *        right; any further channels are silenced.
     * @param numOutputChannels Number of channels in `outputChannelData`.
     * @param numSamples Number of samples to fill in each channel.
     *
     * @note Real-time-safe: no allocation, no locking, no exceptions.
     */
    void renderBlock(float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept;

    /**
     * @brief The output device names currently available, for the device
     *        manager's current device type - see
     *        `sound_mind::core::availableAudioDeviceNames()`'s own docs.
     * @return Device names; empty if none are available (or
     *         `AudioDeviceMode::None` was used and no device type exists).
     */
    [[nodiscard]] std::vector<std::string> availableOutputDeviceNames();

    /**
     * @brief Switches to the named output device, right now.
     *
     * Unlike `LoopEngine`/`RecordEngine` (which have a separate start()/
     * stop() to defer a device-preference change to), this engine opens
     * its device once, at construction, and keeps it open for its whole
     * lifetime (see the class docs) - so there's no later "start" to defer
     * to. Switching devices while `isPlaying()` is safe (JUCE closes and
     * reopens around the switch) but will produce a brief audible gap.
     *
     * @param deviceName The device to switch to, from
     *        availableOutputDeviceNames() - an empty string requests the
     *        system default device.
     * @return `true` if the switch succeeded. `false` leaves whichever
     *         device was open before this call unchanged (including if
     *         `deviceName` isn't a currently available device).
     *
     * @note A real JUCE quirk, confirmed while testing: an unrecognized
     *       `deviceName` is only actually rejected (returning `false`) once
     *       the device manager's device types have been populated at least
     *       once - calling availableOutputDeviceNames() first (as a real
     *       caller populating a device picker always would) guarantees
     *       that; calling this as the very first thing ever done on a
     *       brand-new engine does not, and would trivially "succeed"
     *       against a name that was never actually validated.
     */
    bool setPreferredOutputDevice(const std::string& deviceName);

    /// @brief The currently open output device's name.
    /// @return Empty if no device is open (isDeviceAvailable() is `false`).
    [[nodiscard]] std::string currentOutputDeviceName() const;

    /**
     * @brief Sets the output gain applied in renderBlock().
     *
     * Allowed above `1.0` - a real gain boost past unity, not just an
     * attenuator down to silence, per the confirmed scope for this
     * milestone. Clamped to `[0, kMaxVolume]`.
     *
     * @param volume The new gain - `1.0` is unchanged/unity.
     */
    void setVolume(float volume) noexcept;

    /// @brief The current output gain - see setVolume().
    /// @return The current gain, `1.0` meaning unity.
    [[nodiscard]] float volume() const noexcept;

    /// @brief The upper bound setVolume() clamps to - 200%, a real boost
    /// past unity but still a sane ceiling against accidental clipping/
    /// hearing damage from an unbounded slider.
    static constexpr float kMaxVolume = 2.0f;

    /**
     * @brief The current playback position, in samples into the loaded
     *        audio - for a live position bar/playhead indicator.
     * @return The same position renderBlock() is currently reading from;
     *         `0` for a fresh or stopped engine, and never past
     *         totalSamples().
     * @note Real-time-safe (a plain atomic load), safely callable from
     *       either thread - same as volume().
     */
    [[nodiscard]] std::size_t positionSamples() const noexcept;

    /// @brief The loaded audio's own length, in samples - the upper bound
    /// positionSamples() advances toward.
    /// @return `0` if nothing has been loaded yet.
    [[nodiscard]] std::size_t totalSamples() const noexcept;

    /// @brief The loaded audio's own sample rate, in Hz - divide
    /// positionSamples()/totalSamples() by this to get seconds.
    /// @return The sample rate loadAudio()'s argument carried; `44100`
    ///         (AudioBuffer's own default) if nothing has been loaded yet.
    [[nodiscard]] std::uint32_t sampleRateHz() const noexcept;

    /**
     * @brief Jumps playback to an arbitrary position - for a seekable
     *        position bar.
     *
     * Safe to call whether or not isPlaying() - a paused/stopped engine
     * just resumes from the new position on the next play().
     *
     * @param sampleIndex The new position, in samples; clamped to
     *        `[0, totalSamples()]`, then - if that clamped position is at
     *        or past the active setPlaybackRange() restriction's own end -
     *        resolved exactly the way reaching that end during playback
     *        would be: wrapped to the loop-back position, or (not looping)
     *        clamped to the range's own end instead, which also halts
     *        playback (isPlaying() becomes `false`) right there - the same
     *        outcome a seek dragged past a halting range's end should have.
     * @note Real-time-safe (a plain atomic store), safely callable from
     *       either thread - same as setVolume(). A concurrent renderBlock()
     *       call may still read the pre-seek position for that one block -
     *       a benign, momentary race, not a correctness issue.
     */
    void seek(std::size_t sampleIndex) noexcept;

    /**
     * @brief Restricts playback to `[0, rangeEndSamples)`, and chooses what
     *        happens once that end is reached - see the class docs'
     *        Decision #217 note.
     *
     * Both renderBlock() and seek() consult this immediately and entirely
     * within this class - no separate polling or later reaction is needed
     * (or should be built) to make a loop-back or an end-of-range halt
     * actually happen.
     *
     * @param loopEnabled `true` wraps back to `loopBackSamples` once
     *        `rangeEndSamples` is reached; `false` halts playback there
     *        instead (same as reaching the natural end of the loaded
     *        audio always has).
     * @param rangeEndSamples Where the restricted range ends. Clamped to
     *        `totalSamples()` at the point it's actually used - pass
     *        `totalSamples()` itself for "no narrower restriction than the
     *        whole loaded audio" (loadAudio()'s own default).
     * @param loopBackSamples Where a loop-back (`loopEnabled`) seeks to.
     *        Ignored when `!loopEnabled`. If this isn't strictly less than
     *        the (clamped) `rangeEndSamples`, looping is skipped in favor
     *        of halting instead - a defensive fallback against a
     *        degenerate/misconfigured range, not a case callers should
     *        rely on.
     * @note Real-time-safe (plain atomic stores), safely callable from
     *       either thread - same as setVolume()/seek().
     */
    void setPlaybackRange(bool loopEnabled, std::size_t rangeEndSamples, std::size_t loopBackSamples) noexcept;

private:
    /// @brief What position()/seek() should resolve to on reaching
    /// `rangeEnd` - either the loop-back position (`halted == false`) or
    /// `rangeEnd` itself (`halted == true`) - see setPlaybackRange()'s own
    /// docs on the degenerate-loop-back fallback. Shared by renderBlock()
    /// and seek() so the two can't disagree on the outcome.
    struct RangeCrossing {
        std::size_t position;
        bool halted;
    };
    [[nodiscard]] RangeCrossing resolveRangeCrossing(std::size_t rangeEnd) const noexcept;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager_;
    bool deviceAvailable_ = false;
    sound_mind::codec::AudioBuffer audio_;
    std::atomic<std::size_t> position_{0};
    std::atomic<bool> playing_{false};
    std::atomic<float> volume_{1.0f};
    std::atomic<bool> loopEnabled_{false};
    std::atomic<std::size_t> rangeEndSamples_{0};
    std::atomic<std::size_t> loopBackSamples_{0};
};

}  // namespace sound_mind::core
