#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/fill_operation.h"

namespace sound_mind::core {

/**
 * @brief Applies a `FillOperation`'s own color (or gradient) directly onto
 *        a `StreamImage`'s amplitude planes, in place - see
 *        `docs/sound-mind-design.md`'s "Fill": "confined exactly to the
 *        selection's boundary".
 *
 * Every bin/frame cell strictly within `operation.bounds()` blends toward
 * `operation.gradient()`'s target intensity, the same per-pixel blend
 * `applyPaintOperation()` uses (`newDb = oldDb + (targetDb - oldDb) *
 * opacity`) - but with no falloff or stamp shape at all: a fill is a hard-
 * edged solid write across its whole selection, not a brush stamped
 * repeatedly along a path.
 *
 * The gradient's own `t=0` (start) to `t=1` (end) runs left-to-right
 * across the selection's own time axis - `docs/sound-mind-design.md`
 * doesn't specify a fill gradient's direction, and this is the most
 * visually intuitive default (see `docs/sound-mind-architecture.md`'s
 * Decisions Made for the full rationale).
 *
 * `operation.bounds()` extending past `content`'s own frame/bin range is
 * *clamped*, not dropped: the fill's own footprint stops at the nearest
 * valid edge and still paints that edge row/column, rather than skipping
 * the overhanging portion entirely - a real, if narrower, contribution
 * along the boundary. `applyPasteOperation()`'s own out-of-range handling
 * is genuinely different (drops the overhanging cells outright) - see its
 * own docs for why the two operations differ here.
 *
 * @param operation The fill to apply - its own `bounds()`/`gradient()`
 *        fully describe it.
 * @param content The `StreamImage` to fill into, mutated in place - its
 *        own `config` supplies the `frequencyToBinIndex()`/
 *        `timeToFrameIndex()` mapping actually used (both from
 *        `paint_application.h`, reused rather than re-derived).
 */
void applyFillOperation(const FillOperation& operation, sound_mind::codec::StreamImage& content);

}  // namespace sound_mind::core
