#pragma once

#include <cstdint>
#include <optional>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"

namespace sound_mind::core {

/**
 * @brief Renders a single layer's cached content as displayable pixels,
 *        placed onto a `canvasWidth`-wide time axis.
 *
 * A single-layer building block, used both directly (nothing else needs
 * an individual layer's own placed-but-unblended render right now) and as
 * the geometry compositeProject() below repeats per layer, in the
 * amplitude/phase domain instead of already-converted RGB pixels.
 *
 * Applies the layer's own horizontal transform - `layer.rescaleFactor()`
 * (stretches/compresses the content's own timeline, applied first) then
 * `layer.translationColumns()` (shifts the result earlier/later, applied
 * second) - and always returns an image exactly `canvasWidth` columns
 * wide: a gap left by translation, or a layer narrower than `canvasWidth`
 * even with no transform at all, is padded with black columns; anything
 * that would fall outside `[0, canvasWidth)` is cropped.
 *
 * @param layer The layer to render.
 * @param canvasWidth The project's canvas width, in pixels/columns - see
 *        `sound_mind::core::ProjectSettings::canvasWidth`.
 * @return The rendered RGB composite, exactly `canvasWidth` columns wide,
 *         or `std::nullopt` if the layer has no cached content yet
 *         (`layer.content()` is empty).
 */
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> renderLayer(const Layer& layer,
                                                                      std::uint32_t canvasWidth);

/**
 * @brief Composites every visible, contentful layer in `project` into one
 *        combined `StreamImage`, per `docs/sound-mind-design.md`'s new
 *        "Compositing" subsection and `docs/sound-mind-roadmap.md`'s
 *        `v0.Y.27.1` (Multi-layer Compositing) - this is the real
 *        multi-layer composite `renderLayer()`'s own docs, and several
 *        milestones before it, described as still-future work.
 *
 * **Normal compositing is audio-style mixing, not image-style
 * alpha-over**: each layer's own stored amplitude (dB) converts to
 * linear, is scaled by that layer's own `opacity()` acting as a linear
 * gain, and sums with every other visible layer's own contribution at
 * the same output bin/column - as a complex value, using each layer's
 * own `sharedPhaseRadians` to give its (per-channel) linear amplitude a
 * direction before summing. The summed left/right complex values'
 * magnitudes become the composite's own left/right amplitude (converted
 * back to dB); the composite's own single shared phase is the *angle* of
 * the summed mono/mid signal (`(left + right) / 2`) - the same
 * "approximate shared phase from a mono downmix" contract a single
 * layer's own encode() already establishes (see `StreamImage`'s own
 * docs), just applied to the sum of every layer instead of one. A fully
 * opaque layer never mutes what's beneath it this way, unlike image
 * alpha-over - summation has no such "coverage" concept, and is also
 * commutative, so unlike alpha-over, stack *order* doesn't affect the
 * result (only which layers participate does).
 *
 * Every layer is placed onto the project's own `canvasWidth`-wide time
 * axis exactly the way renderLayer() places a single layer's own RGB
 * render - the same rescale-then-translate geometry, applied here to the
 * layer's own raw amplitude/phase columns instead of already-converted
 * pixels, so both this function and renderLayer() agree on where a given
 * layer's own content actually sits. A hidden layer (`layer.visible()`
 * is `false`) or one with no cached content contributes nothing, the
 * same as `renderLayer()`'s own single-layer convention.
 *
 * The returned image's own `config` comes from
 * `streamCodecConfigFor(project.settings())` (so every layer's amplitude
 * lines up bin-for-bin against a shared frequency range), `frameCount`
 * is `project.settings().canvasWidth`, and `sampleCount` is
 * `frameCount * config.hopLength` - the exact sample count
 * `sound_mind::codec::decode()` should reconstruct for the project's own
 * full canvas-width duration.
 *
 * @note CPU-only, and not real-time-safe as written (allocates
 *       throughout) - the same characterization `sound_mind::codec::
 *       encode()`/`decode()` already carry, and consistent with
 *       `docs/sound-mind-roadmap.md`'s own confirmed scope for this
 *       milestone (a DirectX 12 Compute path is deliberately deferred
 *       to a dedicated performance pass, not built speculatively here).
 *
 * @param project The project to composite.
 * @return The combined `StreamImage`, or `std::nullopt` if no layer in
 *         `project` is both visible and has any cached content at all.
 */
[[nodiscard]] std::optional<sound_mind::codec::StreamImage> compositeProject(const Project& project);

}  // namespace sound_mind::core
