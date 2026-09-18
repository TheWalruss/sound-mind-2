#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/warp_operation.h"

namespace sound_mind::core {

/**
 * @brief Applies a `WarpOperation` to a `StreamImage`'s amplitude/phase
 *        planes, in place - ported directly from the legacy Python
 *        Studio's own `WarpTool`/`_apply_warp_frequency()`/
 *        `_apply_warp_time()` (`packages/sound_mind_studio/src/
 *        sound_mind_studio/tools/warp_pick_tools.py`), confirmed with
 *        the user against that implementation.
 *
 * **The deflection, per column (`WarpAxis::Frequency`) or row
 * (`WarpAxis::Time`)**: the curve is densely sampled, then binned by its
 * own nearest integer frame (Frequency axis) or bin (Time axis) index.
 * For a covered frame/bin, the deflection is `curveValue - curveStartValue`
 * (in bins for Frequency axis, in frames for Time axis - the curve's own
 * *first* node is the baseline) - when the curve crosses the same frame/
 * bin more than once, the crossing with the smallest absolute deflection
 * is kept. A frame/bin the curve never reaches gets zero deflection
 * (untouched).
 *
 * **`WarpMode::Stretch`** scales every column's (or row's) own deflection
 * by that column's (row's) own position across `operation.bounds()`'s own
 * span - `0` at the first column/row, `1` at the last, linear in between
 * (a single-column/row bounding box gets `0` everywhere, matching
 * `numpy.linspace(0, 1, 1)`'s own `[0.0]` the legacy code relies on).
 * `WarpMode::Displace` applies every column's/row's own deflection at
 * full strength, unscaled.
 *
 * **The shift itself**, per affected column/row: every cell within
 * `operation.bounds()`'s own span along that column/row is displaced by
 * its own (possibly fractional) deflection via linear interpolation
 * between the two nearest source cells - the same "read from an
 * unmodified snapshot, write to a separate result" correctness precedent
 * every other multi-cell edit in this codebase already establishes (no
 * scanline-order bias). A destination cell that receives no inbound
 * content from the original span (because it shifted away entirely) is
 * left at the display-range silence floor for the amplitude planes
 * (`-96dB`, this codebase's own established "erased" convention) and
 * `0` radians for phase (arbitrary but harmless - an amplitude-silent
 * cell's own phase is inaudible either way). A shifted cell can land
 * *outside* `operation.bounds()`'s own span entirely, overwriting
 * whatever content was already there ("bleeding" into the surrounding,
 * unselected area) - the same behavior the legacy tool's own docstring
 * describes, kept deliberately rather than clamped, since clamping would
 * silently lose content a real warp is supposed to actually move.
 *
 * @param operation The warp to apply - its own `bounds()`/`curve()`/
 *        `axis()`/`mode()` fully describe it.
 * @param content The `StreamImage` to warp, mutated in place.
 */
void applyWarpOperation(const WarpOperation& operation, sound_mind::codec::StreamImage& content);

}  // namespace sound_mind::core
