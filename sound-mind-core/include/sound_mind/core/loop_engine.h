#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/stream_incremental_encoder.h"
#include "sound_mind/core/master_limiter.h"
#include "sound_mind/core/playback_engine.h"

namespace sound_mind::core {

/**
 * @brief Loop Mode: plays the whole project over and over while recording
 *        a new take into the Loop layer each pass - `docs/sound-mind-
 *        design.md`'s "Loop Mode" (rewritten in `v0.1.35.1` per the user's
 *        own spec, `docs/sound-mind-roadmap.md`'s Pre-Beta Plan `v0.Y.75.1`).
 *
 * **What's heard each pass:** the project's own audio (`setBackingAudio()`
 * - the caller composites every layer *except* the Loop layer) plus the
 * previous pass's take, scaled by setTakeGains() (the Loop layer's own
 * gain and balance). Mixing the take in here, in the audio callback,
 * rather than re-compositing the whole project at every pass boundary, is
 * what lets a fresh take be heard the instant the loop restarts - a full
 * composite takes far longer than zero samples. Consequence: while
 * looping live, filters and blend modes stacked *above* the Loop layer
 * don't apply to the take; once Loop Mode stops, ordinary playback
 * composites the Loop layer like any other.
 *
 * **What's recorded:** input (scaled by setInputGain()) is written into a
 * second take buffer at the same position the output is playing, so a take
 * lines up with the project sample for sample. At the end of each pass the
 * two take buffers swap: the take just recorded is what plays next, and
 * the previous one is overwritten by the next pass's recording.
 *
 * **Freeze** (setFrozen()): recording stops immediately - the partial take
 * in progress is discarded, never swapped in - and the current take keeps
 * playing every pass. Unfreezing starts recording again at the beginning
 * of the next pass, never mid-pass, so a take is always one whole pass.
 *
 * **Project audio changes** (setBackingAudio() while running) take effect
 * at the next pass start, never mid-pass - the same staged, try-lock
 * hand-off `PlaybackEngine::stageAudio()` uses.
 *
 * **The background worker** (run(), or processPendingAudio() called
 * directly in tests) encodes each finished take into currentImage() - the
 * Loop layer's new content - and feeds currentPreviewImage() from the take
 * in progress, for the live-encoding view.
 *
 * @note Thread-safety: start()/stop()/setters are for the UI thread.
 *       processBlock() is the audio callback - real-time-safe: no
 *       allocation, no blocking locks (only a try-lock), no exceptions.
 *       processPendingAudio() is the worker's body and must not run
 *       concurrently with itself. The getters are safe from any thread.
 */
class LoopEngine : private juce::AudioIODeviceCallback, private juce::Thread {
public:
    /// @brief Constructs an idle engine; nothing runs until start().
    /// @param config Stream codec parameters for encoding takes.
    /// @param loopLengthSamples One pass - the project's own duration - in
    ///        samples per channel; treated as at least 1.
    /// @param deviceMode `Real` opens the system's input+output device on
    ///        start(); `None` never does (for tests that call
    ///        processBlock()/processPendingAudio() directly).
    explicit LoopEngine(sound_mind::codec::StreamCodecConfig config, std::size_t loopLengthSamples,
                         AudioDeviceMode deviceMode = AudioDeviceMode::Real);
    ~LoopEngine() override;

    LoopEngine(const LoopEngine&) = delete;
    LoopEngine& operator=(const LoopEngine&) = delete;

    /// @brief Resets to the start of a fresh session (position 0, both
    ///        takes silent, no takes completed, no images) - keeping the
    ///        current project audio - opens the device (in `Real` mode)
    ///        and starts the worker. Does nothing if already running.
    void start();

    /// @brief Stops the worker and closes the device. Does nothing if not
    ///        running.
    void stop();

    /// @return `true` between start() and stop().
    [[nodiscard]] bool isRunning() const noexcept;

    /// @return `true` if start() opened a real input+output device.
    [[nodiscard]] bool isDeviceAvailable() const noexcept;

    /// @return Why the last start() couldn't open a device; empty otherwise.
    [[nodiscard]] const std::string& deviceProblem() const noexcept;

