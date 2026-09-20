#pragma once

#include <QDockWidget>
#include <QString>
#include <QStringList>

class QComboBox;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel exposing Recording's transport and input device
 *        selection - see `docs/sound-mind-roadmap.md`'s Transport Panels
 *        milestone (`v0.Y.16.1`).
 *
 * Adapted from the legacy Studio's `RecordPanel`, narrowed to what this
 * codebase's Recording actually has scope for today: a Start/Stop button
 * and an input device picker (with a "(System Default)" first entry) - no
 * input gain control of its own; `RecordEngine` gained one as of
 * `v0.0.42.1`, but it's surfaced through the new `ConfigureDevicesPanel`
 * instead of a second, duplicate slider here.
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`/`LoopPanel`: every user action is a signal `MainWindow`
 * connects to its own handlers. Wrapped in a `QScrollArea` so its content
 * is never clipped, and never forces the dock wider than the window.
 */
class RecordPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with an empty device list.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit RecordPanel(QWidget* parent = nullptr);

    /// @brief Updates the Start/Stop button's label/checked state and
    ///        enables/disables the device picker (locked while recording,
    ///        matching `LoopPanel`'s own reasoning - `RecordEngine`'s
    ///        device selection only takes effect on its *next* start()).
    /// @param recording Whether Recording is currently active.
    void setRecording(bool recording);

    /// @brief Replaces the input device picker's choices - a
    ///        "(System Default)" entry (mapping to an empty device name)
    ///        always comes first.
    /// @param deviceNames Real device names, in the order they should be
    ///        listed after the default entry.
    void setInputDevices(const QStringList& deviceNames);

    /**
     * @brief Selects `deviceName` in the input device picker, without
     *        repopulating its list or emitting inputDeviceChanged() -
     *        `v0.0.42.1` (Configure Devices panel), for `MainWindow` to
     *        keep this picker in sync when the *same* underlying
     *        preference is changed from the Configure Devices panel
     *        instead of this one.
     * @param deviceName The device to select; falls back to
     *        "(System Default)" if it isn't currently in the picker's own
     *        list (e.g. stale until the next setInputDevices() refresh).
     */
    void setSelectedInputDevice(const QString& deviceName);

signals:
    /// @brief The Start/Stop button was clicked.
    void toggleRequested();

    /// @brief The input device picker's selection changed.
    /// @param deviceName The chosen device's real name, or empty for
    ///        "(System Default)".
    void inputDeviceChanged(const QString& deviceName);

private:
    QPushButton* toggleButton_ = nullptr;
    QComboBox* inputDeviceCombo_ = nullptr;
};

}  // namespace sound_mind::studio
