#pragma once

#include <QDockWidget>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel exposing Loop Mode's transport, device
 *        selection, and "Freeze Loop" controls - see
 *        `docs/sound-mind-roadmap.md`'s Transport Panels milestone
 *        (`v0.Y.16.1`).
 *
 * Adapted from the legacy Studio's `LivePanel`: a Start/Stop button, input
 * and output device pickers (each with a Refresh button), and "Freeze
 * Loop" (moved here from the transport toolbar checkbox `v0.Y.12.1`
 * added as a confirmed stopgap; labeled "Keep Looping" before a later
 * rename - the checkbox's internal name and API were never affected, only
 * the displayed text) - narrowed to what this codebase's Loop Mode
 * actually has scope for today: no channel selector, input level meter, or
 * session-saving - none of those are part of this milestone's confirmed
 * scope, unlike the legacy panel.
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
    /// @brief Builds the panel with empty device lists and "Freeze Loop"
    ///        unchecked.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit LoopPanel(QWidget* parent = nullptr);

    /**
     * @brief Updates the Start/Stop button's label/checked state and
     *        enables/disables the device pickers.
     *
     * The pickers are locked while running - matching the legacy panel's
     * own behavior - since `LoopEngine`'s device selection only takes
     * effect on its *next* start() (see its own docs), so changing them
     * mid-session wouldn't do anything until a restart anyway.
     *
     * @param running Whether Loop Mode is currently running.
     */
    void setRunning(bool running);

    /// @brief Replaces the input device picker's choices - a
    ///        "(System Default)" entry (mapping to an empty device name)
    ///        always comes first.
    /// @param deviceNames Real device names, in the order they should be
    ///        listed after the default entry.
    void setInputDevices(const QStringList& deviceNames);

    /// @brief Replaces the output device picker's choices - see
    ///        setInputDevices()'s docs for the same "(System Default)"
    ///        first-entry convention.
    /// @param deviceNames Real device names, in the order they should be
    ///        listed after the default entry.
    void setOutputDevices(const QStringList& deviceNames);

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

    /// @brief The input device picker's selection changed.
    /// @param deviceName The chosen device's real name, or empty for
    ///        "(System Default)".
    void inputDeviceChanged(const QString& deviceName);

    /// @brief The output device picker's selection changed - see
    ///        inputDeviceChanged()'s docs for the same empty-means-default
    ///        convention.
    void outputDeviceChanged(const QString& deviceName);

private:
    QPushButton* toggleButton_ = nullptr;
    QCheckBox* keepLoopingCheckBox_ = nullptr;
    QComboBox* inputDeviceCombo_ = nullptr;
    QComboBox* outputDeviceCombo_ = nullptr;
};

}  // namespace sound_mind::studio
