#pragma once

#include <vector>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/**
 * @brief Per-column (time-axis frame) loudness across every encoded bin,
 *        in dB - `docs/sound-mind-design.md`'s "Analysis" section,
 *        "Loudness & mastering", `v0.Y.52.1`.
 *
 * Each frame's own value averages that column's own left/right linear
 * amplitude across every bin (dB values aren't meaningfully additive, so
 * averaging happens in linear space first), then converts the result back
 * to dB. A simplified, RMS-style loudness measure - not full ITU-R
 * BS.1770 LUFS (which needs a K-weighting filter this installment doesn't
 * implement) - a real, honest simplification for this first installment,
 * not a claim of broadcast-standard compliance.
 *
 * Shared building block behind `sound_mind::core::renderLayerAmplitudeSummary()`'s
 * own Composer Mode track background (`v0.Y.48.1`) and the Layers Panel's
 * own per-layer loudness indicator (`v0.Y.52.1`) - both need exactly this
 * same per-column computation, so it lives here once rather than as two
 * independently-maintained copies.
 *
 * @param content The layer content to analyze.
 * @return One value per `content.frameCount` column, in dB - empty if
 *         `content.frameCount` or `content.config.binCount` is `0`.
 */
[[nodiscard]] std::vector<float> computeLoudnessProfile(const sound_mind::codec::StreamImage& content);

/**
 * @brief `profile`'s own average loudness, in dB.
 *
 * Averages in linear space first, then converts back - the same "dB
 * values aren't additive" reasoning `computeLoudnessProfile()`'s own docs
 * give, applied again across frames rather than across bins.
 *
 * @param profile A `computeLoudnessProfile()` result (or any other
 *        per-column dB sequence).
 * @return The average, in dB - a very quiet floor value if `profile` is
 *         empty (no real content to average).
 */
[[nodiscard]] float averageLoudnessDb(const std::vector<float>& profile) noexcept;

/**
 * @brief `profile`'s own single loudest value, in dB.
 *
 * @param profile A `computeLoudnessProfile()` result (or any other
 *        per-column dB sequence).
 * @return The maximum value in `profile` - a very quiet floor value if
 *         `profile` is empty.
 */
[[nodiscard]] float peakLoudnessDb(const std::vector<float>& profile) noexcept;

}  // namespace sound_mind::core
