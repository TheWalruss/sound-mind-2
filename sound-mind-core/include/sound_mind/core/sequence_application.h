#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

/**
 * @brief Applies a `SequenceOperation` to a `StreamImage`, in place - stamps
 *        every one of `operation.notes()` through `operation.config()`,
 *        reusing `applyPaintOperation()`'s own existing per-tool-type
 *        rendering entirely rather than duplicating it. `v0.Y.40.1`
 *        Installment A.
 *
 * **Each note becomes its own tiny stroke**: a note with `durationSeconds()`
 * at or below `0` becomes a single-tap `Path` (one node, at
 * `(startTimeSeconds, frequencyHz)`) - matching a "block chord" note
 * struck and released at once. A note with a positive duration becomes a
 * straight, horizontal two-node `Path` from `(startTimeSeconds,
 * frequencyHz)` to `(startTimeSeconds + durationSeconds, frequencyHz)` -
 * a held tone at a fixed pitch, the natural spectrogram-domain shape for
 * "the same note, sustained." Both cases reuse `operation.config()`'s own
 * `defaultGradient()` for the stroke's own color/opacity, the same
 * gradient a live-painted stroke with that same configuration would use -
 * every note in one sequence shares it, matching `NoteEvent`'s own docs on
 * why a note carries no per-note color of its own.
 *
 * **Renders through whichever tool type `operation.config()` actually is**
 * (`docs/sound-mind-design.md`'s "anything paintable can be the
 * instrument a sequence plays through") - a plain `ProceduralConfiguration`
 * stamp, a `InstrumentConfiguration` note (vibrato/tremolo and all),
 * a `MindShotConfiguration`/`MindGrainConfiguration` stamp, or any future
 * paintable tip - `applyPaintOperation()`'s own existing dispatch already
 * handles every one of these identically to a real user-painted stroke, so
 * this function needs no per-tool-type logic of its own at all. A Mind
 * Shot/Mind Grain note's own `durationSeconds` still produces a two-node
 * path internally, the same as any other type - since those tool types
 * ignore a stroke's own length/falloff already (see their own docs),
 * `StampMode::Stroke`'s default dense sampling along that path can produce
 * several overlapping stamps for a held Mind Shot/Mind Grain note, exactly
 * matching how that tool type already behaves for any other multi-sample
 * stroke - not specially collapsed to one stamp here.
 *
 * @param operation The sequence to apply - its own `notes()`/`config()`
 *        fully describe it.
 * @param frequencyToTimeScale Passed through to `applyPaintOperation()`
 *        for each note's own stroke - see its own docs; must be positive.
 * @param content The `StreamImage` to paint into, mutated in place.
 * @param resolveLayerContent Passed through to `applyPaintOperation()` for
 *        each note - only consulted when `operation.config()` is a
 *        `MindGrainConfiguration`; see its own docs.
 * @param resolveMindWave Passed through to `applyPaintOperation()` for
 *        each note - only consulted when `operation.config()` is an
 *        `InstrumentConfiguration`; see its own docs.
 */
void applySequenceOperation(const SequenceOperation& operation, double frequencyToTimeScale,
                             sound_mind::codec::StreamImage& content,
                             const LayerContentResolver& resolveLayerContent = {},
                             const MindWaveResolver& resolveMindWave = {});

}  // namespace sound_mind::core
