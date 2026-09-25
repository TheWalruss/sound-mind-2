#pragma once

#include <QDialog>

#include "sound_mind/core/gradient.h"

namespace sound_mind::studio {

class GradientEditorWidget;

/**
 * @brief Lets the user choose a real, multi-stop gradient to fill the
 *        current selection with - `docs/sound-mind-design.md`'s "Fill"
 *        ("Fills the selected region with a color or gradient"), real-world
 *        testing pass, 2026-09-20, finding #17.
 *
 * Shown by `MainWindow::fillSelection()`, replacing the flat, single-color
 * `QColorDialog` that method used before - see its own docs for the exact
 * prior shape. Wraps a plain `GradientEditorWidget` (the same shared editor
 * `ToolConfigurationPanel`/`FilterConfigurationPanel` already embed) plus an
 * OK/Cancel `QDialogButtonBox`, the same "purely presentational, read back
 * after `exec()`" shape `WarpDialog`'s own docs establish: `MainWindow`
 * reads gradient() back after `exec()` returns `QDialog::Accepted` and
 * applies it itself - no content is ever displaced by this class.
 */
class FillGradientDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog, seeding its own editor with `gradient`.
    /// @param gradient The gradient to start the editor from - typically
    ///        the same fully-opaque default `fillSelectionWith()`'s own
    ///        flat-color shortcut always used.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit FillGradientDialog(sound_mind::core::Gradient gradient, QWidget* parent = nullptr);

    /// @brief The gradient the editor currently holds.
    /// @return The current gradient.
    [[nodiscard]] const sound_mind::core::Gradient& gradient() const noexcept;

private:
    GradientEditorWidget* editor_ = nullptr;
};

}  // namespace sound_mind::studio
