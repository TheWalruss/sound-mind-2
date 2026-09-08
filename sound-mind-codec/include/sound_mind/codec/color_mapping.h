#pragma once

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::codec {

/**
 * @brief Renders a StreamImage as a basic RGB composite, per
 *        `docs/sound-mind-design.md`'s Color Mapping: red = left amplitude,
 *        green = right amplitude, blue = the shared phase channel.
 *
 * One pixel per `[bin][frame]` cell - width = `frameCount`, height =
 * `binCount`. Row 0 (top) is the *highest* encoded frequency, matching how
 * a spectrogram is conventionally displayed (bass at the bottom, treble at
 * the top) - the opposite of StreamImage's own internal storage, which
 * keeps bin 0 = lowest (see StreamImage's docs for why).
 *
 * @param image The Stream image to render.
 * @return The rendered RGB composite.
 */
[[nodiscard]] RgbImage toRgbImage(const StreamImage& image);

/**
 * @brief Renders a StreamImage as a single grayscale page, per
 *        `docs/sound-mind-design.md`'s Color Mapping - just amplitude (the
 *        average, in dB, of left and right), no color.
 * @param image The Stream image to render.
 * @return The rendered grayscale image (R == G == B at every pixel).
 */
[[nodiscard]] RgbImage toGrayscaleImage(const StreamImage& image);

/**
 * @brief Converts an RGB image into a StreamImage - the reverse of
 *        toRgbImage() - per `docs/sound-mind-design.md`'s "sound and image
 *        are one continuous surface" principle: an imported image becomes
 *        amplitude/phase data like any audio import, not a picture with no
 *        underlying sound representation.
 *
 * No transform is involved (unlike encode()): the image's own pixel grid
 * *is* the bin/frame grid directly (`width` -> `frameCount`, `height` ->
 * `binCount`, overwriting whatever `config` supplied for those two fields -
 * the same pattern encode() uses for `sampleRateHz`), so an imported
 * image's amplitude/phase resolution is exactly whatever resolution the
 * image itself was.
 *
 * @param image The RGB image to convert, in toRgbImage()'s row-0-is-
 *        highest-frequency orientation.
 * @param config The codec parameters the resulting StreamImage should
 *        carry (sample rate, hop length, frequency range) - an image has no
 *        sample rate of its own, so this must be supplied by the caller.
 * @return The converted StreamImage.
 */
[[nodiscard]] StreamImage fromRgbImage(const RgbImage& image, const StreamCodecConfig& config);

/**
 * @brief Renders a PoolImage as an RGB composite, using the same
 *        red/green/blue convention as `toRgbImage(const StreamImage&)`:
 *        red = left amplitude, green = right amplitude, blue = a phase
 *        channel - here, arbitrarily but consistently, the *left* phase
 *        (PoolImage has no single "the" phase channel the way StreamImage
 *        does, since it keeps left and right independent; picking one
 *        deterministically is what makes a layer's Pool and Stream renders
 *        directly comparable pixel-for-pixel, per PoolImage's own docs on
 *        why they share a grid).
 *
 * Same row-0-is-highest-frequency orientation as the StreamImage overload.
 *
 * @param image The Pool image to render.
 * @return The rendered RGB composite.
 */
[[nodiscard]] RgbImage toRgbImage(const PoolImage& image);

}  // namespace sound_mind::codec
