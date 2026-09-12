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
 * - **Every other `FilterType`** (`UniformBlur`, `EdgePreservingBlur`,
 *   `DirectionalBlur`, `Sharpen`, `ToneCurve`) is not implemented yet -
 *   `docs/sound-mind-roadmap.md`'s own later installments of this same
 *   milestone. Returns `composite` unchanged (a harmless passthrough,
 *   not a silent wrong answer) until each lands.
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
