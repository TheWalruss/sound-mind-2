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

    /// @brief Left channel amplitude, in dBFS (A-weighted), row-major
    ///        `[bin][frame]` - a full-scale (amplitude 1) 1 kHz sine reads
    ///        about 0 dB in its own bin, at any hop length; see encode().
    std::vector<float> leftMagnitudeDb;

    /// @brief Right channel amplitude, in dBFS (A-weighted), row-major
    ///        `[bin][frame]` - see leftMagnitudeDb.
    std::vector<float> rightMagnitudeDb;

    /// @brief The shared phase channel, in radians, row-major
    ///        `[bin][frame]`.
    std::vector<float> sharedPhaseRadians;
};

/**
 * @brief Encodes stereo audio into the Stream codec's time-frequency
 *        representation.
 *
 * Three pre/post-processing steps wrap the core STFT, per
 * `docs/sound-mind-codec-design.md`:
 *
 * - **Absolute dBFS calibration.** Each bin's FFT magnitude has
 *   `20*log10(fftSize/4)` subtracted - the magnitude a full-scale sine's
 *   own bin reads with this Hann window - so stored values are absolute
 *   dBFS: a full-scale tone reads about 0 dB whatever the hop length, and
 *   a quieter input reads correspondingly lower. `decode()` adds it back.
 *   (This replaced a per-encode input normalization in `v0.1.33.1`, which
 *   put every clip near the top of the range regardless of its real level
 *   - see `docs/sound-mind-architecture.md`'s Decision #232.)
 * - **Reflection padding.** The signal is extended by
 *   `min(sampleRateHz / minFrequencyHz, numSamples - 1)` samples at each
 *   end, mirroring the signal's own content (not repeating the boundary
 *   sample) rather than the hard zero-silence `encode()` used to read past
 *   either edge - softening the discontinuity an analysis window sees near
 *   a clip's true start/end. Transparent to the stored representation:
 *   `frameCount`/`sampleCount` are unaffected, and `decode()` needs no
 *   knowledge that padding happened at all.
 * - **A-weighting.** Each output bin's stored dB value has a per-frequency
 *   offset added (the standard IEC 61672 A-weighting curve, normalized to
 *   `0` dB at 1 kHz) - so the stored/displayed amplitude better matches how
 *   loud a frequency actually *sounds*, rather than its raw linear-FFT
 *   magnitude (bass and treble content no longer reads as fainter than an
 *   equally-loud 1 kHz tone). Purely cosmetic: `decode()` subtracts the
 *   same offset back out before reconstructing audio, so this never
 *   affects the decoded result.
 *
 * `StreamIncrementalEncoder` applies the same calibration and A-weighting
 * (both pure per-bin, per-frame functions), but not padding, which needs
 * the whole signal's extent. See that class's own docs.
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
 * Reverses encode()'s per-bin steps: each bin's A-weighting offset is
 * subtracted and the full-scale reference added back before reconstructing
 * amplitude, so the decoded audio is at the original absolute level.
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
