#pragma once

#include <vector>

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;

namespace sound_mind::studio {

/**
 * @brief A modal dialog presenting a scrollable list of labeled
 *        checkboxes, with Select All/Deselect All buttons - the Note
 *        Grid's own "a button that opens a list of all the possible
 *        notes, with checkboxes" and "select or de-select octaves"
 *        requirements (`docs/sound-mind-design.md`'s "Overlay Grids" >
 *        "Frequency Grid"), shared by both since they're structurally
 *        identical: a flat list of items, each independently on or off.
 *
 * Purely presentational, the same "owns session UI state, reads back
 * through plain accessors" shape every other configuration dialog in
 * this codebase already follows - the caller supplies the labels and
 * each one's own starting checked state, reads `checkedStates()` back
 * after `exec()` returns `QDialog::Accepted`, and decides what that
 * means (which step indices or octave numbers end up excluded) entirely
 * on its own; this class has no opinion on what a checkbox *represents*.
 */
class CheckListDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog with one checkbox per entry in `labels`.
     * @param title The dialog's own window title.
     * @param labels Each checkbox's own display text, in order.
     * @param initiallyChecked Each checkbox's own starting state, the
     *        same length and order as `labels` - a short `labels` paired
     *        with a too-short `initiallyChecked` treats every missing
     *        entry as unchecked, rather than it being a hard error, since
     *        a slightly-stale caller-side list is a cosmetic concern, not
     *        a correctness one.
     * @param parent The owning widget, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    CheckListDialog(const QString& title, const QStringList& labels, const std::vector<bool>& initiallyChecked,
                     QWidget* parent = nullptr);

    /// @brief Every checkbox's own current state, in the same order
    ///        `labels` was given in - meaningful whether or not the
    ///        dialog was actually accepted; the caller decides whether to
    ///        read it based on `exec()`'s own return value.
    /// @return One entry per label, `true` if checked.
    [[nodiscard]] std::vector<bool> checkedStates() const;

private:
    std::vector<QCheckBox*> checkBoxes_;
};

}  // namespace sound_mind::studio
