#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

/**
 * @brief The scientific-pitch-notation octave number a frequency falls
 *        into (octave `N` spans `[C_N, C_(N+1))`) - e.g. middle C (`C4`)
 *        and the B just below the next C both report `4`.
 *
 * A different calculation from `noteNameForFrequency()`'s own: that
 * function rounds an arbitrary frequency to its *nearest* note name
 * first (appropriate for labeling a point that may not sit exactly on a
 * note); this one buckets a frequency by raw ratio to `C0` with no
 * rounding at all, appropriate instead for a frequency that's already
 * known to sit exactly on a grid/scale step (the Note Grid's own Octave
 * filter, `grid_config.h`) and just needs placing into the right octave
 * band.
 *
 * @param frequencyHz The frequency to bucket; must be positive.
 * @param referenceHz The tuning reference for A4, in Hz (the same anchor
 *        `noteNameForFrequency()`/`frequencyForMidiNote()` use); must be
 *        positive.
 * @return The octave number; `0` if either argument isn't positive (a
 *         degenerate input, not meant to be distinguished from a
 *         genuinely-computed octave `0`).
 */
[[nodiscard]] int octaveNumberForFrequency(double frequencyHz, double referenceHz) noexcept;

/**
 * @brief A pitch class within one 12-tone octave - the same `C`-through-`B`
 *        letter-name set `noteNameForFrequency()` already uses, given its
 *        own enumerators so the Note Grid's Key selector
 *        (`grid_config.h`) has something stronger than a bare `int` to
 *        work with.
 *
 * Deliberately independent of any particular `Temperament` below - a
 * pitch class is a letter name (`"C"`, `"C#"`, ...), not a frequency;
 * `stepWithinOctaveForPitchClass()` is what resolves one against a
 * specific tuning system.
 */
enum class PitchClass {
    C = 0,
    CSharp,
    D,
    DSharp,
    E,
    F,
    FSharp,
    G,
    GSharp,
    A,
    ASharp,
    B,
};

/// @brief `pitchClass`'s own display name (`"C"`, `"C#"`, ... `"B"`) - the
///        same letter names `noteNameForFrequency()`'s own note names
///        start with.
[[nodiscard]] std::string pitchClassName(PitchClass pitchClass) noexcept;

/**
 * @brief A scale/mode - a fixed pattern of scale degrees above a root,
 *        for the Note Grid's own Scale filter (`grid_config.h`,
 *        `docs/sound-mind-design.md`'s "Overlay Grids").
 *
 * **A curated common-practice subset, not the full list at
 * https://en.wikipedia.org/wiki/List_of_musical_scales_and_modes** (which
 * runs to several hundred entries, most highly specialized) - confirmed
 * with the user: the seven diatonic modes, the two non-natural minor
 * forms, the two most common pentatonic scales, Blues, Whole Tone, and
 * Octatonic/Diminished, plus `Chromatic` itself (every pitch class - the
 * "no scale restriction" choice). More can be added later if a specific
 * one is wanted.
 */
enum class ScaleType {
    /// @brief Every pitch class - the "no restriction" choice; ignores
    ///        whatever Key is selected (every root sounds identical for a
    ///        scale with no missing degrees).
    Chromatic,
    /// @brief The major scale (Ionian mode).
    Major,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    /// @brief The natural minor scale (Aeolian mode).
    Minor,
    Locrian,
    HarmonicMinor,
    /// @brief The melodic minor scale's own ascending form (the same
    ///        seven degrees used both ways is a common simplification -
    ///        the classical "different descending form" isn't modeled
    ///        separately here).
    MelodicMinor,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
    WholeTone,
    /// @brief The octatonic (diminished) scale, whole-half form.
    Octatonic,
};

/// @brief `scale`'s own display name (`"Major (Ionian)"`,
///        `"Minor (Aeolian)"`, ...).
[[nodiscard]] std::string scaleTypeName(ScaleType scale) noexcept;

/// @brief `scale`'s own semitone offsets above its root, within one
///        12-tone octave - always starts with `0` (the root itself),
///        strictly ascending, every value in `[0, 11]`.
/// @return The offsets; `Chromatic`'s own is all twelve, `{0, 1, ..., 11}`.
[[nodiscard]] const std::vector<int>& scaleSemitoneOffsets(ScaleType scale) noexcept;

