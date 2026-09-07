#pragma once

#include <atomic>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::core {

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
 * @note Thread-safety: `loadAudio()`/`play()`/`pause()`/`stop()` are meant
 *       to be called from the UI thread only. `loadAudio()` must not be
 *       called while playing (`stop()` first) - swapping the buffer while
 *       the audio thread might be reading it isn't safe, and this first
 *       pass doesn't build the lock-free double-buffering that would allow
 *       it. `renderBlock()` is the one method actually called from the
 *       audio callback thread (indirectly, via JUCE) - it is real-time-safe
 *       (no allocation, no locking, only atomics and array indexing).
 */
class PlaybackEngine : private juce::AudioIODeviceCallback {
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    /**
     * @brief Loads the audio to play, replacing anything previously loaded.
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

private:
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
};

}  // namespace sound_mind::core
