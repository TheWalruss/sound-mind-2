#pragma once

#include <QDockWidget>

class QCheckBox;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel exposing Loop Mode's transport and "Freeze Loop"
 *        control - see `docs/sound-mind-roadmap.md`'s Transport Panels
 *        milestone (`v0.Y.16.1`).
 *
 * Adapted from the legacy Studio's `LivePanel`: a Start/Stop button and
 * "Freeze Loop" (moved here from the transport toolbar checkbox `v0.Y.12.1`
 * added as a confirmed stopgap; labeled "Keep Looping" before a later
 * rename - the checkbox's internal name and API were never affected, only
 * the displayed text) - narrowed to what this codebase's Loop Mode
 * actually has scope for today: no channel selector, input level meter, or
 * session-saving - none of those are part of this milestone's confirmed
 * scope, unlike the legacy panel.
 *
 * **Input/output device pickers removed (real-world testing pass,
 * 2026-09-20, finding #7)**: they duplicated Configure Devices' own
 * combos exactly, always kept in sync with them by `MainWindow` - pure
 * redundancy, not distinct controls. Configure Devices is now the single
 * place to change either; its own combos are disabled while Loop Mode is
 * running (`ConfigureDevicesPanel::setInputDeviceSelectionEnabled()`/
 * `setOutputDeviceSelectionEnabled()`), preserving this panel's own prior
 * "locked while running" behavior.
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`: every user action is a signal `MainWindow` connects to
 * its own handlers - no engine state is touched here directly. Its content
 * is wrapped in a `QScrollArea` so it's never clipped, and never forces
 * the dock wider than the window, if it doesn't fit the available height.
 */
class LoopPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with "Freeze Loop" unchecked.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit LoopPanel(QWidget* parent = nullptr);

    /// @brief Updates the Start/Stop button's label/checked state.
    /// @param running Whether Loop Mode is currently running.
    void setRunning(bool running);

    /// @brief Sets the "Freeze Loop" checkbox's displayed state without
    ///        emitting keepLoopingChanged() - for `MainWindow` to sync
    ///        display state without a signal feedback loop.
    /// @param checked The new displayed state.
    void setKeepLoopingChecked(bool checked);

signals:
    /// @brief The Start/Stop button was clicked.
    void toggleRequested();

    /// @brief The "Freeze Loop" checkbox changed.
    void keepLoopingChanged(bool checked);

private:
    QPushButton* toggleButton_ = nullptr;
    QCheckBox* keepLoopingCheckBox_ = nullptr;
};

}  // namespace sound_mind::studio
