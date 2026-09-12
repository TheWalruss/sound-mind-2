#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief Applies a Filter layer's own filter to `composite` - the actual
 *        DSP behind `docs/sound-mind-design.md`'s "Filter Layer", called
 *        by `compositeProject()` (`compositor.h`) once per Filter-type
 *        layer, on the running composite of everything beneath it.
 *
 * Dispatches on `config.type()`:
 * - **`FrequencyAxisGradient`** (this milestone's first real
 *   implementation - see `docs/sound-mind-roadmap.md`'s `v0.Y.28.1`,
 *   Installment A): `config.frequencyGradient()` is evaluated once per
 *   bin, at `t = bin / (binCount - 1)` (`t=0` the lowest encoded
 *   frequency, `t=1` the highest) - `binIndexToFrequency()`'s own
 *   log-scaled bin-to-Hz mapping already makes this exactly "normalized
 *   position across the encoded frequency range", with no separate Hz
 *   conversion needed. Every column in that bin's own row blends toward
 *   the resulting stop's own `leftIntensity`/`rightIntensity`, by
 *   `leftOpacity`/`rightOpacity` - the identical `lerp(existing, target,
 *   opacity)` blend `paint_application.cpp`'s own Fill/Paint already use
 *   for a `Gradient` (see `docs/sound-mind-architecture.md`'s own
 *   Decision recording why this reuses `Gradient` unchanged: the
 *   Equalizer layer's own "Cut" editor is this same filter, with its own
 *   UI simply always writing `leftIntensity`/`rightIntensity` at the
 *   silence floor and exposing only opacity, as "how much to cut").
 *   Phase is left untouched - only amplitude is affected.
 * - **`UniformBlur`/`EdgePreservingBlur`/`DirectionalBlur`/`Sharpen`**
 *   (`v0.Y.28.1` Installment B): each operates on `leftMagnitudeDb` and
 *   `rightMagnitudeDb` independently, directly in dB space (not converted
 *   to linear amplitude first - the same domain `FrequencyAxisGradient`
 *   and the legacy Python reference implementation both already operate
 *   in, since legacy's own normalized `[0,1]` pixel domain is just a
 *   linear rescale of dB). Phase is left untouched, matching
 *   `FrequencyAxisGradient`'s own precedent - legacy's optional
 *   `apply_to_phase` toggle isn't represented here. Every one of these
 *   four uses clamp-to-edge boundary handling (the nearest in-bounds
 *   cell stands in for anything off the edge of the grid) - a
 *   deliberate simplification, not a port of legacy's own
 *   per-filter-inconsistent boundary modes (`scipy.ndimage`'s
 *   default `reflect` for its Gaussian/median filters, an explicit
 *   `nearest` for its convolve-based motion blur - see
 *   `../sound-mind/packages/sound_mind_studio/src/sound_mind_studio/
 *   filters/core.py`). Radii/sizes/lengths are all in raw bin/column
 *   units, per `FilterConfiguration`'s own docs.
 *   - `UniformBlur`: a separable 2D Gaussian blur (`blurSigma()`,
 *     floored at `0.1` matching legacy), kernel truncated at 4 standard
 *     deviations (matching `scipy.ndimage.gaussian_filter`'s own default
 *     `truncate`).
 *   - `EdgePreservingBlur`: a 2D median filter over a square window
 *     (`medianSize()`, forced odd and at least `3` the same way legacy
 *     forces it: `max(3, size | 1)`).
 *   - `DirectionalBlur`: a line-shaped kernel stepped along
 *     `directionalBlurAngleDegrees()` for `directionalBlurLength()`
 *     samples each side of center, normalized to sum to 1 - ported
 *     directly from legacy's own `motion_blur_filter` algorithm (`0`°
 *     blurs along the time axis/columns, `90`° along the frequency
 *     axis/bins).
 *   - `Sharpen`: an unsharp mask - `original + sharpenAmount() *
 *     (original - gaussianBlur(original, sigma=1.0))`, the same fixed
 *     internal sigma legacy's own `sharpen_filter` uses.
 * - **`ToneCurve`** is not implemented yet - `docs/sound-mind-roadmap.md`'s
 *   own later installment of this same milestone. Returns `composite`
 *   unchanged (a harmless passthrough, not a silent wrong answer) until
 *   it lands.
 *
 * @param composite The running composite to filter - everything visible
 *        beneath the Filter layer this configuration belongs to, already
 *        composited (see `compositeProject()`'s own docs).
 * @param config Which filter to apply, and its own parameters.
 * @param settings The project settings `streamCodecConfigFor()` needs,
 *        for any filter whose own algorithm depends on the project's
 *        encoded frequency range/bin count.
 * @return The filtered result, the same shape (`config`/`frameCount`) as
 *         `composite`.
 */
[[nodiscard]] sound_mind::codec::StreamImage applyFilter(const sound_mind::codec::StreamImage& composite,
                                                             const FilterConfiguration& config,
                                                             const ProjectSettings& settings);

}  // namespace sound_mind::core
