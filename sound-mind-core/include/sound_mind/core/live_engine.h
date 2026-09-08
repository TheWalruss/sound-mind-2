#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/codec/stream_incremental_encoder.h"
#include "sound_mind/core/playback_engine.h"

namespace sound_mind::core {

/**
 * @brief Continuously captures live audio input, encodes it into a growing
 *        Stream image, and streams the decoded result back out - the first,
 *        "plain" real-time pipeline per `docs/sound-mind-roadmap.md`'s Live
 *        Mode milestone (`v0.Y.7.1`).
 *
 * Per the confirmed scope for this milestone: the output is the live input
 * alone, round-tripped through the Stream codec (encode, then decode the
 * growing result) - not composited with any other layer or project content
 * yet (real multi-layer audio mixing doesn't exist anywhere in the codebase
 * yet, not even for Playback - see PlaybackEngine's own docs). "Genuinely
 * continuous," per the design doc's Live section, refers to this encode-
 * then-decode round trip actually running continuously on live audio,
 * rather than to compositing with a wider project - that's future work,
 * once real multi-layer mixing exists.
 *
 * Architecture: the audio callback thread only ever copies fixed-size
 * blocks into/out of two lock-free ring buffers (`juce::AbstractFifo`,
 * the same primitive `docs/sound-mind-architecture.md`'s GPU/Audio-Thread
 * Handoff section names as the standard tool for exactly this) - no
 * allocation, no locking, satisfying `CLAUDE.md`'s real-time constraint.
 * All the real (allocating) work - `sound_mind::codec::
 * StreamIncrementalEncoder::pushSamples()` and `sound_mind::codec::
 * decode()` - runs on a background worker thread that drains the capture
 * ring and refills the playback ring.
 *
 * Because `decode()`'s overlap-add reconstruction only fully resolves a
 * given output sample once every analysis frame that overlaps it has been
 * encoded (see decode()'s own STFT structure), the worker only ever
 * publishes the *stable* prefix of each re-decode - the last `fftSize -
 * hopLength` samples of any given decode are still subject to change as
 * more audio arrives, and are recomputed (correctly) on the next pass
 * rather than published early. This bounds the round-trip latency to
 * roughly one analysis window (~40 ms at the default config) - a real,
 * expected STFT-processing latency, not a bug.
 *
 * @note **Known limitation of this first pass**: the worker thread
 *       re-decodes the *entire* accumulated history on every pass (there is
 *       no incremental/streaming decode yet - see stream_incremental_
 *       encoder.h's docs for why encoding could be made incremental but
 *       decoding, for now, just reuses the existing whole-buffer decode()).
 *       Cost grows with how long a Live session has been running; fine for
 *       the durations this milestone's demo needs, but a real problem for
 *       an extended session. Revisit with an actual incremental decoder if
 *       that turns out to matter in practice.
 *
 * @note Thread-safety: `start()`/`stop()` are meant to be called from the
 *       UI thread only. `processBlock()` is real-time-safe and is the one
 *       method actually called from the audio callback thread (indirectly,
 *       via JUCE). `processPendingAudio()` is the worker thread's own loop
 *       body, exposed separately (like `processBlock()`) so both can be
 *       driven directly and deterministically by a test, without depending
 *       on real device or thread timing. `currentImage()` is safe to call
 *       from any thread (StreamIncrementalEncoder is internally
 *       synchronized).
 */
class LiveEngine : private juce::AudioIODeviceCallback, private juce::Thread {
public:
    /// @brief Constructs a LiveEngine. Capture doesn't start until start()
    ///        is called.
    /// @param config Stream codec parameters for the encoded live layer -
    ///        `config.sampleRateHz` should match whatever the audio device
    ///        will actually be opened at (see audioDeviceAboutToStart()).
    /// @param deviceMode `Real` (the default) attaches to the system's
    ///        real input+output device on start(), falling back gracefully
    ///        (isDeviceAvailable() == false) if none exists; `None` never
    ///        attaches to one at all - for hermetic tests that drive
    ///        processBlock()/processPendingAudio() directly instead (same
    ///        reasoning as PlaybackEngine's AudioDeviceMode).
    explicit LiveEngine(sound_mind::codec::StreamCodecConfig config, AudioDeviceMode deviceMode = AudioDeviceMode::Real);
    ~LiveEngine() override;

