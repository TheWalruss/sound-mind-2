#pragma once

#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/**
 * @brief Stereo image measurements for the Analysis panel's Stereo tab
 *        (Pre-Beta Plan `v0.Y.80.1`).
 *
 * **Correlation** is the normalized cross-correlation of left and right:
 * `+1` mono, `0` unrelated channels, `-1` one channel the other inverted
 * (a mono-compatibility problem - the channels cancel when summed).
 * **Width** is `sqrt(Eside) / (sqrt(Emid) + sqrt(Eside))` with mid
 * `(L+R)/2` and side `(L-R)/2`: `0` mono, about `0.5` for unrelated
 * channels, `1` fully out of phase (the legacy Studio's own measure).
 */
struct StereoMeasurement {
    /// @brief Correlation over the whole buffer, `[-1, 1]` (`1` when silent).
    double correlation = 1.0;
    /// @brief Width over the whole buffer, `[0, 1]`.
    double width = 0.0;
    /// @brief Correlation over each 400 ms window, every 100 ms - aligned
    ///        with `LoudnessMeasurement::momentaryLufs`.
    std::vector<float> correlationOverTime;
    /// @brief Width over the same windows.
    std::vector<float> widthOverTime;
};

/**
 * @brief Measures `audio`'s stereo image - see `StereoMeasurement`.
 * @param audio Stereo audio; a mono buffer (no right channel) reads as mono.
 * @return The measurement.
 * @note Allocates; run off the audio and GUI threads for long buffers.
 */
[[nodiscard]] StereoMeasurement measureStereo(const sound_mind::codec::AudioBuffer& audio);

/**
 * @brief Each loudness band's stereo width over the whole of `content`, read
 *        straight from the spectral data: mid/side from each cell's left and
 *        right amplitudes (A-weighting removed), energies summed over the
 *        band's cells, then the same width formula as `StereoMeasurement`.
 *        One value per `loudnessBands()` entry; `0` for a band with no bins
 *        or no energy.
 * @note The format stores one phase shared by both channels, so this sees
 *       level differences between the channels, not phase differences -
 *       `measureStereo()` on the decoded audio sees both.
 * @param content Layer or composite content.
 * @return The widths, lowest band first.
 */
[[nodiscard]] std::vector<float> bandStereoWidths(const sound_mind::codec::StreamImage& content);

}  // namespace sound_mind::core
