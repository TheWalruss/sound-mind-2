#pragma once

#include <string>

namespace sound_mind::core {

/**
 * @brief The nearest standard 12-TET (equal-temperament) note name for a
 *        frequency, against a given tuning reference - e.g. `"A4"`,
 *        `"C#5"`.
 *
 * Used by the Frequency axis's own "Notes" label mode (see
 * `docs/sound-mind-design.md`'s "Axis Labels") and, later, the Overlay
 * Grid's own note grid - both need the same "which note is this Hz value
 * closest to" answer, against whichever tuning reference the project's
 * own `ProjectSettings::referenceHz` currently holds (440 Hz by
 * default - concert pitch - but a project can retune it to any
 * alternate historical/philosophical reference).
 *
 * The MIDI note-number convention (`69` = A4) still applies regardless
 * of `referenceHz`'s own value: retuning changes what frequency A4
 * *is*, not which note is nearest to a given frequency in semitone
 * terms.
 *
 * @param frequencyHz The frequency to name; must be positive.
 * @param referenceHz The tuning reference for A4, in Hz; must be
 *        positive.
 * @return The note name: a letter (`A`-`G`), an optional `#` (sharp),
 *         and a signed octave number (following the same convention
 *         where middle C is `C4`) - e.g. `"A4"`, `"C#5"`, `"D-1"`. An
 *         empty string if either argument isn't positive.
 */
[[nodiscard]] std::string noteNameForFrequency(double frequencyHz, double referenceHz) noexcept;

/**
 * @brief The frequency of a given MIDI note number, in 12-TET against a
 *        given tuning reference - the exact inverse of
 *        `noteNameForFrequency()`'s own semitone math (see its own docs for
 *        the shared `69` = A4 convention).
 *
 * Used by the Chord Generator (`chord_generator.h`) to resolve a chord's
 * own root note + octave, and each interval above it, to real Hz values -
 * the same "resolve to a plain Hz value before it ever reaches
 * `NoteEvent`" boundary `sequence_operation.h`'s own docs describe.
 *
 * @param midiNote The MIDI note number to resolve - `69` is A4, following
 *        the same convention `noteNameForFrequency()` uses; any integer is
 *        accepted, not clamped to MIDI's own nominal `[0, 127]` range,
 *        since a chord's own intervals can legitimately push a high root
 *        note's own notes past it.
 * @param referenceHz The tuning reference for A4, in Hz; must be positive.
 * @return `referenceHz * 2^((midiNote - 69) / 12)` - `0.0` if `referenceHz`
 *         isn't positive.
 */
[[nodiscard]] double frequencyForMidiNote(int midiNote, double referenceHz) noexcept;

}  // namespace sound_mind::core
