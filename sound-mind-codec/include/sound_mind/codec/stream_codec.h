#pragma once

#include <cstdint>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::codec {

/**
 * @brief Parameters controlling a Stream encode, and needed again to decode.
 *
 * Mirrors the handful of fields `docs/sound-mind-architecture.md`'s Stream
 * File Format section calls "self-describing" (sample rate, hop/timestep,
 * frequency range) - a StreamImage carries its own copy of this, so a
 * Stream file never needs anything from outside itself to decode.
 *
 * @note Deliberately independent of `sound_mind::core::ProjectSettings`:
 *       per the architecture doc's Build & Module Layout, `sound-mind-codec`
 *       has no project model and must stay usable standalone - `core`
 *       depends on `codec`, never the other way around.
 */
struct StreamCodecConfig {
    /// @brief Sample rate of the audio this config was/will be built from.
    std::uint32_t sampleRateHz = 44100;

    /// @brief Samples between successive analysis frames - the width of one
    ///        time-axis column. 441 is ~10 ms at 44100 Hz, matching
    ///        `ProjectSettings`' default timestep.
    std::uint32_t hopLength = 441;

    /// @brief Number of log-spaced frequency bins - the image's height.
    std::uint32_t binCount = 512;

    /// @brief Lower edge of the encoded frequency range.
    float minFrequencyHz = 20.0f;

    /// @brief Upper edge of the encoded frequency range. Clamped to the
    ///        Nyquist frequency (`sampleRateHz / 2`) at encode/decode time
    ///        if it would otherwise exceed it.
    float maxFrequencyHz = 16000.0f;
};

/**
 * @brief The Stream codec's in-memory time-frequency representation.
 *
 * Per `docs/sound-mind-architecture.md`'s Stream File Format: left and
 * right amplitude, plus a single *shared* phase (not independent left/right
 * phase) taken from the two channels' mono/mid downmix - the "approximate
 * shared phase" option for the format's third channel. This is what makes
 * Stream mode's transform non-invertible in the strict sense Pool's NSGT
 * is meant to be: reconstructing left and right with the same phase is an
 * approximation, not a perfect inverse, traded deliberately for speed and
 * for fitting the format's three-channel budget. For audio where left and
 * right are identical (or very similar), the shared phase equals each
 * channel's true phase and reconstruction is effectively exact; genuine
 * stereo content trades some fidelity for that budget.
 *
 * Storage is row-major `[bin][frame]`, bin 0 = the lowest encoded
 * frequency - unlike Pool's TIFF convention of flipping so row 0 reads as
 * the top of the image in a generic viewer, Stream has no such
 * external-viewer constraint to satisfy.
 */
struct StreamImage {
    /// @brief The parameters this image was encoded with.
    StreamCodecConfig config;

    /// @brief Number of time-axis columns.
    std::uint32_t frameCount = 0;

    /// @brief The exact sample count of the audio this was encoded from, so
    ///        decode() can trim the reconstructed buffer back to it exactly
    ///        - frame-based analysis/synthesis otherwise pads to a whole
    ///        number of hops.
    std::uint64_t sampleCount = 0;

    /// @brief Left channel amplitude, in dB, row-major `[bin][frame]`.
    std::vector<float> leftMagnitudeDb;

    /// @brief Right channel amplitude, in dB, row-major `[bin][frame]`.
    std::vector<float> rightMagnitudeDb;

    /// @brief The shared phase channel, in radians, row-major
    ///        `[bin][frame]`.
    std::vector<float> sharedPhaseRadians;
};

/**
 * @brief Encodes stereo audio into the Stream codec's time-frequency
 *        representation.
 *
 * @param audio The audio to encode.
 * @param config Encode parameters; the same values are needed again to
 *        decode, and are carried on the returned StreamImage for that
 *        reason. `config.sampleRateHz` is overwritten with `audio`'s own
 *        sample rate.
 * @return The encoded representation.
 *
 * @note CPU-only for now - see `docs/sound-mind-architecture.md`'s GPU /
 *       Audio-Thread Handoff section for the not-yet-prototyped GPU path.
 *       Not real-time-safe as written (allocates throughout) - the
 *       audio-callback-safe decode path is Playback's concern
 *       (`v0.Y.4.1` in `docs/sound-mind-roadmap.md`), not this one.
 */
[[nodiscard]] StreamImage encode(const AudioBuffer& audio, const StreamCodecConfig& config);

/**
 * @brief Decodes a Stream image back into stereo audio.
 *
 * @param image The encoded representation to decode.
 * @return The reconstructed audio, trimmed to `image.sampleCount` samples.
 *
 * @note Reconstruction is approximate, not lossless - see StreamImage's
 *       documentation for why (shared phase, not independent left/right).
 * @note Not real-time-safe as written (allocates throughout) - see
 *       encode()'s note.
 */
[[nodiscard]] AudioBuffer decode(const StreamImage& image);

}  // namespace sound_mind::codec
