#pragma once

#include <cstddef>
#include <vector>

#include <QDialog>

#include "sound_mind/core/midi_import.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"

class QCheckBox;
class QListWidget;

namespace sound_mind::studio {

/**
 * @brief Lets the user choose which channels of a parsed MIDI file to
 *        import, whether to import each selected channel's entire content
 *        or chop it into project-length snippets first (and if so, which
 *        snippet(s)), and whether each channel becomes its own layer -
 *        `v0.Y.55.1`'s own MIDI track import milestone, Installment B. See
 *        `docs/sound-mind-design.md`'s "Import" ("the user can select all
 *        or a subset of the MIDI programs in the file... choose to import
 *        the selected program into a single layer, or in a separate layer
 *        for each program").
 *
 * Shown only when there's an actual choice to make - `MainWindow::
 * importMidi()` skips it entirely for a file with exactly one channel and
 * one snippet (nothing to choose either way), the same "only show a picker
 * when there's a real choice" precedent `AudioSnippetPickerDialog`
 * establishes for audio.
 *
 * **"Import whole file" defaults to checked** (real-world testing pass,
 * 2026-09-27 - see `wholeFile()`'s own docs) - `snippetList_`/its own
 * "Select All Snippets" checkbox are disabled while it's checked, since
 * `selectedSnippetIndices()`'s own result is ignored in that case. Together
 * with "Separate layer per channel" also defaulting to checked, accepting
 * this dialog immediately, with no changes, reproduces
 * `sound_mind::studio::importMidiChannelsInto()`'s own original
 * Installment A default exactly (every channel, unchopped, one layer
 * each).
 *
 * Purely presentational, the same division of responsibility as
 * `AudioSnippetPickerDialog`: `MainWindow` builds the row data from
 * `sound_mind::studio::midiImportPreviewForFile()`'s result and reads
 * `selectedChannelNumbers()`/`selectedSnippetIndices()`/
 * `separateLayerPerChannel()`/`wholeFile()` back after `exec()` returns
 * `QDialog::Accepted` - no file I/O or project mutation happens in this
 * class at all. Unlike `AudioSnippetPickerDialog`, there's no offset field
 * here yet - a real, separate future refinement if this milestone's own
 * snippet grid ever needs the same re-anchoring audio's own Offset field
 * gives (`docs/sound-mind-architecture.md`'s real-world testing pass
 * finding #23), not built here since nothing has asked for it yet.
 */
class MidiImportDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog with one row per entry in `channels` and one
     *        per entry in `snippets`, every row of both lists checked by
     *        default, and "Separate layer per channel" checked by default -
     *        so accepting immediately, with no changes, imports everything
     *        into one layer per channel, matching
     *        `sound_mind::studio::importMidiChannelsInto()`'s own
     *        Installment A default exactly.
     * @param channels The parsed channels to list, in order (see
     *        `sound_mind::core::parseMidiFile()`'s own docs - ascending
     *        `channelNumber`).
     * @param snippets The project-length snippets to list, in order.
     * @param parent The owning widget, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit MidiImportDialog(const std::vector<sound_mind::core::MidiChannelNotes>& channels,
                               const std::vector<AudioSnippetPickerDialog::RowData>& snippets,
                               QWidget* parent = nullptr);

    /// @brief The channels currently checked.
    /// @return Each checked row's own `channelNumber`, in list order.
    [[nodiscard]] std::vector<int> selectedChannelNumbers() const;

    /// @brief The snippets currently checked - meaningless while
    ///        `wholeFile()` is `true` (the list is disabled, but still
    ///        reports whatever it last held).
    /// @return Each checked row's `RowData::index`, in list order.
    [[nodiscard]] std::vector<std::size_t> selectedSnippetIndices() const;

    /// @brief The "Separate layer per channel" checkbox's own current
    ///        state - see `importMidiSelectionInto()`'s own docs for
    ///        exactly what each value produces.
    /// @return `true` if checked.
    [[nodiscard]] bool separateLayerPerChannel() const;

    /**
     * @brief The "Import whole file (don't split into snippets)"
     *        checkbox's own current state - real-world testing pass,
     *        2026-09-27, finding: a real way to bypass snippet chopping
     *        entirely was missing, not just a narrower default. Checked
     *        (the default): every selected channel's *entire* file-length
     *        content imports unchopped, via
     *        `sound_mind::studio::importMidiChannelsInto()`'s own
     *        channel-filtering overload - `selectedSnippetIndices()`'s own
     *        result is meaningless in this case (the snippet list is
     *        disabled while this is checked). Unchecked: the snippet list
     *        applies as normal, via `importMidiSelectionInto()`.
     * @return `true` if checked.
     */
    [[nodiscard]] bool wholeFile() const;

private:
    /// @brief The "Select All Channels"/"Select All Snippets" checkboxes'
    ///        own slots - set every row of the matching list to match, in
    ///        one direction only, the same deliberately simple shape
    ///        `AudioSnippetPickerDialog::setAllChecked()` already
    ///        establishes.
    void setAllChannelsChecked(bool checked);
    void setAllSnippetsChecked(bool checked);

    /// @brief `wholeFileCheckBox_`'s own `toggled` slot - enables/disables
    ///        `snippetList_` and its own "Select All Snippets" checkbox to
    ///        make clear the snippet selection is ignored while whole-file
    ///        import is checked, rather than silently doing nothing.
    void setSnippetControlsEnabled(bool enabled);

    QListWidget* channelList_ = nullptr;
    QListWidget* snippetList_ = nullptr;
    QCheckBox* separateLayerCheckBox_ = nullptr;
    QCheckBox* wholeFileCheckBox_ = nullptr;
    QCheckBox* selectAllSnippetsCheckBox_ = nullptr;
};

}  // namespace sound_mind::studio