    /// @return The available input device names.
    [[nodiscard]] std::vector<std::string> availableInputDeviceNames();

    /// @return The available output device names.
    [[nodiscard]] std::vector<std::string> availableOutputDeviceNames();

    /// @brief Sets the input device the next start() opens (empty = the
    ///        system default).
    /// @param deviceName The device name.
    void setPreferredInputDevice(std::string deviceName);

    /// @return The input device the next start() prefers.
    [[nodiscard]] const std::string& preferredInputDevice() const noexcept;

    /// @brief Sets the output device the next start() opens (empty = the
    ///        system default).
    /// @param deviceName The device name.
    void setPreferredOutputDevice(std::string deviceName);

    /// @return The output device the next start() prefers.
    [[nodiscard]] const std::string& preferredOutputDevice() const noexcept;

    /// @brief Sets the gain applied to captured input, clamped to
    ///        [0, kMaxGain].
    /// @param gain `1` is unity.
    void setInputGain(float gain) noexcept;

    /// @return The current input gain.
    [[nodiscard]] float inputGain() const noexcept;

    /// @brief setInputGain()'s upper bound.
    static constexpr float kMaxGain = 2.0f;

    /// @return The last processed block's peak input level, post-gain.
    [[nodiscard]] float currentInputLevel() const noexcept;

    /**
     * @brief Sets the project's own audio - everything but the Loop layer -
     *        played each pass. Takes effect immediately while idle, or at
     *        the next pass start while running.
     * @param audio Exactly loopLengthSamples() long, at the engine's own
     *        sample rate.
     * @return `false` (nothing changes) if the length or rate doesn't match.
     * @note UI thread only; allocation happens here, never on the audio
     *       thread.
     */
    bool setBackingAudio(sound_mind::codec::AudioBuffer&& audio);

    /**
     * @brief Turns the master limiter on the loop's output on or off
     *        (`v0.1.40.6`) - the same zero-latency `MasterLimiter` playback
     *        uses, as the last stage of processBlock(). It shapes only what's
     *        heard: captured takes are recorded unlimited. On by default.
     * @param enabled Whether to limit.
     * @note Real-time safe (atomic), callable from any thread.
     */
    void setLimiterEnabled(bool enabled) noexcept { limiter_.setEnabled(enabled); }

    /// @brief Whether the output limiter is on.
    /// @return The flag.
    [[nodiscard]] bool limiterEnabled() const noexcept { return limiter_.enabled(); }

    /// @brief Sets the output limiter's ceiling - see `MasterLimiter::setCeilingDb()`.
    /// @param ceilingDb The ceiling, dBFS.
    /// @note Real-time safe (atomic), callable from any thread.
    void setLimiterCeilingDb(float ceilingDb) noexcept { limiter_.setCeilingDb(ceilingDb); }

    /// @brief Sets the take's own playback gains - the Loop layer's gain
    ///        and balance, already combined per channel by the caller.
    ///        Applies from the next audio block.
    /// @param left Left-channel gain.
    /// @param right Right-channel gain.
    void setTakeGains(float left, float right) noexcept;

    /// @brief Freezes or unfreezes the loop - see the class docs.
    /// @param frozen `true` to stop recording and keep the current take.
    void setFrozen(bool frozen) noexcept;

    /// @return Whether the loop is frozen.
    [[nodiscard]] bool frozen() const noexcept;

    /// @return One pass, in samples per channel.
    [[nodiscard]] std::size_t loopLengthSamples() const noexcept { return loopLengthSamples_; }

    /// @return Where the output is within the current pass, in samples -
    ///         for the playhead.
    [[nodiscard]] std::size_t playbackPositionSamples() const noexcept;

    /// @return How many whole takes have been recorded (and swapped in)
    ///         since start() - frozen passes don't count.
    [[nodiscard]] std::uint64_t takesCompleted() const noexcept;

    /// @return The latest finished take, encoded by the worker - the Loop
    ///         layer's new content. Empty (`frameCount == 0`) until the
    ///         worker has encoded one.
    [[nodiscard]] sound_mind::codec::StreamImage currentImage() const;

