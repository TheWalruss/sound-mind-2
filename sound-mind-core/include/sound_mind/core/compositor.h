#pragma once

#include <cstdint>
#include <optional>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/core/layer.h"

namespace sound_mind::core {

/**
 * @brief Renders a single layer's cached content as displayable pixels,
 *        placed onto a `canvasWidth`-wide time axis.
 *
 * A first, minimal version of the Compositor sketched in
 * `docs/sound-mind-architecture.md`'s Core Data Model - single-layer only,
 * with no blending, opacity, or MindWave-bound parameters applied yet
 * (those need a real multi-layer composite loop and blend modes, neither
 * of which exist yet). It exists now so `sound-mind-studio`'s CanvasWidget
 * can stay a thin display of whatever Core hands it, rather than doing
 * Color-Mapping pixel conversion itself - `sound-mind-codec::toRgbImage()`
 * is where that conversion actually happens; this is the Core-level entry
 * point that ties a Layer to it.
 *
 * As of `v0.Y.21.1` (Layer Time Alignment), this also applies the layer's
 * own horizontal transform - `layer.rescaleFactor()` (stretches/compresses
 * the content's own timeline, applied first) then `layer.translationColumns()`
 * (shifts the result earlier/later, applied second) - and always returns an
 * image exactly `canvasWidth` columns wide: a gap left by translation, or a
 * layer narrower than `canvasWidth` even with no transform at all, is
 * padded with black columns; anything that would fall outside
 * `[0, canvasWidth)` is cropped. Rendering directly onto the canvas's own
 * time axis, rather than at the layer's native width, is deliberate ahead
 * of real multi-layer compositing (`docs/sound-mind-architecture.md`'s
 * Decisions Made) - every layer's render already lines up column-for-column
 * with every other's, so a later composite loop can composite them
 * directly without each caller re-deriving this placement itself.
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

}  // namespace sound_mind::core
