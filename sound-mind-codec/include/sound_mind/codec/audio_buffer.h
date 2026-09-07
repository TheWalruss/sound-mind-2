#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sound_mind::codec {

/**
 * @brief A simple in-memory stereo PCM audio buffer, sample-rate tagged.
 *
 * Planar (not interleaved) storage: left and right channels are held as two
 * independent arrays, since every consumer so far (the Stream codec's
 * per-channel analysis) wants to walk one channel's samples at a time.
 * Samples are normalized floating point, nominally in [-1, 1] - not clamped
 * or enforced here.
 *
 * @note Not real-time-safe as a type: constructing or resizing one
 *       allocates. Real-time audio paths that need to hand samples to or
 *       from the audio callback thread need a different, allocation-free
 *       mechanism - this is for offline/background-thread codec work only
 *       (see docs/sound-mind-architecture.md's Threading & Real-Time
 *       Model).
 */
struct AudioBuffer {
    /// @brief Sample rate the audio was captured/generated at.
    std::uint32_t sampleRateHz = 44100;

    /// @brief Left channel samples.
    std::vector<float> left;

    /// @brief Right channel samples.
    std::vector<float> right;

    /// @brief Number of sample frames.
    /// @return `left`'s length. Left and right are expected to be the same
    ///         length; callers are responsible for keeping them so.
    [[nodiscard]] std::size_t frameCount() const noexcept { return left.size(); }
};

}  // namespace sound_mind::codec
