#pragma once

#include <cstddef>
#include <vector>

#include <QDialog>

class QDoubleSpinBox;
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
 *
 * **Snippet offset (real-world testing pass finding #23)**: an **Offset**
 * spin box lets the split itself be re-anchored, discarding that much
 * audio off the very start before re-splitting what's left - the whole
 * grid restarts fresh at the new offset, not just a shortened snippet
 * `0`. This dialog has no file access of its own (see the class's own
 * docs on why), so it can't recompute the split itself: changing the spin
 * box only emits offsetChanged(); the caller (`MainWindow`, which already
 * has the file path and project) recomputes via `audioSnippetsForFile()`
 * and pushes the fresh result back in via setSnippets().
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
     *        behavior. The offset spin box always starts at `0.0`,
     *        matching `snippets`' own implicit starting offset (every
     *        existing caller computes its first list with none).
     * @param snippets The snippets to list, in order.
     * @param parent The owning widget, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit AudioSnippetPickerDialog(const std::vector<RowData>& snippets, QWidget* parent = nullptr);

    /// @brief The snippets currently checked.
    /// @return Each checked row's `RowData::index`, in list order.
    [[nodiscard]] std::vector<std::size_t> selectedIndices() const;

    /// @brief The offset spin box's own current value, in seconds - the
    ///        offset `selectedIndices()`'s own indices were last
    ///        recomputed against (via setSnippets(), or `0.0` if it was
    ///        never called).
    /// @return The current offset, in seconds.
    [[nodiscard]] double offsetSeconds() const;

    /**
     * @brief Replaces the displayed snippet list - the recomputed result
     *        of a real offsetChanged() the caller already reacted to (see
     *        the class's own docs). Every row is reset to checked, the
     *        same "start from every snippet" default the constructor
     *        itself establishes, since a changed offset is a genuinely
     *        new split, not an edit to the old one - there's no
     *        meaningful way to carry over which *positions* were checked
     *        before.
     * @param snippets The freshly recomputed snippets to list, in order.
     */
    void setSnippets(const std::vector<RowData>& snippets);

signals:
    /// @brief The offset spin box's own value changed - see the class's
    ///        own docs for the recompute-and-push-back workflow this
    ///        drives. Not emitted by setSnippets() itself.
    /// @param offsetSeconds The spin box's own new value, in seconds.
    void offsetChanged(double offsetSeconds);

private:
    /// @brief The "Select All" checkbox's slot - sets every row's checked
    ///        state to match, in one direction only (a row's own checkbox
    ///        changing doesn't reflect back onto "Select All" - a
    ///        deliberately simple first pass).
    void setAllChecked(bool checked);

    /// @brief Clears and repopulates `list_` from `snippets` - the shared
    ///        row-building logic the constructor and setSnippets() both
    ///        need.
    void populateList(const std::vector<RowData>& snippets);

    QDoubleSpinBox* offsetSpinBox_ = nullptr;
    QListWidget* list_ = nullptr;
};

}  // namespace sound_mind::studio
