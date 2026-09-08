#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/core/playback_engine.h"

namespace sound_mind::core {

/**
 * @brief One-shot capture from an input device into a plain audio buffer,
 *        per `docs/sound-mind-roadmap.md`'s Record milestone (`v0.Y.8.1`).
 *
 * Deliberately much simpler than `LiveEngine`: Record doesn't stream
 * anything back out, doesn't need a continuously-updating encoded image
 * during capture, and per `docs/sound-mind-design.md`'s Record section -
 * "the audio is encoded into the layer exactly as any other imported audio
 * would be" - its result is meant to go through the *same* whole-buffer
 * `sound_mind::codec::encode()` any import already uses, not
 * `StreamIncrementalEncoder`'s slightly different (never-zero-padded)
 * framing. So this class only owns the capture half - accumulating raw
 * samples - and hands the caller a plain `AudioBuffer` to encode however
 * an imported one would be (see `sound-mind-studio`'s `MainWindow` for
 * that half, mirroring `importAudioFile()`).
 *
 * Per the confirmed precedent from Playback (`v0.0.4.1`) and Live Mode
 * (`v0.0.7.1`), both of which deferred a real input/output device picker:
 * this first pass also defers the design doc's "choose the input device
 * (with rescan)" and "set an input gain" as UI affordances layered on top
 * of a working capture pipeline, rather than building them unasked.
 *
 * Architecture: the audio callback thread only ever copies fixed-size
 * blocks into a lock-free ring buffer (`juce::AbstractFifo`, same as
 * `LiveEngine`) - no allocation, no locking. Unlike `LiveEngine`, draining
 * that ring doesn't need a dedicated background thread: there's no
 * per-block encoding work happening during capture, just moving samples
 * into a growing buffer, cheap enough to do straight from a UI-thread
 * timer's periodic drainAvailable() call (see `sound-mind-studio`'s
 * `MainWindow`).
 *
 * @note Thread-safety: `start()`/`stop()`/`drainAvailable()`/
 *       `capturedAudio()` are meant to be called from the UI thread only.
 *       `processBlock()` is real-time-safe and is the one method actually
 *       called from the audio callback thread (indirectly, via JUCE).
 */
class RecordEngine : private juce::AudioIODeviceCallback {
public:
    /// @brief Constructs a RecordEngine. Capture doesn't start until
    ///        start() is called.
    /// @param sampleRateHz The sample rate the resulting AudioBuffer is
    ///        tagged with - the actual input device is trusted to run at
    ///        this rate (matching PlaybackEngine/LiveEngine's own
    ///        simplification: neither validates a real device's actual
    ///        negotiated rate against what its caller expects either).
    /// @param deviceMode `Real` (the default) attaches to the system's
    ///        real input device on start(), falling back gracefully
    ///        (isDeviceAvailable() == false) if none exists; `None` never
    ///        attaches to one at all - for hermetic tests that drive
    ///        processBlock() directly instead (same reasoning as
    ///        PlaybackEngine's AudioDeviceMode).
    explicit RecordEngine(std::uint32_t sampleRateHz, AudioDeviceMode deviceMode = AudioDeviceMode::Real);
    ~RecordEngine() override;

    RecordEngine(const RecordEngine&) = delete;
    RecordEngine& operator=(const RecordEngine&) = delete;

    /// @brief Opens the real input device (if constructed with
    ///        `AudioDeviceMode::Real`) and starts capturing - clears any
    ///        previously captured audio first. Does nothing if already
    ///        recording.
    void start();

    /// @brief Stops capturing (drains any final samples still in the ring
    ///        buffer first) and closes the device, if one was open. Does
    ///        nothing if not recording.
    void stop();

    /// @brief Whether capture is currently active.
    /// @return `true` between a start() and the next stop().
    [[nodiscard]] bool isRecording() const noexcept;

    /**
     * @brief Whether a real input device was actually opened.
     *
     * `false` means start() still flips isRecording() to `true`, but
     * nothing will actually be captured - a defensive fallback for
     * environments with no audio hardware (CI runners, in particular),
     * matching PlaybackEngine's identical accessor.
     *
     * @return `true` if a real input device is open.
     */
    [[nodiscard]] bool isDeviceAvailable() const noexcept;

    /**
     * @brief Moves whatever's currently in the capture ring buffer into
     *        the accumulated recording.
     *
     * Call this periodically (e.g. from a UI-thread timer) while
     * recording, so the ring buffer never fills up and starts dropping
     * samples - see the class docs for why this doesn't need its own
     * background thread the way `LiveEngine`'s equivalent drain does.
     *
     * @note Not real-time-safe (allocates, as the captured buffer grows) -
     *       call from the UI thread, never an audio callback.
     */
    void drainAvailable();

    /// @brief The audio captured so far (or in total, once stopped) -
    ///        reflects only what the last drainAvailable() call moved out
    ///        of the ring buffer.
    /// @return The accumulated capture, tagged with the sample rate given
    ///         at construction.
    [[nodiscard]] const sound_mind::codec::AudioBuffer& capturedAudio() const noexcept;

    /**
     * @brief The engine's real-time-safe capture callback: copies input
     *        into the capture ring buffer.
     *
     * Exposed separately from the `AudioIODeviceCallback` interface it's
     * normally invoked through, specifically so it's testable without a
     * real device - same reasoning as `PlaybackEngine::renderBlock()`.
     *
     * @param inputChannelData Pointers to each input channel's captured
     *        samples, each `numSamples` long. Channel 0 is treated as
     *        left; channel 1 (if present) as right; a mono (single-
     *        channel) device's one channel is used for both.
     * @param numInputChannels Number of channels in `inputChannelData`.
     * @param numSamples Number of samples to process in each channel.
     *
     * @note Real-time-safe: no allocation, no locking, no exceptions.
     */
    void processBlock(const float* const* inputChannelData, int numInputChannels, int numSamples) noexcept;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    /// @brief Ring buffer capacity, in samples per channel - only needs to
    /// bridge the gap between successive drainAvailable() calls (a UI
    /// timer tick, typically tens of milliseconds), not a whole recording.
    static constexpr int kRingCapacity = 1 << 14;

    juce::AudioDeviceManager deviceManager_;
    bool deviceAvailable_ = false;
    AudioDeviceMode deviceMode_;
    std::uint32_t sampleRateHz_;

    juce::AbstractFifo captureFifo_{kRingCapacity};
    std::vector<float> captureRingLeft_;
    std::vector<float> captureRingRight_;

    sound_mind::codec::AudioBuffer capturedAudio_;
    std::atomic<bool> recording_{false};
};

}  // namespace sound_mind::core
