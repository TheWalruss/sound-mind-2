#pragma once

#include <atomic>
#include <string>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/core/playback_engine.h"

namespace sound_mind::core {

/**
 * @brief Plays a brief, fixed test tone through a chosen output device -
 *        the Configure Devices panel's own "test an output device"
 *        affordance, per `docs/sound-mind-roadmap.md`'s `v0.0.42.1`
 *        milestone.
 *
 * Deliberately independent of `PlaybackEngine`: testing a device is a
 * device-*configuration*-time action, not tied to (and never interfering
 * with) whatever a project's own `PlaybackEngine` is doing at the time -
 * layering a "play a tone instead" mode onto that engine's own real render
 * path would risk exactly that entanglement. This class owns its own
 * `juce::AudioDeviceManager`, opened only for as long as a tone is actually
 * playing.
 *
 * How long the tone plays is the caller's own job (`sound-mind-studio`'s
 * `MainWindow`/`ConfigureDevicesController`, typically via a short one-shot
 * UI timer calling stop() after ~1 second) - this class itself has no
 * notion of duration, only "playing" or not.
 *
 * @note Thread-safety: `start()`/`stop()`/`isPlaying()`/`isDeviceAvailable()`
 *       are meant to be called from the UI thread only. `processBlock()` is
 *       real-time-safe and is the one method actually called from the
 *       audio callback thread (indirectly, via JUCE) - exposed separately
 *       so it's testable without a real device, same reasoning as every
 *       other engine's own equivalent.
 */
class DeviceTestTonePlayer : private juce::AudioIODeviceCallback {
public:
    /// @brief Constructs a player. No tone plays until start() is called.
    /// @param deviceMode `Real` (the default) attaches to a real output
    ///        device on start(), falling back gracefully
    ///        (isDeviceAvailable() == false) if none exists; `None` never
    ///        attaches to one at all - for hermetic tests that drive
    ///        processBlock() directly instead, same reasoning as every
    ///        other engine's own identical `AudioDeviceMode` parameter.
    explicit DeviceTestTonePlayer(AudioDeviceMode deviceMode = AudioDeviceMode::Real);
    ~DeviceTestTonePlayer() override;

    DeviceTestTonePlayer(const DeviceTestTonePlayer&) = delete;
    DeviceTestTonePlayer& operator=(const DeviceTestTonePlayer&) = delete;

    /**
     * @brief Opens `outputDeviceName` and starts playing a continuous test
     *        tone - stops any tone already playing first, and resets the
     *        tone's own phase, so consecutive test plays always sound
     *        identical rather than picking up wherever the last one left
     *        off.
     * @param outputDeviceName The device to open; an empty string requests
     *        the system default device.
     */
    void start(const std::string& outputDeviceName);

    /// @brief Stops playing and closes the device (if one was open). Does
    ///        nothing if not currently playing.
    void stop();

    /// @brief Whether a tone is currently playing.
    /// @return `true` between a start() and the next stop().
    [[nodiscard]] bool isPlaying() const noexcept;

    /// @brief Whether a real output device was actually opened - see
    ///        every other engine's own identical accessor for why this can
    ///        be `false` while isPlaying() is `true` (no audio hardware
    ///        available, e.g. in CI).
    /// @return `true` if a real output device is open.
    [[nodiscard]] bool isDeviceAvailable() const noexcept;

    /// @brief The test tone's own frequency, in Hz - a plain, recognizable
    ///        reference pitch (concert A), not user-configurable.
    static constexpr float kToneFrequencyHz = 440.0f;

    /// @brief The test tone's own peak amplitude - deliberately moderate
    ///        (not full-scale), so a "test" doesn't risk being startling or
    ///        clipping a downstream device.
    static constexpr float kToneAmplitude = 0.25f;

    /// @brief The sample rate the tone's own phase increment is computed
    ///        against - trusted to match whatever real device actually
    ///        opens, the same simplification every other engine in this
    ///        codebase already makes (none validates a real device's own
    ///        negotiated rate against what it expects).
    static constexpr double kSampleRateHz = 44100.0;

    /**
     * @brief The real-time-safe render callback: fills every output
     *        channel with the current test tone (identical on every
     *        channel), or silence while not playing.
     *
     * @param outputChannelData Pointers to each output channel's sample
     *        array to fill, each `numSamples` long.
     * @param numOutputChannels Number of channels in `outputChannelData`.
     * @param numSamples Number of samples to process in each channel.
     *
     * @note Real-time-safe: no allocation, no locking, no exceptions.
     */
    void processBlock(float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager_;
    AudioDeviceMode deviceMode_;
    bool deviceAvailable_ = false;
    std::atomic<bool> playing_{false};

    /// @brief Audio-thread-owned (only ever touched while a tone is
    /// playing, so never concurrently with start()'s own reset, which only
    /// happens after stop() has already closed the device).
    double phase_ = 0.0;
};

}  // namespace sound_mind::core
