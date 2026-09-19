#pragma once

#include <string>
#include <vector>

#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

/**
 * @brief Parses Sound Mind's own compact, ABC-inspired sequence notation
 *        into real `NoteEvent`s - `docs/sound-mind-design.md`'s
 *        "Chords/Arpeggiator/Sequencer" ("the same underlying notation
 *        lets a sequence be written directly - any series of notes or
 *        frequencies with their own timing"), Installment C.
 *
 * **Grammar** (whitespace-separated tokens, in order):
 * - A **note**: `<pitch>:<duration>`.
 *   - `<pitch>` is either a note name (`A`-`G`, an optional `#`, then a
 *     signed integer octave - the same `69 = A4` convention
 *     `frequencyForMidiNote()` uses, e.g. `A4`, `C#5`, `D-1`) or a bare
 *     positive number (a raw Hz value, e.g. `440`) - unambiguous by its
 *     first character, since a note name always starts with a letter
 *     `A`-`G` and an Hz value always starts with a digit or `.`.
 *   - `<duration>` is a positive number, optionally suffixed `b` for
 *     beats (converted via `(60/bpm) * value`, the same "beats" unit
 *     `ChordGeneratorParams::stepBeats` already uses) - no suffix means
 *     plain seconds.
 * - A **rest**: `z<duration>` (same `<duration>` grammar as above) -
 *     advances the timeline without emitting a note, ABC's own convention
 *     for silence (its only other reserved letter beyond `A`-`G`, so this
 *     stays unambiguous too).
 * - A **chord** (simultaneous notes): two or more notes joined by `+`,
 *     e.g. `A4:0.5+C#5:0.5+E5:0.5` - every note in the group starts at the
 *     same time; each may have its own independent duration.
 *
 * **Timing is sequential, not explicit** - the first token starts at
 * `startTimeSeconds`; each following token starts exactly when the
 * previous one's own *longest* note (or, for a rest, its own duration)
 * finishes. There is no way to express deliberate overlap between two
 * non-simultaneous tokens (no explicit `@time` syntax) - a rest is the
 * only way to leave a gap, and a `+`-joined chord the only way to overlap
 * notes - a deliberate simplification (confirmed with the user alongside
 * the grammar's own overall style): it keeps every token's own start time
 * a pure function of everything before it, with nothing to keep
 * consistent by hand, the same "sequential, unless grouped" shape a
 * written arpeggio already has.
 *
 * @param notation The notation text to parse.
 * @param referenceHz The tuning reference for note names, in Hz - see
 *        `frequencyForMidiNote()`'s own docs; ordinarily a project's own
 *        `ProjectSettings::referenceHz`.
 * @param bpm The tempo beats-suffixed durations are resolved against;
 *        ordinarily a project's own `ProjectSettings::defaultTempoBpm`.
 * @param startTimeSeconds When the first token starts, in seconds -
 *        ordinarily wherever the user clicked on the canvas's own time
 *        axis, the same role `ChordGeneratorParams::startTimeSeconds`
 *        plays for a generated chord.
 * @return The parsed notes, in the order they appear in `notation`.
 * @throws std::invalid_argument if `notation` doesn't match the grammar
 *         above - the message names the offending token and why.
 */
[[nodiscard]] std::vector<NoteEvent> parseSequenceNotation(const std::string& notation, double referenceHz,
                                                             double bpm, double startTimeSeconds = 0.0);

/**
 * @brief The inverse of `parseSequenceNotation()` - renders `notes` back
 *        into notation text.
 *
 * **Always raw Hz, always plain seconds** - never a note name, never a
 * beats-suffixed duration, even if `notes` originally came from parsing
 * text that used either: a `NoteEvent`'s own `frequencyHz` is exact, but
 * "the nearest note name" is lossy (see `noteNameForFrequency()`'s own
 * docs), and a `NoteEvent`'s own `durationSeconds` has already lost
 * whatever `bpm` its original beats value (if any) was resolved against -
 * rendering both canonically, rather than guessing which form round-trips
 * "close enough", keeps this function's own output exact and independent
 * of any particular `bpm`. Parsing the result back with any `bpm`/
 * `referenceHz` therefore always reproduces the same `NoteEvent`s exactly
 * (see this module's own round-trip tests) - only the *text* itself
 * doesn't necessarily match what a human originally typed.
 *
 * Consecutive notes sharing the exact same `startTimeSeconds` are
 * rendered as one `+`-joined chord token, in `notes`' own order; a gap
 * between one token's own end and the next token's own start is rendered
 * as an explicit `z<duration>` rest, so the result always parses back to
 * the same absolute timing `notes` describes, offset so the first token
 * starts at `notes`' own earliest `startTimeSeconds` (not necessarily
 * `0.0`).
 *
 * @param notes The notes to render, in any order - sorted by
 *        `startTimeSeconds` internally before rendering.
 * @return The rendered notation text; an empty string for an empty
 *         `notes`.
 */
[[nodiscard]] std::string sequenceNotationFor(const std::vector<NoteEvent>& notes);

}  // namespace sound_mind::core
