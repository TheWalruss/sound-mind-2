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

/**
 * @brief The inverse of rectToPolar() - un-warps a polar-encoded source
 *        image (a "Sound Flower" shape, or any image whose content
 *        radiates from a centre point) back into a rectangular image -
 *        `docs/sound-mind-design.md`'s "Polar Coordinates (Sound Flower)"
 *        ("A source image that already looks like a flower... can be
 *        imported directly"), `docs/sound-mind-roadmap.md`'s `v0.Y.53.1`
 *        Installment B (polar-form image import).
 *
 * Same convention as rectToPolar(), inverted: `outputColumn` maps to
 * `theta` (`0` at twelve o'clock, increasing clockwise, spanning
 * `[arcStartRadians, arcEndRadians)`), `outputRow` maps to `r` (`0` - the
 * output's own top row - samples `maxRadius`, the highest encoded
 * frequency; the output's own bottom row samples `r = 0`, the centre,
 * the lowest). A direct, from-scratch reimplementation of the legacy
 * Python Studio's own `polar_image_to_rect()` - see rectToPolar()'s own
 * docs on why the coordinate convention is deliberately identical despite
 * the implementation not being a transliteration.
 *
 * @param source The polar-encoded image to un-warp.
 * @param originX The flower's own centre, in `source`'s own pixel
 *        coordinates (horizontal).
 * @param originY The flower's own centre (vertical).
 * @param maxRadius The maximum sampling radius, in `source`'s own pixels -
 *        pixels beyond this ring are never sampled.
 * @param arcStartRadians Where the output's own first column samples from,
 *        in radians (`0` = twelve o'clock, increasing clockwise).
 * @param arcEndRadians Where the output's own last column samples from.
 *        Equal to `arcStartRadians` (within a small epsilon) means a full
 *        `2*pi` circle, matching rectToPolar()'s own full-revolution
 *        convention - not a zero-width output.
 * @param outputWidth The result's own width, in pixels.
 * @param outputHeight The result's own height, in pixels.
 * @return An `outputWidth` x `outputHeight` image; a source pixel that
 *         would be sampled from outside `source`'s own bounds (the centre/
 *         radius picked a point off the edge of the image) is black,
 *         matching rectToPolar()'s/`placeOnCanvas()`'s own "nothing there"
 *         precedent. Empty (all-black) if `source`, `outputWidth`, or
 *         `outputHeight` is empty/zero.
 */
[[nodiscard]] RgbImage polarToRect(const RgbImage& source, double originX, double originY, double maxRadius,
                                    double arcStartRadians, double arcEndRadians, std::uint32_t outputWidth,
                                    std::uint32_t outputHeight);

}  // namespace sound_mind::codec
