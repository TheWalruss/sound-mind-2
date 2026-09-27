#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "sound_mind/core/tool_preset.h"

namespace sound_mind::core {

/**
 * @brief How one General MIDI program should paint when imported -
 *        `v0.Y.55.1`'s own MIDI Configuration panel installment, see
 *        `docs/sound-mind-design.md`'s "Import" ("A new 'MIDI
 *        configuration' panel allows the user to map MIDI programs to
 *        saved/named Paint Tool configurations... duration, pitch,
 *        strength, etc can modify the paint op in different ways").
 *
 * Keyed by `programNumber` (a General MIDI program number, 0-127) rather
 * than an auto-assigned id like every other "peer resource library" in
 * this codebase (`NamedMindShot`/`NamedToolPreset`/etc.) - the program
 * number itself is already the natural, stable, project-wide-meaningful
 * key: unlike a Mind Shot or a Tool Preset, there's nothing else about a
 * mapping worth identifying it by, and reusing it directly means a mapping
 * saved from one MIDI import is automatically picked up by any *other*
 * MIDI file whose own tracks use the same GM program, without the user
 * re-mapping it - the "map once, reuse across every future import"
 * behavior the design doc's own project-level framing implies.
 *
 * **No `strength`/velocity field** - deliberately, matching `NoteEvent`'s
 * own docs: velocity has nothing in this codebase's own paint model to
 * attach to yet (no per-note intensity of any kind), so the design doc's
 * own "duration, pitch, strength" list is only two-thirds implemented
 * here, not silently dropped.
 */
struct MidiProgramMapping {
    /// @brief The General MIDI program this mapping applies to, 0-127.
    int programNumber = 0;

    /// @brief Which of the project's own saved Tool Presets a note on
    ///        this program paints through - `std::nullopt` (the default)
    ///        means "no mapping yet," painting through the same plain,
    ///        opaque default `ProceduralConfiguration` every unmapped
    ///        program already used before this installment existed. An id
    ///        that no longer resolves (its own `NamedToolPreset` was
    ///        removed from the project) falls back to that same default
    ///        too, rather than failing the import - the same "a stale
    ///        reference degrades gracefully" precedent
    ///        `Layer::opacityMindWave()`'s own docs already establish.
    std::optional<ToolPresetId> toolPresetId;

    /// @brief Multiplies each note's own `durationSeconds` before it's
    ///        painted - `1.0` (the default) leaves duration unchanged.
    double durationScale = 1.0;

    /// @brief Shifts each note's own `frequencyHz` by this many
    ///        semitones (equal temperament: `frequencyHz *= 2^(semitones /
    ///        12)`) before it's painted - `0.0` (the default) leaves pitch
    ///        unchanged.
    double pitchOffsetSemitones = 0.0;
};

/// @brief Serializes a MIDI program mapping to its JSON representation -
///        `toolPresetId` is omitted entirely when `std::nullopt`, the same
///        convention `Layer::opacityMindWave()`'s own serialization uses.
void to_json(nlohmann::json& json, const MidiProgramMapping& mapping);

/// @brief Parses a MIDI program mapping from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, MidiProgramMapping& mapping);

}  // namespace sound_mind::core
