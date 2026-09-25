#pragma once

#include <QDockWidget>

class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel exposing Recording's transport - see
 *        `docs/sound-mind-roadmap.md`'s Transport Panels milestone
 *        (`v0.Y.16.1`).
 *
 * Adapted from the legacy Studio's `RecordPanel`, narrowed to what this
 * codebase's Recording actually has scope for today: a Start/Stop button.
 *
 * **Input device picker removed (real-world testing pass, 2026-09-20,
 * finding #7)**: it duplicated Configure Devices' own input device combo
 * exactly, always kept in sync with it by `MainWindow` - pure redundancy,
 * not a distinct control. Configure Devices is now the single place to
 * change it; its own combo is disabled while Recording is active
 * (`ConfigureDevicesPanel::setInputDeviceSelectionEnabled()`), preserving
 * this panel's own prior "locked while recording" behavior.
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`/`LoopPanel`: every user action is a signal `MainWindow`
 * connects to its own handlers. Wrapped in a `QScrollArea` so its content
 * is never clipped, and never forces the dock wider than the window.
 */
class RecordPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit RecordPanel(QWidget* parent = nullptr);

    /// @brief Updates the Start/Stop button's label/checked state.
    /// @param recording Whether Recording is currently active.
    void setRecording(bool recording);

signals:
    /// @brief The Start/Stop button was clicked.
    void toggleRequested();

private:
    QPushButton* toggleButton_ = nullptr;
};

}  // namespace sound_mind::studio
