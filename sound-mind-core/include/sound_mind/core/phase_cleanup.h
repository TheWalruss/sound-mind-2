#pragma once

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

/**
 * @brief Zeroes `content`'s own shared phase channel wherever both the
 *        left and right magnitude channels are already at or below the
 *        silence floor (`-96`dB) - real-world testing pass finding #24.
 *
 * Imported audio's own silent stretches often decode with random,
 * meaningless phase data (an artifact of the encode round trip, not real
 * signal) - this reads as visual noise on the canvas despite having no
 * audible effect at all: a cell this quiet contributes nothing
 * perceptible to the decoded output regardless of what its own phase
 * happens to be, the same "silence has no phase worth keeping" reasoning
 * `silenceGradient()`'s own docs already establish for a *painted*
 * silence. A one-shot, in-place data-hygiene pass over a layer's own
 * already-decoded content - not an ongoing `Filter` layer, since it
 * corrects stored data once rather than applying a live, recomposited
 * effect - see `docs/sound-mind-architecture.md`'s own Decision for the
 * full reasoning.
 *
 * Every cell where either channel is *louder* than the floor is left
 * completely untouched, phase included - this never rewrites a cell with
 * any real, audible content.
 *
 * @param content The `StreamImage` to clean up, mutated in place.
 */
void applyPhaseCleanup(sound_mind::codec::StreamImage& content);

}  // namespace sound_mind::core