/// @brief Every pitch class `scale` includes when rooted at `key` -
///        `scaleSemitoneOffsets(scale)` transposed by `key` and wrapped
///        within the octave.
/// @param key The scale's own root.
/// @param scale Which interval pattern to apply - see `ScaleType`'s own
///        docs; `Chromatic` returns every pitch class regardless of `key`.
/// @return The included pitch classes, in ascending scale-degree order
///         (not necessarily ascending `PitchClass` numeric order, since
///         the root can be any of the twelve).
[[nodiscard]] std::vector<PitchClass> pitchClassesInScale(PitchClass key, ScaleType scale);

/**
 * @brief A tuning system for the Note Grid - `docs/sound-mind-design.md`'s
 *        "Overlay Grids" > "Frequency Grid", extended with alternate
 *        temperaments beyond the default 12-TET.
 *
 * **Two families, with different implications for Key/Scale filtering**
 * (see `supportsKeyAndScale()`'s own docs):
 * - **Equal temperaments** (`Equal12` through `Equal72`): `N` equal
 *   divisions of the octave. `Equal12` is standard 12-TET (this
 *   codebase's own long-standing default, unchanged); `Equal24` is
 *   quarter-tones (every `Equal12` pitch, plus one quarter-tone pitch
 *   between each pair). The rest (`Equal15`/`17`/`19`/`22`/`31`/`34`/`41`/
 *   `53`/`72`) have no standard mapping onto the familiar `C`-through-`B`
 *   letter names at all - there is no universally agreed "which of these
 *   N pitches is called C" the way there is for 12 or 24.
 * - **Named historical tunings** (`QuarterCommaMeantone`, `Pythagorean`):
 *   still exactly 12 pitch classes per octave, using the same letter
 *   names as `Equal12` - just spaced differently (unequal step sizes,
 *   tempered for purer thirds or fifths respectively) rather than every
 *   semitone being an identical ratio.
 */
enum class Temperament {
    Equal12,
    Equal15,
    Equal17,
    Equal19,
    Equal22,
    Equal24,
    Equal31,
    Equal34,
    Equal41,
    Equal53,
    Equal72,
    QuarterCommaMeantone,
    Pythagorean,
};

/// @brief `temperament`'s own display name (`"12-TET"`,
///        `"Quarter-Comma Meantone"`, `"Pythagorean"`, ...).
[[nodiscard]] std::string temperamentName(Temperament temperament) noexcept;

/// @brief How many equal-tempered divisions of the octave `temperament`
///        uses for step-indexing purposes - its own `N` for every equal
///        temperament, and `12` for both named historical tunings (see
///        `Temperament`'s own docs on why those still have 12 pitch
///        classes per octave despite not being *equally* spaced).
[[nodiscard]] int stepsPerOctave(Temperament temperament) noexcept;

/**
 * @brief Whether `temperament` has a standard 12-pitch-class-per-octave
 *        structure for a Key/Scale filter to mean anything against - see
 *        `Temperament`'s own docs.
 * @return `true` for `Equal12`, `Equal24`, `QuarterCommaMeantone`, and
 *         `Pythagorean`; `false` for every other equal temperament, which
 *         instead exposes its own plain numbered steps with no Key/Scale
 *         filtering available (confirmed with the user).
 */
[[nodiscard]] bool supportsKeyAndScale(Temperament temperament) noexcept;

/**
 * @brief The frequency of scale step `step` under `temperament`, against
 *        `referenceHz` - `frequencyForMidiNote()`'s own `69 = A4`
 *        convention generalized to an arbitrary tuning system, except
 *        re-anchored so `step == 0` (not `69`) is the reference pitch
 *        itself, for every temperament uniformly.
 *
 * For an equal temperament: `referenceHz * 2^(step / stepsPerOctave(temperament))` -
 * `Equal12`'s own result exactly matches `frequencyForMidiNote(step + 69,
 * referenceHz)` (see this function's own tests), just indexed from `0`
 * instead of `69`.
 *
 * For `QuarterCommaMeantone`/`Pythagorean` (12 pitch classes per octave,
 * unequally spaced): `step` is split into an octave (`floor(step / 12)`)
 * and a pitch-class-within-octave (`step` reduced into `[0, 12)`), and
 * the pitch class's own fixed cents offset from the reference (a chain-
 * of-fifths derivation, tabulated once - see `music_theory.cpp`'s own
 * comments for the exact values and how they were derived) is applied on
 * top of the octave's own `2^octave` multiplier.
 *
 * @param temperament Which tuning system to resolve `step` against.
 * @param step The scale step to resolve; `0` is `referenceHz` itself.
 * @param referenceHz The tuning reference, in Hz; must be positive.
 * @return The resolved frequency; `0.0` if `referenceHz` isn't positive.
 */
