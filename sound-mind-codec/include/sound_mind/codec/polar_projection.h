#pragma once

#include <cstdint>

#include "sound_mind/codec/rgb_image.h"

namespace sound_mind::codec {

/**
 * @brief Projects a rectangular image onto a circular "Sound Flower" -
 *        `docs/sound-mind-design.md`'s "Polar Coordinates (Sound Flower)",
 *        `docs/sound-mind-roadmap.md`'s `v0.Y.53.1`.
 *
 * Time wraps around the ring (`theta = 0` at twelve o'clock, increasing
 * clockwise, one full revolution covering `source`'s own full width) and
 * frequency radiates outward from the centre (`r = 0` at the centre
 * sampling `source`'s own bottom row - the lowest encoded frequency, per
 * `toRgbImage()`'s own "row 0 = highest frequency" convention - `r` at the
 * outer edge sampling `source`'s own top row, the highest). Every output
 * pixel is bilinearly sampled, matching `downsampleAveraged()`'s own
 * "don't alias, sample properly" precedent rather than nearest-neighbor.
 *
 * A direct, from-scratch reimplementation of the legacy Python Studio's
 * own `rect_to_polar()` (`../sound-mind/packages/sound_mind_studio/src/
 * sound_mind_studio/views/polar.py`) - CLAUDE.md's own "lessons learned,
 * not code to port directly" stance still applies (this is a fresh
 * scalar-loop translation, not a transliteration), but the coordinate
 * convention itself (theta/r directions, which row samples the centre vs.
 * the outer ring) is deliberately identical, since that convention is
 * already load-bearing documentation in `docs/sound-mind-design.md`.
 *
 * @param source The rectangular image to project - typically the
 *        project's own composited canvas.
 * @param diameter The output's own width and height, in pixels (always
 *        square) - the disk's own radius is `diameter / 2`.
 * @return A `diameter` x `diameter` image; every pixel outside the disk is
 *         black, matching `placeOnCanvas()`'s own "outside the source, stays
 *         black" precedent rather than taking a separate background-color
 *         parameter no other image transform here offers either. Empty
 *         (all-black) if `source` or `diameter` is empty/zero.
 */
[[nodiscard]] RgbImage rectToPolar(const RgbImage& source, std::uint32_t diameter);

}  // namespace sound_mind::codec
