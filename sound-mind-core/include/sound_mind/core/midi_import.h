#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

/**
 * @brief Every note played on one MIDI channel across a whole Standard MIDI
 *        File, ready to become a `SequenceOperation` - `v0.Y.55.1`'s own
 *        MIDI-specific work, see `docs/sound-mind-design.md`'s "Import"
 *        ("each MIDI program/instrument in the file is mapped to a paint
 *        preset... every MIDI note becomes an individual paint event").
 *
 * **Grouped by MIDI channel, not by file track** - a deliberate, simpler
 * first pass: a Standard MIDI File's tracks and its 16 channels aren't the
 * same axis (a Type 0 file interleaves every channel in one track; a Type 1
 * file conventionally, but not necessarily, gives each channel its own
 * track), and channel is the one that actually determines "which instrument
 * plays this note" via Program Change events, which is what this milestone
 * needs to map notes onto a saved Tool Preset. Two tracks that both write to
 * the same channel are merged into one `MidiChannelNotes` entry here, with
 * `notes` sorted by `startTimeSeconds` regardless of which track each one
 * came from originally.
 */
struct MidiChannelNotes {
    /// @brief 1-16, matching `juce::MidiMessage::getChannel()`.
    int channelNumber = 1;

    /// @brief The General MIDI program (0-127) active on this channel -
    ///        whichever Program Change event on this channel came first in
    ///        the file, timestamp-wise, or `0` (Acoustic Grand Piano) if
    ///        none exists.
    ///
    /// **Fixed for the whole file, not re-detected per note** - a further
    /// deliberate simplification: a real-world MIDI file essentially never
    /// changes a channel's instrument mid-file, and letting it do so here
    /// would mean a single channel could need more than one Tool Preset
    /// mapping (`v0.Y.55.1`'s own MIDI Configuration panel installment maps
    /// one preset per channel/program, not per note) - revisit only if a
    /// real file surfaces this as an actual problem.
    int programNumber = 0;

    /// @brief A human-readable label for this channel - the General MIDI
    ///        instrument name for `programNumber`
    ///        (`juce::MidiMessage::getGMInstrumentName()`), e.g. "Acoustic
    ///        Grand Piano".
    std::string instrumentName;

    /// @brief Every note played on this channel, in ascending
    ///        `startTimeSeconds` order - see this struct's own docs on why
    ///        this can merge notes from more than one file track.
    std::vector<NoteEvent> notes;
};

/**
 * @brief Parses a Standard MIDI File (`.mid`/`.midi`) at `path` into one
 *        `MidiChannelNotes` per channel that actually has at least one
 *        note - see that struct's own docs for the channel-grouping
 *        rationale.
 *
 * Every note's `startTimeSeconds`/`durationSeconds` are real seconds from
 * the file's own start (`juce::MidiFile::convertTimestampTicksToSeconds()`,
 * which resolves tempo-map/time-signature meta-events itself - a file with
 * no explicit tempo event is read at the MIDI spec's own default, 120 BPM).
 * `frequencyHz` is `juce::MidiMessage::getMidiNoteInHertz()` at standard
 * concert pitch (A4 = 440 Hz) - matching `NoteEvent`'s own "arbitrary
 * frequencies, not just named pitches" representation exactly, so nothing
 * downstream needs to know these originated as MIDI note numbers at all.
 * A note whose own note-off is missing from the file is still included
 * (JUCE's own `readFrom(..., createMatchingNoteOffs=true)` synthesizes one
 * at the file's own end) rather than silently dropped.
 *
 * @param path Path to the `.mid`/`.midi` file to parse.
 * @param errorMessage If non-null and this returns empty, set to a
 *        human-readable description of what went wrong.
 * @return One entry per channel with at least one note, ascending by
 *         `channelNumber`; empty if the file couldn't be opened, isn't a
 *         valid MIDI file, or has no notes at all.
 */
[[nodiscard]] std::vector<MidiChannelNotes> parseMidiFile(const std::filesystem::path& path,
                                                           std::string* errorMessage = nullptr);

}  // namespace sound_mind::core
