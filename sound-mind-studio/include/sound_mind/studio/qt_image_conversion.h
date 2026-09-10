#pragma once

#include <QImage>

#include "sound_mind/codec/rgb_image.h"

namespace sound_mind::studio {

/**
 * @brief Converts a QImage to `codec::RgbImage`, forcing a consistent
 *        3-byte-per-pixel layout first regardless of the source's own
 *        format.
 *
 * Extracted out of `MainWindow` as part of the Phase 2.5 Refactor & Clean
 * Up milestone (`v0.Y.23.1`) - the Qt-specific half of the RgbImage/QImage
 * bridge `sound-mind-codec` itself can't own (it has no Qt dependency, per
 * `docs/sound-mind-architecture.md`'s Build & Module Layout).
 *
 * @param source The image to convert.
 * @return The converted image, with its own independent pixel storage.
 */
[[nodiscard]] sound_mind::codec::RgbImage toRgbImage(const QImage& source);

/**
 * @brief Wraps an RgbImage's pixel data as a QImage, without copying it -
 *        valid only as long as `image` itself stays alive.
 *
 * A plain, non-owning view - callers that need the result to outlive
 * `image` (or that will mutate it) should call `.copy()` on the return
 * value themselves; `CanvasWidget::paintEvent()`'s own synchronous paint
 * call is the case that doesn't need to, which is what this shape exists
 * for.
 *
 * @param image The image to view - deliberately not called `toQImage()`,
 *        so a copy-needing call site can't reach for this by mistake and
 *        get a dangling-once-`image`-is-gone result instead.
 * @return A QImage viewing `image`'s own pixel storage directly.
 */
[[nodiscard]] QImage toQImageView(const sound_mind::codec::RgbImage& image);

}  // namespace sound_mind::studio