[[nodiscard]] double frequencyForTemperamentStep(Temperament temperament, int step, double referenceHz) noexcept;

/**
 * @brief The step-within-octave index (`[0, stepsPerOctave(temperament))`)
 *        that `pitchClass` maps onto under `temperament` - only
 *        meaningful while `supportsKeyAndScale(temperament)` is `true`.
 *
 * `Equal12`/`QuarterCommaMeantone`/`Pythagorean` (12 steps/octave): a
 * direct remapping from `PitchClass`'s own `C = 0` indexing to this
 * function's own `step == 0 is the reference pitch (A)` indexing.
 * `Equal24` (24 steps/octave): the same remapping, doubled - every
 * standard pitch class lands on an even step, leaving the 12 odd
 * (quarter-tone) steps unreachable by a Key/Scale filter, which is
 * exactly right: a quarter-tone pitch has no letter name of its own to
 * belong to a scale in the first place.
 *
 * @param temperament Which tuning system to resolve `pitchClass` against -
 *        must satisfy `supportsKeyAndScale()`.
 * @param pitchClass The pitch class to resolve.
 * @return The step-within-octave index.
 */
[[nodiscard]] int stepWithinOctaveForPitchClass(Temperament temperament, PitchClass pitchClass) noexcept;

/// @brief JSON (de)serialization for `PitchClass` - by name (`"C"`,
/// `"CSharp"`, ...), not ordinal, so a saved value stays meaningful (and
/// tolerant of enum reordering) across versions. First needed by
/// `docs/sound-mind-architecture.md`'s Grid Preset decision - the Note
/// Grid's own Key selector had never been serialized before (the Frequency
/// Grid itself was session-only Studio state, never persisted).
NLOHMANN_JSON_SERIALIZE_ENUM(PitchClass, {
                                              {PitchClass::C, "C"},
                                              {PitchClass::CSharp, "CSharp"},
                                              {PitchClass::D, "D"},
                                              {PitchClass::DSharp, "DSharp"},
                                              {PitchClass::E, "E"},
                                              {PitchClass::F, "F"},
                                              {PitchClass::FSharp, "FSharp"},
                                              {PitchClass::G, "G"},
                                              {PitchClass::GSharp, "GSharp"},
                                              {PitchClass::A, "A"},
                                              {PitchClass::ASharp, "ASharp"},
                                              {PitchClass::B, "B"},
                                          })

/// @brief JSON (de)serialization for `ScaleType` - by name, same reasoning
/// as `PitchClass`'s own.
NLOHMANN_JSON_SERIALIZE_ENUM(ScaleType, {
                                             {ScaleType::Chromatic, "Chromatic"},
                                             {ScaleType::Major, "Major"},
                                             {ScaleType::Dorian, "Dorian"},
                                             {ScaleType::Phrygian, "Phrygian"},
                                             {ScaleType::Lydian, "Lydian"},
                                             {ScaleType::Mixolydian, "Mixolydian"},
                                             {ScaleType::Minor, "Minor"},
                                             {ScaleType::Locrian, "Locrian"},
                                             {ScaleType::HarmonicMinor, "HarmonicMinor"},
                                             {ScaleType::MelodicMinor, "MelodicMinor"},
                                             {ScaleType::MajorPentatonic, "MajorPentatonic"},
                                             {ScaleType::MinorPentatonic, "MinorPentatonic"},
                                             {ScaleType::Blues, "Blues"},
                                             {ScaleType::WholeTone, "WholeTone"},
                                             {ScaleType::Octatonic, "Octatonic"},
                                         })

/// @brief JSON (de)serialization for `Temperament` - by name, same
/// reasoning as `PitchClass`'s own.
NLOHMANN_JSON_SERIALIZE_ENUM(Temperament, {
                                               {Temperament::Equal12, "Equal12"},
                                               {Temperament::Equal15, "Equal15"},
                                               {Temperament::Equal17, "Equal17"},
                                               {Temperament::Equal19, "Equal19"},
                                               {Temperament::Equal22, "Equal22"},
                                               {Temperament::Equal24, "Equal24"},
                                               {Temperament::Equal31, "Equal31"},
                                               {Temperament::Equal34, "Equal34"},
                                               {Temperament::Equal41, "Equal41"},
                                               {Temperament::Equal53, "Equal53"},
                                               {Temperament::Equal72, "Equal72"},
                                               {Temperament::QuarterCommaMeantone, "QuarterCommaMeantone"},
                                               {Temperament::Pythagorean, "Pythagorean"},
                                           })

}  // namespace sound_mind::core
