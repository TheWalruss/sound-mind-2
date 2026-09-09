#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/playback_engine.h"

namespace sound_mind::core {

/**
 * @brief A fixed-length loop pedal: continuously captures live audio input,
 *        one loop-length at a time, encoding/decoding each completed loop
 *        into the Stream codec and playing the previous loop's result back
 *        during the next one - per `docs/sound-mind-roadmap.md`'s Loop Mode
 *        milestone (`v0.Y.12.1`, renamed from Live Mode).
 *
 * Renamed and reimplemented from the original `LiveEngine`'s continuous,
 * always-decoding-the-whole-history model: per the milestone's confirmed
 * scope, this is the fixed-length loop pedal the legacy Studio's own "Live
 * Mode" actually was, not genuinely continuous streaming. `loopLengthSamples`
 * (derived from the project's own duration - see `docs/sound-mind-roadmap.md`
 * - not user-adjustable) is fixed for the engine's whole lifetime; each
 * completed loop of raw input is handed to the background worker thread as
 * one bounded buffer, encoded and decoded via the same whole-buffer
 * `sound_mind::codec::encode()`/`decode()` any import or Recording already
 * uses (not `StreamIncrementalEncoder` - there is no continuously-growing
 * history to maintain incrementally here, unlike the original Live Mode).
 *
 * **No compositing with the rest of the project** - the confirmed reduced
 * scope for this milestone: real multi-layer audio mixing still doesn't
 * exist anywhere in the codebase (see the roadmap's own note on why this
 * removed a dependency `v0.0.7.1`'s notes originally expected).
 *
 * Architecture: the audio callback thread only ever copies fixed-size
 * blocks into a lock-free ring buffer (`juce::AbstractFifo`, same primitive
 * `LiveEngine`/`RecordEngine` already use) on the capture side, and reads
 * sequentially from one of two pre-allocated, fixed-size (`loopLengthSamples`
 * each) playback buffers on the output side - no allocation, no locking on
 * that thread, satisfying `CLAUDE.md`'s real-time constraint. All the real
 * (allocating) work - draining the capture ring, slicing off one loop's
 * worth of raw samples, `sound_mind::codec::encode()`/`decode()` - runs on a
 * background worker thread.
 *
 * **The "next loop" hand-off, and loop delay:** the background worker
 * publishes a newly-decoded loop's audio by flipping which of the two
 * playback buffers is "active" (`activeSlot_`); the audio thread only reads
 * that flag *once per loop boundary it crosses itself* (never mid-loop, so a
 * newly-published loop never splices in with an audible jump) - see
 * processBlock()'s own comments.
 *
 * @note **Real, structural minimum latency, confirmed acceptable rather
 *       than engineered away:** a loop's audio can't even begin encoding
 *       until its capture finishes, and the playback cursor only checks for
 *       a newly-published result once per loop it itself plays through - so
 *       the earliest a captured loop's result can become the *active* slot
 *       is partway through the very next playback cycle, and the earliest
 *       the audio thread actually *starts reading* that slot is the cycle
 *       after that. In practice (encode/decode of one loop takes a small
 *       fraction of the wall-clock time it took to capture it - the normal
 *       case, and the only one this first pass targets), that means: what
 *       you captured as "loop N" is first heard during playback loop
 *       `N + 2`, not `N + 1` - two loop-lengths of latency, not one, even
 *       when the worker is keeping up perfectly (loopsBehind() == 0). This
 *       is a direct, unavoidable consequence of never splicing a result in
 *       mid-loop (see above) combined with a full encode/decode round trip
 *       needing to happen somewhere in between - a true "hear it the very
 *       next loop" design would need to deliberately run capture and
 *       playback out of phase with each other (playback's cycle boundary
 *       arriving some fixed margin after capture's), which is future work
 *       if this baseline latency turns out to matter in practice, not part
 *       of this milestone's confirmed scope. loopsBehind() tracks a
 *       different, additive thing on top of this fixed baseline: whether
 *       the worker has *also* fallen further behind than that (a slower-
 *       than-real-time pipeline falls behind by whole loop iterations, not
 *       a fixed latency, per the roadmap's own framing) - `0` means only
 *       the fixed two-loop baseline applies, not that there is no latency
 *       at all.
 *
 * **"Keep looping":** while setKeepLooping(true), newly-captured input is
 * discarded rather than handed to the worker at all (see processBlock()) -
 * the currently-active playback buffer just keeps replaying unchanged, loop
 * after loop, instead of being recorded over. The default, `false`, matches
 * the legacy behavior: record over the previous take every loop.
 *
 * @note Thread-safety: `start()`/`stop()`/`setKeepLooping()`/`keepLooping()`
 *       are meant to be called from the UI thread only. `processBlock()` is
 *       real-time-safe and is the one method actually called from the audio
 *       callback thread (indirectly, via JUCE). `processPendingAudio()` is
 *       the worker thread's own loop body, exposed separately (like
 *       `LiveEngine`'s equivalent was) so it's directly, deterministically
 *       testable without depending on real thread timing. `currentImage()`,
 *       `loopsCaptured()`, and `loopsBehind()` are safe to call from any
 *       thread.
 */
class LoopEngine : private juce::AudioIODeviceCallback, private juce::Thread {
public:
    /// @brief Constructs a LoopEngine. Capture doesn't start until start()
    ///        is called.
    /// @param config Stream codec parameters for each encoded loop -
    ///        `config.sampleRateHz` should match whatever the audio device
    ///        will actually be opened at (see audioDeviceAboutToStart()).
    /// @param loopLengthSamples The fixed length of one loop, in samples per
    ///        channel at `config.sampleRateHz` - derived from the project's
    ///        own duration, per the class docs. Treated as at least 1 (a
    ///        value of 0 would make loop-boundary arithmetic undefined) -
    ///        callers should never actually pass 0 in practice.
    /// @param deviceMode `Real` (the default) attaches to the system's
    ///        real input+output device on start(), falling back gracefully
    ///        (isDeviceAvailable() == false) if none exists; `None` never
    ///        attaches to one at all - for hermetic tests that drive
    ///        processBlock()/processPendingAudio() directly instead (same
    ///        reasoning as PlaybackEngine's AudioDeviceMode).
    explicit LoopEngine(sound_mind::codec::StreamCodecConfig config, std::size_t loopLengthSamples,
                         AudioDeviceMode deviceMode = AudioDeviceMode::Real);
    ~LoopEngine() override;