    /// @return The latest finished take's audio - what currentImage() was
    ///         encoded from, kept so the take can be saved as a project
    ///         source (`v0.1.43.2`). Empty until the worker has encoded one.
    [[nodiscard]] sound_mind::codec::AudioBuffer currentTakeAudio() const;

    /// @return The take in progress, encoded as far as it has arrived -
    ///         for the live-encoding view. Starts over at each pass; empty
    ///         while frozen.
    [[nodiscard]] sound_mind::codec::StreamImage currentPreviewImage() const;

    /// @return A silent image one loop long, for a brand-new Loop layer.
    /// @note Allocates; UI thread.
    [[nodiscard]] sound_mind::codec::StreamImage emptyImage() const;

    /**
     * @brief The audio callback's work: mixes the project audio and the
     *        playing take into the output, records input into the other
     *        take, and at each pass end swaps the takes (unless frozen) and
     *        adopts any staged project audio.
     * @param inputChannelData Input channels (0 = left, 1 = right; mono is
     *        used for both), each `numSamples` long.
     * @param numInputChannels Number of input channels.
     * @param outputChannelData Output channels to fill.
     * @param numOutputChannels Number of output channels; any beyond two
     *        are silenced.
     * @param numSamples Samples per channel.
     * @note Real-time-safe.
     */
    void processBlock(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                       int numOutputChannels, int numSamples) noexcept;

    /// @brief The worker's own step: encodes a newly finished take into
    ///        currentImage(), and feeds currentPreviewImage() with whatever
    ///        of the current take has arrived since the last call.
    /// @note Allocates; never concurrently with itself.
    void processPendingAudio();

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void run() override;

    /// @brief Adopts staged project audio, if any, without ever waiting.
    void swapInStagedBacking() noexcept;

    juce::AudioDeviceManager deviceManager_;
    bool deviceAvailable_ = false;
    std::string deviceProblem_;
    AudioDeviceMode deviceMode_;
    sound_mind::codec::StreamCodecConfig config_;
    std::size_t loopLengthSamples_;
    std::string preferredInputDevice_;
    std::string preferredOutputDevice_;

    /// @brief The project audio played each pass (audio thread reads it).
    sound_mind::codec::AudioBuffer backing_;
    /// @brief Replacement project audio waiting for the next pass start.
    sound_mind::codec::AudioBuffer stagedBacking_;
    std::mutex stagedMutex_;
    std::atomic<bool> hasStagedBacking_{false};

    /// @brief The two take buffers - one playing, one recording - each one
    ///        pass long, allocated once.
    std::array<std::vector<float>, 2> takeLeft_;
    std::array<std::vector<float>, 2> takeRight_;
    /// @brief Which take buffer is playing (the other records).
    std::atomic<int> playingTake_{0};

    /// @brief Audio-thread position and recording state.
    std::size_t position_ = 0;
    bool recording_ = true;
    std::atomic<std::size_t> publishedPosition_{0};
    /// @brief Whether the audio thread is recording right now - for the
    ///        worker's live preview.
    std::atomic<bool> recordingNow_{true};
    std::atomic<std::uint64_t> takesCompleted_{0};

    std::atomic<bool> frozen_{false};
    std::atomic<bool> running_{false};
    std::atomic<float> inputGain_{1.0f};
    std::atomic<float> currentInputLevel_{0.0f};
    std::atomic<float> takeGainLeft_{1.0f};
    std::atomic<float> takeGainRight_{1.0f};
    /// @brief The output's last stage - see setLimiterEnabled(). Reset in
    ///        start(), before the callback runs.
    MasterLimiter limiter_{MasterLimiter::LookAhead::Off};

    /// @brief Worker-thread state.
    std::uint64_t encodedTakes_ = 0;
    std::uint64_t previewTake_ = 0;
    std::size_t previewFed_ = 0;
    sound_mind::codec::StreamIncrementalEncoder previewEncoder_;

    mutable std::mutex imageMutex_;
    sound_mind::codec::StreamImage currentImage_;
    sound_mind::codec::AudioBuffer currentTakeAudio_;
};

}  // namespace sound_mind::core
