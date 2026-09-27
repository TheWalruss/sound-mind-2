#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QString>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/midi_import.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"

namespace sound_mind::studio {

/**
 * @brief Imports every note-bearing channel of the Standard MIDI File at
 *        `path` into `project` as one or more new, non-destructive layers,
 *        with no chopping and no channel selection - the "nothing to
 *        choose" fast path `MainWindow::importMidi()` uses when a file has
 *        exactly one channel and fits in one project-length snippet (see
 *        `midiImportPreviewForFile()`'s own docs); `importMidiSelectionInto()`
 *        is the general, real-choice-supporting alternative Installment B
 *        added alongside this one. See `docs/sound-mind-design.md`'s
 *        "Import" ("each MIDI program/instrument in the file is mapped to
 *        a paint preset... every MIDI note becomes an individual paint
 *        event").
 *
 * Each channel's own notes (`sound_mind::core::parseMidiFile()`) become a
 * single `sound_mind::core::SequenceOperation` appended to `project`'s own
 * `OperationLog` - not baked pixels, matching the design doc's own "the
 * imported layer is therefore fully editable afterward" requirement (see
 * `SequenceOperation`'s own docs on why this makes a note re-timeable/re-
 * voiceable without repainting). **Deliberately does not itself render any
 * pixels** - a freshly added layer is left with no `content()` at all, the
 * same "content synthesized lazily on first rebuild" contract every other
 * content-less layer already has; the caller is responsible for triggering
 * that rebuild afterward (`ToolPaletteController::rebuildLayerContent()`,
 * in `MainWindow::importMidiFile()`'s own case) - this function has no
 * `PaintController` to call it with itself, matching this file's own
 * `import_export.cpp` counterpart's existing "pure `Project&` mutation, no
 * UI/rendering concerns" boundary.
 *
 * **Every channel plays through a plain, default `ProceduralConfiguration`**
 * (a fresh instance per channel, since each `SequenceOperation` owns its
 * own `ToolConfiguration` - see that class's own docs) - mapping a
 * channel's own `programNumber` to one of `project`'s own saved Tool
 * Presets is the still-unbuilt "MIDI Configuration panel" installment's
 * job, same as `importMidiSelectionInto()`.
 *
 * @param project The project to import into.
 * @param path Path to the `.mid`/`.midi` file to import.
 * @param separateLayerPerChannel When `true` (the default), each channel
 *        becomes its own new layer, named "`<stem>` - Ch`<n>` (`<GM
 *        instrument name>`)". When `false`, every channel's own
 *        `SequenceOperation` targets one single new layer instead, named
 *        plainly from the file's own stem - `OperationLog::
 *        activeOperationsTargeting()` already folds every operation
 *        targeting one layer together on rebuild, so this needs no special
 *        handling beyond appending each channel's operation to the same
 *        layer id.
 * @param errorMessage If non-null and this returns empty, set to a
 *        human-readable description of what went wrong.
 * @param channelNumbers Which channels to include, by `channelNumber` -
 *        empty (the default) means every channel the file has, matching
 *        this function's own pre-existing behavior exactly; a non-empty
 *        list filters to just those (a number that doesn't match any
 *        parsed channel is silently ignored), the whole-file counterpart
 *        to `importMidiSelectionInto()`'s own identical parameter, for a
 *        caller that wants channel selection *without* snippet chopping.
 * @return The newly added layer ids, in the same order `parseMidiFile()`
 *         returned their own channels (ascending `channelNumber`) when
 *         `separateLayerPerChannel` is `true`; a single-entry vector with
 *         the one shared layer's id otherwise. Empty if the file couldn't
 *         be parsed, or had no notes on any (selected) channel.
 */
[[nodiscard]] std::vector<sound_mind::core::LayerId> importMidiChannelsInto(
    sound_mind::core::Project& project, const std::filesystem::path& path, bool separateLayerPerChannel = true,
    QString* errorMessage = nullptr, const std::vector<int>& channelNumbers = {});

/**
 * @brief A parsed MIDI file's own channels, plus how it would split into
 *        `project`'s own duration worth of snippets - the row data
 *        `MidiImportDialog` displays. `v0.Y.55.1` Installment B.
 */
struct MidiImportPreview {
    /// @brief Every channel with at least one note, ascending
    ///        `channelNumber` - see `sound_mind::core::parseMidiFile()`'s
    ///        own docs.
    std::vector<sound_mind::core::MidiChannelNotes> channels;

