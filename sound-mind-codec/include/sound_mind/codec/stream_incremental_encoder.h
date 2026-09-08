#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::codec {

/**
 * @brief Incrementally encodes a growing, real-time audio stream into a
 *        Stream image, one frame at a time as enough newly captured audio
 *        makes each frame's analysis window available.
 *
 * The live-capture counterpart to encode()'s whole-buffer-at-once API - see
 * `docs/sound-mind-roadmap.md`'s Live Mode milestone (`v0.Y.7.1`) for why a
 * live stream needs this instead: encode() zero-pads any frame whose
 * analysis window extends past the end of the audio it's given (see
 * extractFrame() in stream_frame_codec.h), which only makes sense for a
 * clip that actually *has* an end. A live stream doesn't have one yet - so
 * a frame is only finalized here once its whole window's real captured
 * audio has actually arrived, never zero-padded. One consequence: for the
 * same total sample count, this produces a few fewer trailing frames than
 * encode() would (the last ~`fftSize/hopLength - 1` frames of a whole-
 * buffer encode are themselves partially zero-padded approximations of a
 * clip's true tail) - the frames both produce in common are numerically
 * identical, since it's the same DSP either way (see stream_frame_codec.h).
 *
 * @note Not real-time-safe (allocates throughout, same as encode()) -
 *       pushSamples() must run on a background thread, never an audio
 *       callback. Internally synchronized (a mutex) so pushSamples() and
 *       snapshot() may be called from different threads (e.g. a background
 *       encode thread and a UI-thread repaint timer) - but pushSamples()
 *       itself must not be called concurrently with itself (single
 *       producer only, matching how audio naturally arrives in order from
 *       one capture source).
 */
class StreamIncrementalEncoder {
public:
    /// @brief Constructs an encoder that will accumulate into a Stream
    ///        image using `config`'s parameters.
    /// @param config Encode parameters, exactly as encode() takes -
    ///        `config.sampleRateHz` should already match the audio that
    ///        will be pushed (unlike encode(), which overwrites it from
    ///        the AudioBuffer it's given - pushSamples() has no such
    ///        buffer to read a sample rate from).
    explicit StreamIncrementalEncoder(StreamCodecConfig config);

    /**
     * @brief Appends newly captured stereo audio, encoding and appending
     *        as many new frames to the accumulated image as this makes
     *        available.
     *
     * @param left Newly captured left-channel samples.
     * @param right Newly captured right-channel samples, same length as
     *        `left`.
     * @param numSamples Number of samples in `left`/`right`.
     */
    void pushSamples(const float* left, const float* right, std::size_t numSamples);

    /// @brief The number of raw samples pushed so far.
    /// @return `image().sampleCount` would report the same value - exposed
    ///         separately since computing a full snapshot() just to check
    ///         this would be wasteful.
    [[nodiscard]] std::size_t sampleCount() const;

    /// @brief The number of frames encoded so far.
    /// @return `image().frameCount` would report the same value - see
    ///         sampleCount()'s docs for why this is exposed separately.
    [[nodiscard]] std::uint32_t frameCount() const;

    /**
     * @brief A snapshot of the image encoded so far.
     * @return An independent copy (safe to keep and use - e.g. rendering
     *         it, or decoding it - without racing further pushSamples()
     *         calls), in the same row-major `[bin][frame]` layout encode()
     *         itself produces.
     */
    [[nodiscard]] StreamImage snapshot() const;

private:
    /// @brief Encodes the one frame starting at `startSample` (which must
    ///        already be known to have a complete window available) and
    ///        appends it to the per-frame result vectors. Caller (
    ///        pushSamples()) holds mutex_ already.
    void encodeFrameAt(std::ptrdiff_t startSample);

    StreamCodecConfig config_;
    std::uint32_t fftSize_;
    std::vector<float> window_;
    std::vector<float> linearBinIndex_;

    std::vector<float> left_;
    std::vector<float> right_;
    std::vector<float> mid_;

    // Scratch reused across encodeFrameAt() calls, to avoid reallocating
    // every single frame.
    std::vector<float> frameLeftScratch_;
    std::vector<float> frameRightScratch_;
    std::vector<float> frameMidScratch_;
    std::vector<std::complex<float>> spectrumLeftScratch_;
    std::vector<std::complex<float>> spectrumRightScratch_;
    std::vector<std::complex<float>> spectrumMidScratch_;

    // Per-frame results, frame-major (framesLeftDb_[frame] is a
    // config_.binCount-long vector) - cheap to append to as new frames
    // complete, unlike StreamImage's own row-major [bin][frame] layout,
    // which snapshot() transposes into on demand.
    std::vector<std::vector<float>> framesLeftDb_;
    std::vector<std::vector<float>> framesRightDb_;
    std::vector<std::vector<float>> framesPhase_;

    mutable std::mutex mutex_;
};

}  // namespace sound_mind::codec
