#pragma once

#include <cstddef>
#include <vector>

#include <QDialog>

class QListWidget;

namespace sound_mind::studio {

/**
 * @brief Lets the user choose which project-length snippets of a longer
 *        audio import actually become layers - see
 *        `docs/sound-mind-roadmap.md`'s Audio Import Snippets milestone
 *        (`v0.Y.19.1`).
 *
 * Going beyond the legacy Studio, per explicit instruction: the legacy
 * Studio always imported every snippet of a too-long audio file, with no
 * picker at all. This dialog is shown only when there's an actual choice
 * to make - `MainWindow::importAudio()` skips it entirely for audio that
 * splits into just one snippet (see its own docs).
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`/`CreateProjectWizard`: `MainWindow` builds the row data
 * from `audioSnippetsForFile()`'s result and reads `selectedIndices()`
 * back after `exec()` returns `QDialog::Accepted` - no file I/O or
 * project mutation happens in this class at all.
 */
class AudioSnippetPickerDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief One snippet's worth of display data, computed by
    /// `MainWindow::audioSnippetsForFile()`.
    struct RowData {
        /// @brief This snippet's position among the source audio's whole
        /// split, starting at `0` - not necessarily contiguous with what
        /// ends up actually imported, since some snippets may go unchecked.
        std::size_t index = 0;

        /// @brief Where this snippet starts within the source audio, in
        /// seconds.
        double startSeconds = 0.0;

        /// @brief Where this snippet ends within the source audio, in
        /// seconds - shorter than a full loop length for the final
        /// snippet, if the source's length isn't an exact multiple of it.
        double endSeconds = 0.0;
    };

    /**
     * @brief Builds the dialog with one row per entry in `snippets`, every
     *        row checked by default - so accepting immediately, with no
     *        changes, matches the legacy Studio's own "import everything"
     *        behavior.
     * @param snippets The snippets to list, in order.
     * @param parent The owning widget, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit AudioSnippetPickerDialog(const std::vector<RowData>& snippets, QWidget* parent = nullptr);

    /// @brief The snippets currently checked.
    /// @return Each checked row's `RowData::index`, in list order.
    [[nodiscard]] std::vector<std::size_t> selectedIndices() const;

private:
    /// @brief The "Select All" checkbox's slot - sets every row's checked
    ///        state to match, in one direction only (a row's own checkbox
    ///        changing doesn't reflect back onto "Select All" - a
    ///        deliberately simple first pass).
    void setAllChecked(bool checked);

    QListWidget* list_ = nullptr;
};

}  // namespace sound_mind::studio
