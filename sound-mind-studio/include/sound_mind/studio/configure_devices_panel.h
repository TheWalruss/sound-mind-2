#pragma once

#include <QDockWidget>
#include <QString>
#include <QStringList>

class QComboBox;
class QProgressBar;
class QPushButton;
class QSlider;

namespace sound_mind::studio {

/**
 * @brief A dockable panel consolidating input/output device selection,
 *        gain, and a "test it" affordance into one place - see
 *        `docs/sound-mind-roadmap.md`'s Workflow & Device Polish milestone
 *        (`v0.0.42.1`), Installment A.
 *
 * **Consolidates, doesn't replace** - `PlaybackPanel`'s own output device
 * picker, `RecordPanel`'s/`LoopPanel`'s own input device pickers (and
 * `LoopPanel`'s own output one) keep working exactly as they already do;
 * this panel is an additional, single place to change the *same*
 * underlying preferences at once, per the roadmap's own "without removing
 * those per-panel pickers' own quick-access convenience." `MainWindow`
 * owns keeping every picker in sync (whichever one last changed).
 *
 * **One input device/gain, one output device/gain - not per-engine.**
 * Choosing an input device here applies to both `RecordEngine` and
 * `LoopEngine` at once (the two engines with an input side); choosing an
 * output device applies to `PlaybackEngine` (via `PlaybackController`) and
 * `LoopEngine` (the two with an output side) - matching the roadmap's own
 * "choose the active one for each [of input/output]" framing, not "for
 * each engine".
 *
 * **"Test" an input device** shows a live level meter (`inputLevelBar_`),
 * fed by `MainWindow`'s own dedicated, always-separate test `RecordEngine`
 * instance while testInputButton_ is checked - never the real
 * recording/loop session's own engine. **"Test" an output device** plays a
 * brief tone (`MainWindow`'s own `DeviceTestTonePlayer`) while
 * testOutputButton_ is checked.
 *
 * Purely presentational, the same division of responsibility as every
 * other dock panel: every user action is a signal `MainWindow` connects to
 * its own handlers.
 */
class ConfigureDevicesPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief The gain sliders' own range, `[0, 200]` - a percentage where
    /// `100` is unity gain, matching `PlaybackPanel::kMaxVolumePercent`'s
    /// own identical convention (and `RecordEngine::kMaxGain`/
    /// `LoopEngine::kMaxGain`'s own `2.0` ceiling).
    static constexpr int kMaxGainPercent = 200;

    /// @brief Builds the panel with empty device lists, gains at 100%
    ///        (unity), and both "Test" toggles unchecked.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit ConfigureDevicesPanel(QWidget* parent = nullptr);

    /// @brief Replaces the input device picker's choices - a
    ///        "(System Default)" entry always comes first, same convention
    ///        as every other device combo in this codebase.
    /// @param deviceNames Real device names, in listed order.
    void setInputDevices(const QStringList& deviceNames);

    /// @brief Replaces the output device picker's choices - see
    ///        setInputDevices()'s own docs.
    /// @param deviceNames Real device names, in listed order.
    void setOutputDevices(const QStringList& deviceNames);

    /// @brief Sets the input gain slider's displayed position without
    ///        emitting inputGainPercentChanged() - for `MainWindow` to sync
    ///        display state without a signal feedback loop.
    /// @param percent Clamped to `[0, kMaxGainPercent]`.
    void setInputGainPercent(int percent);

    /// @brief Sets the output gain slider's displayed position without
    ///        emitting outputGainPercentChanged() - see
    ///        setInputGainPercent()'s own docs.
    /// @param percent Clamped to `[0, kMaxGainPercent]`.
    void setOutputGainPercent(int percent);

    /// @brief Updates the live input level meter - `MainWindow` calls this
    ///        on a timer while testInputButton_ is checked.
    /// @param level `[0, 1]` (or beyond - clamped here), matching
    ///        `RecordEngine::currentInputLevel()`'s/`LoopEngine::
    ///        currentInputLevel()`'s own peak-magnitude range for ordinary
    ///        (non-clipping) audio.
    void setInputLevel(float level);

    /// @brief Sets the input "Test" button's checked state without
    ///        emitting testInputToggled() - for `MainWindow` to sync
    ///        display state when it stops a test session for a reason
    ///        other than the button itself being clicked (e.g. a project
    ///        switch).
    /// @param testing The new checked state.
    void setTestingInput(bool testing);

    /// @brief Sets the output "Test" button's checked state without
    ///        emitting testOutputToggled() - see setTestingInput()'s own
    ///        docs.
    /// @param testing The new checked state.
    void setTestingOutput(bool testing);

signals:
    /// @brief The "Refresh Devices" button was clicked.
    void refreshRequested();

    /// @brief The input device picker's selection changed.
    /// @param deviceName The chosen device's real name, or empty for
    ///        "(System Default)".
    void inputDeviceChanged(const QString& deviceName);

    /// @brief The output device picker's selection changed - see
    ///        inputDeviceChanged()'s own docs.
    void outputDeviceChanged(const QString& deviceName);

    /// @brief The input gain slider moved.
    /// @param percent `[0, kMaxGainPercent]` - `100` is unity gain.
    void inputGainPercentChanged(int percent);

    /// @brief The output gain slider moved - see
    ///        inputGainPercentChanged()'s own docs.
    void outputGainPercentChanged(int percent);

    /// @brief The "Test" toggle for the input device was checked/unchecked.
    /// @param testing `true` when newly checked (start testing).
    void testInputToggled(bool testing);

    /// @brief The "Test" toggle for the output device was checked/unchecked
    ///        - see testInputToggled()'s own docs.
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
