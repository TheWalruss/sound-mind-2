#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

/**
 * @brief Which family of chord `chordsInCategory()` looks up - the same
 *        five-category grouping `docs/sound-mind-design.md`'s "Chords/
 *        Arpeggiator/Sequencer" section lists, ported from the legacy
 *        Python Studio's own `data/chords.json` (kept only as a "lessons
 *        learned" reference, per `CLAUDE.md` - not loaded from an external
 *        file here, unlike there: this codebase's own convention is a
 *        compiled-in static table, matching every other fixed-vocabulary
 *        lookup already in this module, e.g. `BrushTipShape`).
 */
enum class ChordCategory {
    Triads,
    Sixths,
    Sevenths,
    Ninths,
    ExtendedAdded,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(ChordCategory, {
    {ChordCategory::Triads, "triads"},
    {ChordCategory::Sixths, "sixths"},
    {ChordCategory::Sevenths, "sevenths"},
    {ChordCategory::Ninths, "ninths"},
    {ChordCategory::ExtendedAdded, "extendedAdded"},
})
// clang-format on

/**
 * @brief One named chord within a `ChordCategory` - a display name, a
 *        conventional chord-symbol suffix (e.g. `"m7"`, empty for a plain
 *        Major triad), and its own semitone intervals above the root
 *        (always starting with `0`, the root itself).
 */
struct ChordDefinition {
    /// @brief Display name, e.g. `"Minor 7th"`.
    std::string name;
    /// @brief Conventional chord-symbol suffix, e.g. `"m7"` - empty for a
    ///        plain Major triad.
    std::string symbol;
    /// @brief Semitone intervals above the root, always starting with `0`.
    std::vector<int> intervals;
};

/**
 * @brief Every chord defined within a given category, in a fixed, stable
 *        order (matching the legacy Studio's own `chords.json` ordering,
 *        for continuity with anyone already familiar with it) - `Triads`
 *        index `0` is always Major, per this module's own tests.
 * @param category The category to look up.
 * @return A reference to a static, compiled-in table - valid for the
 *         lifetime of the program; never null/empty for a real enum value.
 */
[[nodiscard]] const std::vector<ChordDefinition>& chordsInCategory(ChordCategory category);

/**
 * @brief Which order an arpeggio's own notes play in - the same nine
 *        presets the legacy Studio's own `arpeggiator.py` offered
 *        (`PRESET_NAMES`), ported here as a real enum rather than a string.
 *
 * `Random`'s own port is a deliberate behavior change from the legacy
 * implementation: `arpeggiator.py`'s own `build_sequence()` called
 * `random.shuffle()` directly, non-reproducibly - this codebase's own
 * established rule is that anything seeded replays bit-for-bit (the same
 * expectation `MindWave`'s own seeded generators already meet), so
 * `arpeggioIndexSequence()`'s own `Random` branch takes an explicit seed
 * and shuffles deterministically instead.
 */
enum class ArpeggioOrder {
    Ascending,
    Descending,
    UpDown,
    DownUp,
    Alternating,
    OutsideIn,
    InsideOut,
    Random,
    Custom,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(ArpeggioOrder, {
    {ArpeggioOrder::Ascending, "ascending"},
    {ArpeggioOrder::Descending, "descending"},
    {ArpeggioOrder::UpDown, "upDown"},
    {ArpeggioOrder::DownUp, "downUp"},
    {ArpeggioOrder::Alternating, "alternating"},
    {ArpeggioOrder::OutsideIn, "outsideIn"},
    {ArpeggioOrder::InsideOut, "insideOut"},
    {ArpeggioOrder::Random, "random"},
    {ArpeggioOrder::Custom, "custom"},
})
// clang-format on

/**
 * @brief The index sequence an arpeggio steps through for a chord of
 *        `noteCount` notes, in "definition order" (position `0` is the
 *        chord's own root, ascending from there) - see `buildChordNotes()`
 *        for how this is actually turned into timed notes.
 *
 * Each preset's own algorithm is a direct, faithful port of
 * `arpeggiator.py`'s own `_preset_*()` functions (see this module's own
 * tests for concrete worked examples) - `OutsideIn`/`InsideOut` in
 * particular sort by a stable "distance from the nearest edge" key exactly
 * as the legacy code did, which (despite the names) starts `OutsideIn` from
 * the chord's own middle and `InsideOut` from its own two edges; this is a
 * faithful port of that existing, already-named behavior, not a fresh
 * design independent of it.
 *
 * @param order Which preset (or `Custom`/`Random`) to build.
 * @param noteCount How many notes the chord being arpeggiated has; must be
 *        positive for a non-empty result.
 * @param customIndices Used only when `order` is `Custom` - returned
 *        verbatim (an index outside `[0, noteCount)` is silently kept here
 *        and skipped later by `buildChordNotes()`, matching
 *        `build_arpeggio_events()`'s own precedent); an ascending
 *        `0..noteCount-1` run if empty.
 * @param randomSeed Used only when `order` is `Random` - the same seed
 *        always reproduces the same shuffle (see this class's own docs on
 *        why, unlike the legacy Python implementation).
 * @return The index sequence, one arpeggio cycle long; empty if
 *         `noteCount` isn't positive.
 */
[[nodiscard]] std::vector<int> arpeggioIndexSequence(ArpeggioOrder order, int noteCount,
                                                       const std::vector<int>& customIndices,
                                                       std::uint64_t randomSeed);

/// @brief Whether a chord plays as one simultaneous stack, or spread out
///        over time as an arpeggio - see `ChordGeneratorParams::mode`.
enum class ChordPlaybackMode {
    Block,
    Arpeggio,
};

/**
 * @brief Every parameter `buildChordNotes()` needs to resolve a chosen
 *        chord, root, and (for `Arpeggio`) rhythm into real `NoteEvent`s -
 *        see `docs/sound-mind-design.md`'s "Chords/Arpeggiator/Sequencer",
 *        Installment B (the Chord Generator panel).
 *
 * **"Voicing" is just "which named chord within the category"** here -
 * confirmed with the user as this installment's own scope: no separate
 * inversion/octave-doubling axis, matching the legacy Studio's own only
 * granularity level (`chord_library.py`'s own `get_notes()` has no such
 * axis either).
 */
struct ChordGeneratorParams {
    /// @brief The chord's own root note, as an absolute MIDI note number
    ///        (root pitch class + octave already combined - e.g. middle C,
    ///        octave 4, is `60`) - see `frequencyForMidiNote()`'s own docs
    ///        for the `69` = A4 convention this follows.
    int rootMidiNote = 60;