    LiveEngine(const LiveEngine&) = delete;
    LiveEngine& operator=(const LiveEngine&) = delete;

    /// @brief Opens the real input+output device (if constructed with
    ///        `AudioDeviceMode::Real`) and starts the background encode/
    ///        decode worker thread. Does nothing if already running.
    void start();

    /// @brief Stops the worker thread and closes the device (if one was
    ///        open). Does nothing if not running.
    void stop();

    /// @brief Whether capture is currently active.
    /// @return `true` between a start() and the next stop().
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Whether a real input+output device was actually opened.
     *
     * `false` means start() still flips isRunning() to `true`, but nothing
     * will actually be captured or heard - a defensive fallback for
     * environments with no audio hardware (CI runners, in particular),
     * matching PlaybackEngine's identical accessor.
     *
     * @return `true` if a real input+output device is open.
     */
    [[nodiscard]] bool isDeviceAvailable() const noexcept;

    /// @brief A snapshot of everything encoded so far.
    /// @return An independent copy, safe to render or decode without
    ///         racing the worker thread's own pushSamples() calls.
    [[nodiscard]] sound_mind::codec::StreamImage currentImage() const;

    /**
     * @brief The engine's real-time-safe per-block audio processing:
     *        copies captured input into the capture ring buffer, and fills
     *        `outputChannelData` from whatever decoded audio is currently
     *        available in the playback ring buffer (silencing any
     *        shortfall - an under-run, not an error, if the worker thread
     *        hasn't kept up).
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
     * @param outputChannelData Pointers to each output channel's sample
     *        array to fill, each `numSamples` long.
     * @param numOutputChannels Number of channels in `outputChannelData`;
     *        any beyond the first two are silenced.
     * @param numSamples Number of samples to process in each channel.
     *
     * @note Real-time-safe: no allocation, no locking, no exceptions.
     */
    void processBlock(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                       int numOutputChannels, int numSamples) noexcept;

    /**
     * @brief Drains whatever's currently in the capture ring buffer,
     *        encodes it, and publishes whatever newly-stable decoded audio
     *        that makes available to the playback ring buffer.
     *
     * This is the background worker thread's own per-iteration work (see
     * run()), exposed separately so a test can drive it directly and
     * synchronously, without depending on real thread timing.
     *
     * @note Not real-time-safe (allocates - drives the codec's encode()/
     *       decode() machinery). Must not be called concurrently with
     *       itself from more than one thread.
     */
    void processPendingAudio();

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void run() override;

    /// @brief Ring buffer capacity, in samples per channel - generous
    /// relative to typical device buffer sizes and the worker thread's
    /// poll interval (see run()), so normal operation never approaches it.
    static constexpr int kRingCapacity = 1 << 16;

    juce::AudioDeviceManager deviceManager_;
    bool deviceAvailable_ = false;
    AudioDeviceMode deviceMode_;
    sound_mind::codec::StreamCodecConfig config_;
    sound_mind::codec::StreamIncrementalEncoder encoder_;

    juce::AbstractFifo captureFifo_{kRingCapacity};
    std::vector<float> captureRingLeft_;
    std::vector<float> captureRingRight_;

    juce::AbstractFifo playbackFifo_{kRingCapacity};
    std::vector<float> playbackRingLeft_;
    std::vector<float> playbackRingRight_;

    /// @brief How many samples of decode() output have already been
    /// published to playbackFifo_ - see the class docs' "stable prefix"
    /// explanation for why this only ever advances up to a re-decode's
    /// stable region, not its full (still-subject-to-change) tail.
    std::size_t stableSamplesEmitted_ = 0;

    std::atomic<bool> running_{false};
};

}  // namespace sound_mind::core
