#pragma once

#include <cstdint>

#include "sound_mind/codec/rgb_image.h"

namespace sound_mind::codec {

/**
 * @brief Rescales `source` to exactly `newWidth` x `newHeight`, via area
 *        averaging - every output pixel is the mean of every source pixel
 *        whose own region overlaps it, not a single sampled source pixel
 *        (nearest-neighbor's own approach, as `sound_mind::core::
 *        compositor.cpp`'s file-private `resampleHorizontally()` already
 *        uses for the *unrelated* "apply a layer's own rescaleFactor() to
 *        its timeline" case - correct there since inspecting a stretched-
 *        or-compressed timeline pixel-for-pixel is the point, but wrong
 *        for a genuine *shrink*, where skipping most of the source
 *        entirely would alias/lose detail).
 *
 * Not aspect-ratio-preserving - `newWidth`/`newHeight` are applied exactly,
 * independently; a caller wanting letterboxing computes its own target
 * size first. Correct (if not meaningfully "diligent") for the degenerate
 * `newWidth == source.width && newHeight == source.height` case too -
 * every output pixel's own source region is then exactly one source
 * pixel, so this reduces to a plain copy.
 *
 * @param source The image to rescale - `source.width`/`source.height` of
 *        `0` produce an all-black result rather than dividing by zero.
 * @param newWidth The result's own width, in pixels.
 * @param newHeight The result's own height, in pixels.
 * @return The rescaled image, exactly `newWidth` x `newHeight`.
 */
[[nodiscard]] RgbImage downsampleAveraged(const RgbImage& source, std::uint32_t newWidth, std::uint32_t newHeight);

}  // namespace sound_mind::codec
