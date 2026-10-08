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

    /// @brief The gain `encode()` applied to the input audio before
    ///        transforming it, so the stored amplitude makes good use of
    ///        the dB range regardless of how quiet or loud the source was -
    ///        `decode()` divides the final reconstructed audio by this same
    ///        value, so the decoded result's absolute level always matches
    ///        the original input exactly, independent of this internal
    ///        choice. `1.0` for a signal already at (or silent, so there's
    ///        nothing to measure a peak from) the target peak - see
    ///        `encode()`'s own docs for the exact formula.
    float inputNormalizationScale = 1.0f;

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
 * Three pre/post-processing steps wrap the core STFT, all per
 * `docs/sound-mind-codec-design.md` (and, before this, both tracked as open
 * items on `docs/sound-mind-architecture.md`'s *Decisions Needed*):
 *
 * - **Input-level normalization.** The combined left/right peak (so stereo
 *   balance is preserved - a per-channel peak would scale each channel
 *   differently) is scaled toward a target of `0.95` before anything else
 *   happens: `scale = (peak > 0) ? 0.95f / peak : 1.0f`. This can scale a
 *   quiet signal *up* (so its stored amplitude makes good use of the dB
 *   range, instead of reading as near-black) or a loud signal *down* (so it
 *   doesn't sit right at the dB ceiling with no margin), and is always
 *   exactly reversible - `scale` is carried on the returned `StreamImage`
 *   (`inputNormalizationScale`) and `decode()` divides it back out, so the
 *   decoded audio's absolute level always matches the original input,
 *   regardless of this internal choice.
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
 * `StreamIncrementalEncoder` deliberately does **not** apply normalization
 * or padding (both need knowledge of the whole signal's peak/extent, which
 * a live, causally-arriving stream doesn't have) - only A-weighting, which
 * is a pure per-bin, per-frame function with no such dependency. See that
 * class's own docs.
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
 * Reverses both of encode()'s reversible steps: each bin's A-weighting
 * offset is subtracted before reconstructing amplitude, and the final
 * reconstructed audio is divided by `image.inputNormalizationScale` before
 * trimming - see encode()'s own docs for why both are exact inverses
 * regardless of what internal choice encode() made.
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
