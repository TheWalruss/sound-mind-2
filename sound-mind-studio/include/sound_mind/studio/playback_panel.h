#pragma once

#include <QDockWidget>
#include <QString>
#include <QStringList>

class QComboBox;
class QPushButton;
class QSlider;

namespace sound_mind::studio {

/**
 * @brief A dockable panel exposing Playback's transport, output device
 *        selection, and volume control - see
 *        `docs/sound-mind-roadmap.md`'s Transport Panels milestone
 *        (`v0.Y.16.1`).
 *
 * Adapted from the legacy Studio's `PlaybackPanel`, narrowed to what this
 * codebase's Playback actually has scope for today: Play/Pause/Stop
 * buttons (mirroring `MainWindow`'s existing three-button transport - not
 * combined into a single toggle), an output device picker, and a volume
 * slider - none of legacy's loop-preview markers, position display, or
 * follow mode, which aren't part of this codebase's Playback scope at all.
 *
 * Purely presentational, the same division of responsibility as
 * `LayersPanel`/`LoopPanel`/`RecordPanel`: every user action is a signal
 * `MainWindow` connects to its own handlers. Wrapped in a `QScrollArea` so
 * its content is never clipped, and never forces the dock wider than the
 * window.
 */
class PlaybackPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief The volume slider's range, `[0, 200]` - a percentage where
    /// `100` is unity gain and above `100` is a real boost past it,
    /// matching `PlaybackEngine::kMaxVolume` (`2.0`).
    static constexpr int kMaxVolumePercent = 200;

    /// @brief Builds the panel with an empty device list and volume at
    ///        100% (unity).
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit PlaybackPanel(QWidget* parent = nullptr);

    /// @brief Replaces the output device picker's choices - a
    ///        "(System Default)" entry (mapping to an empty device name)
    ///        always comes first.
    /// @param deviceNames Real device names, in the order they should be
    ///        listed after the default entry.
    void setOutputDevices(const QStringList& deviceNames);

    /// @brief Sets the volume slider's displayed position without emitting
    ///        volumePercentChanged() - for `MainWindow` to sync display
    ///        state without a signal feedback loop.
    /// @param percent Clamped to `[0, kMaxVolumePercent]`.
    void setVolumePercent(int percent);

signals:
    /// @brief The Play button was clicked.
    void playRequested();

    /// @brief The Pause button was clicked.
    void pauseRequested();

    /// @brief The Stop button was clicked.
    void stopRequested();

    /// @brief The output device picker's selection changed.
    /// @param deviceName The chosen device's real name, or empty for
    ///        "(System Default)".
    void outputDeviceChanged(const QString& deviceName);

    /// @brief The volume slider moved.
    /// @param percent `[0, kMaxVolumePercent]` - `100` is unity gain.
    void volumePercentChanged(int percent);

private:
    QComboBox* outputDeviceCombo_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
};

}  // namespace sound_mind::studio
