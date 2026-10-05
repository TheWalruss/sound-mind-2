#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QProgressBar;
class QPushButton;
class QSlider;

namespace sound_mind::studio {

/**
 * @brief The actual input/output device selection, gain, and "test it"
 *        controls - extracted out of `ConfigureDevicesPanel` (`v0.Y.62.1`
 *        Installment H), so the exact same controls can be embedded both
 *        in that dock panel and directly on `LandingPage`, letting a user
 *        pick and test devices before a project is even open.
 *
 * Identical behavior and public surface to what `ConfigureDevicesPanel`
 * used to implement directly - every signal/setter here is exactly what
 * that class forwards 1:1 now that it's a thin `QDockWidget` wrapper
 * around one instance of this. See `ConfigureDevicesPanel`'s own class
 * docs for the actual device/gain-ownership model (one input device/gain,
 * one output device/gain, shared across every engine with that side) -
 * this class itself is purely presentational and knows nothing about
 * `RecordEngine`/`LoopEngine`/`PlaybackController`/`DeviceTestTonePlayer`,
 * same as every other panel in this codebase.
 *
 * **Two independent instances can exist at once** (the dock's own, and
 * `LandingPage`'s own) - `MainWindow` is responsible for keeping both in
 * sync, calling the same setter on each whenever device/gain/testing
 * state changes, and connecting the same handler to the matching signal
 * on each. Neither instance knows the other exists.
 */
class DeviceConfigurationWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief The gain sliders' own range, `[0, 200]` - see
    ///        `ConfigureDevicesPanel::kMaxGainPercent`'s own docs.
    static constexpr int kMaxGainPercent = 200;

    /// @brief Builds the widget with empty device lists, gains at 100%
    ///        (unity), and both "Test" toggles unchecked.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit DeviceConfigurationWidget(QWidget* parent = nullptr);

    /// @brief See `ConfigureDevicesPanel::setInputDevices()`'s own docs.
    /// @param deviceNames Real device names, in listed order.
    void setInputDevices(const QStringList& deviceNames);

    /// @brief See `ConfigureDevicesPanel::setOutputDevices()`'s own docs.
    /// @param deviceNames Real device names, in listed order.
    void setOutputDevices(const QStringList& deviceNames);

    /**
     * @brief Selects `deviceName` in the input device combo, without
     *        emitting inputDeviceChanged() - `v0.Y.62.1` Installment H,
     *        for `MainWindow` to keep this instance's own displayed
     *        selection in sync with the *other* `DeviceConfigurationWidget`
     *        instance whenever either one's own combo actually changes
     *        (see the class's own docs on why two instances can exist).
     * @param deviceName The device to select - falls back to
     *        "(System Default)" if not currently among this combo's own
     *        items (see `setSelectedDeviceInCombo()`'s own docs).
     */
    void setSelectedInputDevice(const QString& deviceName);

    /// @brief See setSelectedInputDevice()'s own docs - the output
    ///        device combo's own counterpart.
    /// @param deviceName The device to select - see setSelectedInputDevice()'s
    ///        own docs.
    void setSelectedOutputDevice(const QString& deviceName);

    /// @brief See `ConfigureDevicesPanel::setInputGainPercent()`'s own docs.
    /// @param percent Clamped to `[0, kMaxGainPercent]`.
    void setInputGainPercent(int percent);

    /// @brief See `ConfigureDevicesPanel::setOutputGainPercent()`'s own docs.
    /// @param percent Clamped to `[0, kMaxGainPercent]`.
    void setOutputGainPercent(int percent);

    /// @brief See `ConfigureDevicesPanel::setInputLevel()`'s own docs.
    /// @param level `[0, 1]` (or beyond - clamped here).
    void setInputLevel(float level);

    /// @brief See `ConfigureDevicesPanel::setTestingInput()`'s own docs.
    /// @param testing The new checked state.
    void setTestingInput(bool testing);

    /// @brief See `ConfigureDevicesPanel::setTestingOutput()`'s own docs.
    /// @param testing The new checked state.
    void setTestingOutput(bool testing);

    /// @brief See `ConfigureDevicesPanel::setInputDeviceSelectionEnabled()`'s
    ///        own docs.
    /// @param enabled `false` to disable; `true` to re-enable.
    void setInputDeviceSelectionEnabled(bool enabled);

    /// @brief See `ConfigureDevicesPanel::setOutputDeviceSelectionEnabled()`'s
    ///        own docs.
    /// @param enabled `false` to disable; `true` to re-enable.
    void setOutputDeviceSelectionEnabled(bool enabled);

signals:
    /// @brief See `ConfigureDevicesPanel::refreshRequested()`'s own docs.
    void refreshRequested();

    /// @brief See `ConfigureDevicesPanel::inputDeviceChanged()`'s own docs.
    void inputDeviceChanged(const QString& deviceName);

    /// @brief See `ConfigureDevicesPanel::outputDeviceChanged()`'s own docs.
    void outputDeviceChanged(const QString& deviceName);

    /// @brief See `ConfigureDevicesPanel::inputGainPercentChanged()`'s own
    ///        docs.
    void inputGainPercentChanged(int percent);

    /// @brief See `ConfigureDevicesPanel::outputGainPercentChanged()`'s own
    ///        docs.
    void outputGainPercentChanged(int percent);

    /// @brief See `ConfigureDevicesPanel::testInputToggled()`'s own docs.
    void testInputToggled(bool testing);

    /// @brief See `ConfigureDevicesPanel::testOutputToggled()`'s own docs.
    void testOutputToggled(bool testing);

private:
    QComboBox* inputDeviceCombo_ = nullptr;
    QSlider* inputGainSlider_ = nullptr;
    QPushButton* testInputButton_ = nullptr;
    QProgressBar* inputLevelBar_ = nullptr;

    QComboBox* outputDeviceCombo_ = nullptr;
    QSlider* outputGainSlider_ = nullptr;
    QPushButton* testOutputButton_ = nullptr;
};

}  // namespace sound_mind::studio