    /// @brief Which category `chordIndex` is looked up within.
    ChordCategory category = ChordCategory::Triads;

    /// @brief Which chord within `chordsInCategory(category)` to play - an
    ///        out-of-range index makes `buildChordNotes()` return an empty
    ///        result rather than throwing (the same "nothing happens by
    ///        accident" default convention `ToolConfiguration`'s own
    ///        not-yet-configured subtypes follow).
    std::size_t chordIndex = 0;

    /// @brief The tuning reference for `rootMidiNote`, in Hz - see
    ///        `frequencyForMidiNote()`'s own docs; ordinarily a project's
    ///        own `ProjectSettings::referenceHz`.
    double referenceHz = 440.0;

    /// @brief When this chord starts, in seconds from the project's start -
    ///        the same field `NoteEvent::startTimeSeconds` uses; the Studio
    ///        layer resolves this from wherever the user clicked on the
    ///        canvas's own time axis (see `docs/sound-mind-architecture.md`'s
    ///        Decision on this installment for why only the time axis, not
    ///        frequency, comes from the click).
    double startTimeSeconds = 0.0;

    /// @brief Block (every note starts together) or Arpeggio (notes spread
    ///        out over time) - see each mode's own fields below.
    ChordPlaybackMode mode = ChordPlaybackMode::Block;

    /// @brief `Block` mode only - how long every note in the stack lasts.
    double blockDurationSeconds = 1.0;

    /// @brief `Arpeggio` mode only - which order the chord's own notes step
    ///        through; see `arpeggioIndexSequence()`.
    ArpeggioOrder order = ArpeggioOrder::Ascending;

    /// @brief `Arpeggio` mode only, used when `order` is `Custom` - see
    ///        `arpeggioIndexSequence()`'s own docs.
    std::vector<int> customOrderIndices;

    /// @brief `Arpeggio` mode only, used when `order` is `Random` - see
    ///        `arpeggioIndexSequence()`'s own docs.
    std::uint64_t randomSeed = 0;

    /// @brief `Arpeggio` mode only - the project's own tempo, in beats per
    ///        minute (ordinarily `ProjectSettings::defaultTempoBpm`).
    double bpm = 120.0;

    /// @brief `Arpeggio` mode only - how many beats apart consecutive
    ///        arpeggio steps land, e.g. `1.0` for quarter-note steps,
    ///        `0.25` for sixteenth-note steps - the same "beats per step"
    ///        convention `arpeggiator.py`'s own `_BEATS_PER_STEP` table
    ///        used (there keyed by a `"1/16"`-style subdivision label; the
    ///        Studio panel's own combo box maps its own label choice to
    ///        this value directly).
    double stepBeats = 0.25;

    /// @brief `Arpeggio` mode only - what fraction of one step's own
    ///        duration each note actually holds for, `(0, 1]` - `1.0`
    ///        lets consecutive notes touch exactly; below that leaves an
    ///        audible gap; not clamped or validated here.
    double noteDurationFraction = 0.8;

    /// @brief `Arpeggio` mode only - how many full cycles through
    ///        `arpeggioIndexSequence()`'s own sequence to play, back to
    ///        back - matching `build_arpeggio_events()`'s own `repeats`
    ///        parameter; a value below `1` is treated as `1` (see
    ///        `buildChordNotes()`'s own docs).
    int repeats = 1;
};

/**
 * @brief Resolves a `ChordGeneratorParams` into the concrete `NoteEvent`s
 *        it describes - the Chord Generator panel's own "Stamp" action
 *        builds these, then hands them to a fresh `SequenceOperation`
 *        (`sequence_operation.h`) to actually log/paint.
 *
 * Every note's own `frequencyHz` is resolved once, here, via
 * `frequencyForMidiNote(params.rootMidiNote + interval, params.referenceHz)`
 * for each of `chordsInCategory(params.category).at(params.chordIndex)`'s
 * own `intervals` - matching `NoteEvent`'s own documented boundary that
 * "whatever note-name-to-Hz mapping a future Chord Generator... uses
 * resolves to a plain Hz value before it ever reaches this struct."
 *
 * @param params The parameters to resolve.
 * @return One `NoteEvent` per note in `Block` mode; `params.repeats` full
 *         arpeggio cycles' worth of `NoteEvent`s in `Arpeggio` mode, each
 *         `params.stepBeats * 60/params.bpm` seconds after the previous
 *         one, starting at `params.startTimeSeconds`. Empty if
 *         `params.chordIndex` is out of range for `params.category`.
 */
[[nodiscard]] std::vector<NoteEvent> buildChordNotes(const ChordGeneratorParams& params);

}  // namespace sound_mind::core
