#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::codec {

/// @brief The noise floor of the absolute dBFS scale every stored Stream
///        magnitude uses - intensity value 0, and the darkest the canvas
///        shows. See `StreamImage::leftMagnitudeDb`.
inline constexpr float kNoiseFloorDbfs = -96.0f;

/// @brief Full scale on that same dBFS scale - a full-scale (amplitude 1)
///        sine's own level; intensity value 1, and the brightest the
///        canvas shows.
inline constexpr float kFullScaleDbfs = 0.0f;

/**
 * @brief The one canonical dBFS -> intensity value mapping: linear in dB,
 *        0 at `kNoiseFloorDbfs`, 1 at `kFullScaleDbfs`. The canvas's own
 *        brightness and every "value"-unit intensity control use exactly
 *        this, so what you see and what you type agree.
 * @param db A level in dBFS.
 * @return The intensity value - not clamped, so above full scale reads
 *         above 1 and below the floor reads below 0.
 */
[[nodiscard]] constexpr float dbfsToIntensityValue(float db) noexcept {
    return (db - kNoiseFloorDbfs) / (kFullScaleDbfs - kNoiseFloorDbfs);
}

/// @brief The exact inverse of dbfsToIntensityValue().
/// @param value An intensity value (0 = noise floor, 1 = full scale).
/// @return The level in dBFS.
[[nodiscard]] constexpr float intensityValueToDbfs(float value) noexcept {
    return kNoiseFloorDbfs + value * (kFullScaleDbfs - kNoiseFloorDbfs);
}

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
 * @brief Overwrites, in `rgb` (a toRgbImage() render of `image`), every
 *        cell whose left or right level is above `kFullScaleDbfs` with a
 *        magenta/black checker, so clipping stands out where brightness
 *        alone has already saturated.
 * @param image The Stream image `rgb` was rendered from.
 * @param rgb The render to mark, in place; must match `image`'s size.
 * @return How many cells were marked.
 */
std::size_t markClippedCells(const StreamImage& image, RgbImage& rgb);

/**
 * @brief Renders a StreamImage as a single grayscale page, per
 *        `docs/sound-mind-design.md`'s Color Mapping - just amplitude (the
 *        average, in dB, of left and right), no color.
 * @param image The Stream image to render.
 * @return The rendered grayscale image (R == G == B at every pixel).
 */
[[nodiscard]] RgbImage toGrayscaleImage(const StreamImage& image);

/**
 * @brief Renders an already-normalized `[0, 1]` scalar field as a
 *        grayscale image - the Studio's own live MindWave Preview overlay
 *        (`docs/sound-mind-design.md`'s "Low Frequency Oscillations") is
 *        the first caller.
 *
 * Unlike toGrayscaleImage(const StreamImage&) above, no dB-to-brightness
 * mapping is applied - `field`'s own values map to a pixel byte linearly
 * (`0.0` -> `0`/black, `1.0` -> `255`/white), since a MindWave's own
 * evaluate() already returns a plain `[0, 1]` value, not a dB magnitude.
 * Out-of-range input is clamped rather than rejected, the same
 * defensiveness `MindWave::evaluate()`'s own docs already call for.
 *
 * Same row-0-is-highest-frequency orientation, and the same row-major,
 * bin-major input layout (see `cellIndex()`'s own docs), as every other
 * per-cell field/image conversion in this codebase.
 *
 * @param field The field to render, size `binCount * frameCount`.
 * @param frameCount The field's own width, in cells.
 * @param binCount The field's own height, in cells.
 * @return The rendered grayscale image (R == G == B at every pixel).
 */
[[nodiscard]] RgbImage toGrayscaleImage(const std::vector<float>& field, std::uint32_t frameCount,
                                         std::uint32_t binCount);

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

}  // namespace sound_mind::codec