    /// @brief How the file's own overall duration (the latest note-end
    ///        time across every channel) splits into `project`'s own
    ///        duration worth of windows - see `midiSnippetWindowsFor()`'s
    ///        own docs for the exact math. Reuses
    ///        `AudioSnippetPickerDialog::RowData` directly (a plain
    ///        index/start/end triple, nothing audio-specific about its
    ///        shape) rather than a near-identical MIDI-only struct.
    std::vector<AudioSnippetPickerDialog::RowData> snippets;
};

/**
 * @brief Parses the MIDI file at `path` and computes how it would split
 *        into `project`'s own duration worth of snippets, without
 *        importing anything or showing any dialog - the read-only preview
 *        `MainWindow::importMidi()` uses to decide whether
 *        `MidiImportDialog` needs showing at all, and to build its row
 *        data when it does.
 *
 * @param project The project whose duration to split against.
 * @param path Path to the `.mid`/`.midi` file to parse.
 * @param errorMessage If non-null and this returns `std::nullopt`, set to
 *        a human-readable description of what went wrong.
 * @return The parsed channels and computed snippets; `std::nullopt` if the
 *         file couldn't be parsed, had no notes on any channel, or
 *         `project`'s own duration is zero (nothing to split against).
 */
[[nodiscard]] std::optional<MidiImportPreview> midiImportPreviewForFile(const sound_mind::core::Project& project,
                                                                         const std::filesystem::path& path,
                                                                         QString* errorMessage = nullptr);

/**
 * @brief Imports specific channels and snippets of a MIDI file as new
 *        layers into `project` - the general, real-choice-supporting
 *        counterpart to `importMidiChannelsInto()`'s own "everything, no
 *        chopping" convenience, driven by `MidiImportDialog`'s own
 *        selection. `v0.Y.55.1` Installment B.
 *
 * Re-parses `path` from scratch (mirroring `encodeAudioSnippets()`'s own
 * "re-read the file, don't thread a previously-parsed result through"
 * precedent - a one-time import action, not a hot path) and recomputes the
 * identical snippet grid `midiImportPreviewForFile()` did, deterministically
 * from `project`'s own settings and the file's own parsed content - so
 * `snippetIndices` (captured against that earlier preview) still resolves
 * to the same windows here.
 *
 * A note is included in a given snippet window if its own
 * `startTimeSeconds` falls within `[windowStart, windowEnd)` - its full
 * `durationSeconds` is kept even if it runs past the window's own end,
 * the same "don't split a single discrete event in two" reasoning
 * `importMidiChannelsInto()`'s own docs already establish for why no
 * chopping happens at all without an explicit snippet selection; a note
 * kept this way is rebased so `windowStart` becomes its own new time `0`,
 * matching how each of `encodeAudioSnippets()`'s own snippets becomes an
 * independent, zero-based layer.
 *
 * A (channel, snippet) combination with nothing left in it after clipping
 * is silently skipped - no empty layer/operation is created for it.
 *
 * @param project The project to import into.
 * @param path Path to the `.mid`/`.midi` file to import.
 * @param channelNumbers Which channels to include, by `channelNumber` -
 *        see `MidiChannelNotes::channelNumber`'s own docs. A number that
 *        doesn't match any parsed channel is silently ignored.
 * @param snippetIndices Which of the computed snippet windows to include,
 *        in any order and with any duplicates ignored; an index at or
 *        beyond the computed snippet count is silently skipped.
 * @param separateLayerPerChannel When `true`, each (channel, snippet)
 *        combination with at least one note in it becomes its own new
 *        layer, named "`<stem>` - Ch`<n>` (`<GM instrument name>`)"
 *        (with a `_NNNN` snippet-index suffix, zero-padded to four
 *        digits, when more than one snippet was computed for this file -
 *        same convention `importAudioSnippetsInto()` already uses). When
 *        `false`, every selected channel's own notes for one snippet
 *        window share a single new layer instead, named plainly from the
 *        file's own stem (with the same `_NNNN` suffix rule) -
 *        `OperationLog::activeOperationsTargeting()` already folds
 *        multiple operations targeting one layer together on rebuild.
 * @param errorMessage If non-null and this returns empty, set to a
 *        human-readable description of what went wrong.
 * @return The newly added layer ids, in channel-then-snippet order when
 *         `separateLayerPerChannel` is `true`, snippet order otherwise.
 *         Empty if the file couldn't be parsed, no requested channel had
 *         any notes, or nothing survived clipping to the requested
 *         snippets.
 */
[[nodiscard]] std::vector<sound_mind::core::LayerId> importMidiSelectionInto(
    sound_mind::core::Project& project, const std::filesystem::path& path, const std::vector<int>& channelNumbers,
    const std::vector<std::size_t>& snippetIndices, bool separateLayerPerChannel, QString* errorMessage = nullptr);

}  // namespace sound_mind::studio