    LoopEngine(const LoopEngine&) = delete;
    LoopEngine& operator=(const LoopEngine&) = delete;

    /// @brief Opens the real input+output device (if constructed with
    ///        `AudioDeviceMode::Real`) and starts the background encode/
    ///        decode worker thread. Resets all loop-cycle state (capture
    ///        position, playback position, loopsCaptured()/loopsBehind(),
    ///        currentImage()) as if newly constructed. Does nothing if
    ///        already running.
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
     * matching LiveEngine's/PlaybackEngine's identical accessor.
     *
     * @return `true` if a real input+output device is open.
     */
    [[nodiscard]] bool isDeviceAvailable() const noexcept;

    /**
     * @brief Whether subsequent loops should replay the last successfully
     *        captured take unchanged instead of recording over it.
     * @param keepLooping The new state - see the class docs' "Keep looping"
     *        note.
     */
    void setKeepLooping(bool keepLooping) noexcept;

    /// @brief The current "Keep looping" state - see setKeepLooping().
    /// @return `true` if newly captured input is currently being discarded.
    [[nodiscard]] bool keepLooping() const noexcept;

    /// @brief The most recently completed loop's encoded Stream image.
    /// @return A default-constructed (`frameCount == 0`) StreamImage until
    ///         at least one whole loop has been captured and processed.
    [[nodiscard]] sound_mind::codec::StreamImage currentImage() const;

