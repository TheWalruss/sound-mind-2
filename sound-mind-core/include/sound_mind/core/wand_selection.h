#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/selection_region.h"

namespace sound_mind::core {

/**
 * @brief Flood-fill-selects connected cells by amplitude similarity from
 *        `anchor`, optionally extending the selection along the anchor's
 *        own harmonic (overtone) rows - the actual work behind Wand
 *        (`docs/sound-mind-design.md`'s "Wand", `v0.Y.35.1` Installment
 *        B).
 *
 * **Contiguous flood fill, compared against the *anchor* cell's own
 * amplitude** (not a running average of the fill so far, and not the
 * immediately-preceding neighbor's own value) - the classic, simplest
 * "magic wand" semantics: a cell joins the selection (4-connected -
 * adjacent in either the frame or the bin axis, not diagonally) if its
 * own average-of-left/right-channel dB value is within `tolerancePercent`
 * of the anchor cell's own, and every cell between it and the anchor is
 * reachable the same way.
 *
 * @param content The layer content to select within.
 * @param anchor The clicked point, in time/frequency space.
 * @param tolerancePercent How similar a neighboring cell's own amplitude
 *        must be to join the fill, as a percentage (`0`-`100`) of this
 *        codebase's own `-96..0` dB display range (`0%` only ever selects
 *        cells at *exactly* the anchor's own value; `100%` selects every
 *        reachable cell regardless of amplitude).
 * @param harmonicsAware When `true`, after the anchor's own fill
 *        completes, the same tolerance-based flood fill re-runs
 *        independently at each harmonic multiple of the anchor's own
 *        frequency (`2x`, `3x`, ... up to Nyquist/the project's own
 *        maximum frequency) - starting from the *same* time position, and
 *        using each harmonic's own starting cell as *its own* reference
 *        value, not the anchor's - skipping any harmonic whose own
 *        starting cell is at or below the silence floor (nothing there to
 *        extend the selection to). Every resulting blob (the anchor's own
 *        plus every harmonic's) unions together into one final selection.
 * @return The selected region, as a `SelectionRegion` mask spanning the
 *         tight bounding box of every selected cell - a degenerate,
 *         empty region (`SelectionRegionKind::Mask`, an empty range) if
 *         `content` has no content at all, or `anchor` falls outside it.
 */
[[nodiscard]] SelectionRegion selectByAmplitudeSimilarity(const sound_mind::codec::StreamImage& content,
                                                            TimeFrequencyPoint anchor, double tolerancePercent,
                                                            bool harmonicsAware);

}  // namespace sound_mind::core
