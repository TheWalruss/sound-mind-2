#pragma once

#include <optional>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/core/layer.h"

namespace sound_mind::core {

/**
 * @brief Renders a single layer's cached content as displayable pixels.
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
 * @param layer The layer to render.
 * @return The rendered RGB composite, or `std::nullopt` if the layer has no
 *         cached content yet (`layer.content()` is empty).
 */
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> renderLayer(const Layer& layer);

}  // namespace sound_mind::core
