#pragma once

#include <QDialog>

#include "sound_mind/core/warp_operation.h"

namespace sound_mind::studio {

/**
 * @brief Lets the user choose Warp's own Axis and Mode before it's
 *        applied - `docs/sound-mind-design.md`'s "Selection" ("Warp"),
 *        `v0.Y.35.1` Installment C.
 *
 * Shown by `MainWindow::warpSelection()`, enabled only when both a
 * selection (`ToolPaletteController::hasSelection()`) and a Picked curve
 * (`PickController::selectedPath()`) exist - see that method's own docs
 * for the full "draw a curve, Pick it, then warp" workflow, which needs
 * no dedicated curve-drawing mode of its own. Purely presentational, the
 * same division of responsibility as `ImageScalePickerDialog`/
 * `AudioSnippetPickerDialogTest`: `MainWindow` reads `selectedAxis()`/
 * `selectedMode()` back after `exec()` returns `QDialog::Accepted` and
 * constructs the actual `WarpOperation` itself - no content is ever
 * displaced by this class.
 */
class WarpDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog with `WarpAxis::Frequency`/
    ///        `WarpMode::Displace` pre-selected - each tool type's own
    ///        established "first, most common" default.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit WarpDialog(QWidget* parent = nullptr);

    /// @brief The currently selected axis.
    /// @return The axis whose radio button is currently checked.
    [[nodiscard]] sound_mind::core::WarpAxis selectedAxis() const noexcept { return selectedAxis_; }

    /// @brief The currently selected mode.
    /// @return The mode whose radio button is currently checked.
    [[nodiscard]] sound_mind::core::WarpMode selectedMode() const noexcept { return selectedMode_; }

private:
    sound_mind::core::WarpAxis selectedAxis_ = sound_mind::core::WarpAxis::Frequency;
    sound_mind::core::WarpMode selectedMode_ = sound_mind::core::WarpMode::Displace;
};

}  // namespace sound_mind::studio
