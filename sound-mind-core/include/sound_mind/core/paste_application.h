#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/paste_operation.h"

namespace sound_mind::core {

/**
 * @brief Captures a rectangular snapshot of `source`, within `bounds`, as a
 *        `Clip` - the actual work behind Copy (and the copy half of Cut).
 *
 * Uses the same time/frequency-to-frame/bin clamping `applyFillOperation()`
 * already uses (`timeToFrameIndex()`/`frequencyToBinIndex()` from
 * `paint_application.h`, both `std::round()`ed and clamped to `source`'s own
 * valid frame/bin range), so a selection's own bounds map onto the same
 * pixels a Fill confined to that same selection would touch.
 *
 * @param source The layer content to copy from.
 * @param bounds The selection's own time/frequency extent to capture.
 * @return A `Clip` holding a copy of every cell `bounds` covers in `source`
 *         - empty (`frameCount`/`binCount` both `0`) if `source` has no
 *         content (`frameCount == 0` or `config.binCount == 0`).
 */
[[nodiscard]] Clip captureClip(const sound_mind::codec::StreamImage& source, const TimeFrequencyRect& bounds);

/**
 * @brief Applies a `PasteOperation`'s own clip directly onto a
 *        `StreamImage`'s amplitude/phase planes, in place.
 *
 * Unlike `applyPaintOperation()`/`applyFillOperation()`, this is a direct
 * overwrite, not a gradient-blended edit: every destination cell within
 * `operation.bounds()` is replaced outright by the clip's own corresponding
 * source cell - a paste reproduces exactly what was copied, not a color
 * mixed toward it. The clip is positioned so its own top-left (lowest time,
 * lowest frequency bin) lands at `operation.bounds()`'s own low corner;
 * any part of the clip that would fall outside `content`'s own frame/bin
 * range is silently clipped, the same as `applyFillOperation()`'s own
 * out-of-range handling.
 *
 * @param operation The paste to apply - its own `bounds()`/`clip()` fully
 *        describe it.
 * @param content The `StreamImage` to paste into, mutated in place - its
 *        own `config` supplies the `frequencyToBinIndex()`/
 *        `timeToFrameIndex()` mapping actually used.
 */
void applyPasteOperation(const PasteOperation& operation, sound_mind::codec::StreamImage& content);

}  // namespace sound_mind::core
