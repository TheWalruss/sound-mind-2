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

}  // namespace sound_mind::core
