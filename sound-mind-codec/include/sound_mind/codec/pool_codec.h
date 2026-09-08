#pragma once

#include <cstdint>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::codec {

/**
 * @brief The Pool codec's in-memory time-frequency representation.
 *
 * Per `docs/sound-mind-design.md`'s Pool Mode and
 * `docs/sound-mind-architecture.md`'s Pool File Format: independent left
 * and right amplitude *and* phase (four planes total, unlike StreamImage's
 * three) - built on a Non-Stationary Gabor Transform (NSGT), a
 * constant-Q transform with deep frequency resolution at bass frequencies
 * and fine time resolution at treble, unlike Stream's fixed-window STFT.
 * Reconstruction is near-perfect (limited only by floating-point precision
 * and the interpolation-to-a-common-grid step every practical NSGT
 * implementation needs for rectangular storage - see poolEncode()'s docs),
 * which is what "lossless" means here, matching how the legacy codec and
 * the wider NSGT literature use the term.
 *
 * Shares `StreamCodecConfig` for its grid parameters (sample rate, hop
 * length, bin count, frequency range) deliberately: per the design doc's
 * "minimize the auditory and visual difference between Pool and Stream
 * output," using the same grid for both makes a layer's Pool and Stream
 * renders directly, pixel-for-pixel comparable.
 */
struct PoolImage {
    /// @brief The parameters this image was encoded with.
    StreamCodecConfig config;

    /// @brief Number of time-axis columns.
    std::uint32_t frameCount = 0;

    /// @brief The exact sample count of the audio this was encoded from.
    std::uint64_t sampleCount = 0;

    /// @brief Left channel amplitude, in dB, row-major `[bin][frame]`.
    std::vector<float> leftMagnitudeDb;

    /// @brief Right channel amplitude, in dB, row-major `[bin][frame]`.
    std::vector<float> rightMagnitudeDb;

    /// @brief Left channel phase, in radians, row-major `[bin][frame]`.
    std::vector<float> leftPhaseRadians;

    /// @brief Right channel phase, in radians, row-major `[bin][frame]`.
    std::vector<float> rightPhaseRadians;
};

/**
 * @brief Encodes stereo audio into the Pool codec's NSGT representation.
 *
 * The transform: one global FFT of the whole signal per channel (NSGT
 * operates on the entire signal at once, not framed like Stream's STFT);
 * for each log-spaced frequency bin, a Hann-windowed slice of that
 * spectrum (width proportional to the bin's own center frequency, for a
 * constant-Q scale) is extracted and inverse-FFT'd directly, which - per
 * the standard DFT filter-bank identity - yields that bin's own
 * "native rate" complex baseband coefficient sequence in one step (no
 * separate demodulation needed). That native-rate sequence's magnitude and
 * (unwrapped) phase are then linearly interpolated onto this image's
 * common `frameCount`-wide pixel grid, the same way Stream interpolates
 * across frequency bins - just transposed onto the time axis here. This
 * last step is the only source of reconstruction error in an otherwise
 * perfectly-invertible transform, and is why "lossless" is a practical,
 * not mathematically absolute, claim (matching the wider NSGT literature
 * and the legacy codec this is informed by).
 *
 * Frequencies outside `[minFrequencyHz, maxFrequencyHz]` (notably DC and
 * Nyquist) are not represented and reconstruct as silence, by design - a
 * signal's real energy in that discarded range is a genuine, accepted
 * loss, not a bug.
 *
 * @param audio The audio to encode.
 * @param config Encode parameters; the same values are needed again to
 *        decode, and are carried on the returned PoolImage for that
 *        reason. `config.sampleRateHz` is overwritten with `audio`'s own
 *        sample rate.
 * @return The encoded representation.
 *
 * @note CPU-only, not real-time-safe (allocates throughout) - Pool is an
 *       explicit, manual, off-line action per the design doc, never part
 *       of the real-time audio path.
 */
[[nodiscard]] PoolImage poolEncode(const AudioBuffer& audio, const StreamCodecConfig& config);

/**
 * @brief Decodes a Pool image back into stereo audio.
 * @param image The encoded representation to decode.
 * @return The reconstructed audio, trimmed to `image.sampleCount` samples.
 */
[[nodiscard]] AudioBuffer poolDecode(const PoolImage& image);

}  // namespace sound_mind::codec