    /// @brief How many whole loops of input have been captured since
    ///        start() - advances continuously while running (unless
    ///        keepLooping() is `true`, which freezes it - see
    ///        setKeepLooping()'s docs), independent of whether the
    ///        background worker has actually finished processing each one.
    /// @return The count of whole loops captured so far.
    [[nodiscard]] std::uint64_t loopsCaptured() const noexcept;

    /**
     * @brief How many whole loops the background worker's published result
     *        currently lags loopsCaptured() by.
     *
     * `0` means the worker has kept up - the just-completed loop's decoded
     * result is (or, if still in flight, is about to become) what's
     * playing. A nonzero value means the worker hasn't finished encoding/
     * decoding that many loops yet, so playback is still repeating an
     * older result - see the class docs' "loop delay" note.
     *
     * @return `loopsCaptured() - <loops the worker has finished>`, never
     *         negative.
     */
    [[nodiscard]] std::uint64_t loopsBehind() const noexcept;

    /**
     * @brief The engine's real-time-safe per-block audio processing: copies
     *        captured input into the capture ring buffer (unless
     *        keepLooping() is `true` - see setKeepLooping()'s docs), and
     *        fills `outputChannelData` by reading sequentially through
     *        whichever playback buffer is active for the loop currently
     *        playing, wrapping - and re-checking which buffer is active -
     *        only at that loop's own boundary (see the class docs' "next
     *        loop" hand-off note).
     *
     * Exposed separately from the `AudioIODeviceCallback` interface it's
     * normally invoked through, specifically so it's testable without a
     * real device - same reasoning as `LiveEngine::processBlock()`.
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
     *        appending it to a growing pending-loop scratch buffer, and -
     *        for every whole loopLengthSamples() now accumulated - slices
     *        one off, encodes/decodes it, and publishes the result (see
     *        the class docs' "next loop" hand-off note).
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

    /// @brief Capture ring buffer capacity, in samples per channel -
    /// generous relative to typical device buffer sizes and the worker
    /// thread's poll interval (see run()), so normal operation never
    /// approaches it. Independent of loopLengthSamples_ (which is usually
    /// much larger) - this only needs to bridge one worker poll interval,
    /// same reasoning as LiveEngine's identical constant.
    static constexpr int kRingCapacity = 1 << 16;

    juce::AudioDeviceManager deviceManager_;
    bool deviceAvailable_ = false;
    AudioDeviceMode deviceMode_;
    sound_mind::codec::StreamCodecConfig config_;
    std::size_t loopLengthSamples_;

    juce::AbstractFifo captureFifo_{kRingCapacity};
    std::vector<float> captureRingLeft_;
    std::vector<float> captureRingRight_;

    /// @brief Worker-thread-only accumulation of drained-but-not-yet-a-
    /// whole-loop capture - never touched by the audio thread.
    std::vector<float> pendingLoopLeft_;
    std::vector<float> pendingLoopRight_;

    /// @brief Two fixed-size (loopLengthSamples_ each), pre-allocated
    /// playback buffers - the worker always writes a newly-decoded loop
    /// into whichever one activeSlot_ does *not* currently name, then
    /// publishes it by flipping activeSlot_ - see the class docs.
    std::array<std::vector<float>, 2> playbackLeft_;
    std::array<std::vector<float>, 2> playbackRight_;
    std::atomic<int> activeSlot_{0};

    /// @brief Audio-thread-only playback cursor state - never touched by
    /// the worker thread. readingSlot_ is only re-read from activeSlot_ at
    /// a loop boundary the audio thread itself crosses (see processBlock()).
    int readingSlot_ = 0;
    std::size_t playPos_ = 0;

    std::atomic<std::uint64_t> capturedSamplesTotal_{0};
    std::atomic<std::uint64_t> loopsDecoded_{0};
    std::atomic<bool> keepLooping_{false};
    std::atomic<bool> running_{false};

    mutable std::mutex imageMutex_;
    sound_mind::codec::StreamImage currentImage_;
};

}  // namespace sound_mind::core
