#pragma once

#include <QDockWidget>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
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
 * slider - none of legacy's loop-preview markers or follow mode, which
 * aren't part of this codebase's Playback scope at all.
 *
 * **As of `v0.0.21.1`:** a draggable position bar and elapsed/total time
 * label were added (confirmed with the user, alongside a matching moving
 * playhead line drawn over the canvas itself - see `CanvasWidget`'s own
 * docs) - the position display the class docs above used to say wasn't in
 * scope.
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

    /// @brief The position slider's fixed resolution - its range is always
    /// `[0, kPositionSliderSteps]` regardless of the loaded audio's actual
    /// duration, mapped to/from seconds via totalSeconds_ at the moment
    /// each value is read - fine-grained enough (1000 steps) that seeking
    /// feels continuous even for a long recording.
    static constexpr int kPositionSliderSteps = 1000;

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

    /**
     * @brief Sets the total duration the position bar/label represent -
     *        called whenever a new layer is loaded for playback. Resets
     *        the displayed position to `0:00`.
     * @param totalSeconds The loaded audio's own duration, in seconds;
     *        `0` (or negative) is treated as "nothing loaded" - the
     *        position bar reports `0:00 / 0:00` and dragging it does
     *        nothing.
     */
    void setDuration(double totalSeconds);

    /**
     * @brief Sets the position bar's displayed position and the elapsed
     *        half of the time label, without emitting seekRequested() -
     *        for `MainWindow` to sync display state during playback
     *        without a signal feedback loop (the same
     *        setVolumePercent()/`QSignalBlocker` pattern, but connected to
     *        `QSlider::valueChanged` rather than `sliderMoved` since a
     *        position bar needs to reflect playback progress it didn't
     *        itself initiate, unlike a slider that's only ever
     *        user-driven).
     * @param positionSeconds Clamped to `[0, totalSeconds]` (the value
     *        setDuration() was last called with).
     */
    void setPositionSeconds(double positionSeconds);

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

    /// @brief The position bar was dragged to a new position.
    /// @param positionSeconds The requested position, in seconds into the
    ///        duration setDuration() was last called with.
    void seekRequested(double positionSeconds);

private:
    /// @brief Refreshes positionLabel_'s text from totalSeconds_ and
    /// whatever positionSlider_'s current value implies.
    void updatePositionLabel();

    QComboBox* outputDeviceCombo_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QSlider* positionSlider_ = nullptr;
    QLabel* positionLabel_ = nullptr;
    double totalSeconds_ = 0.0;
    double positionSeconds_ = 0.0;
};

}  // namespace sound_mind::studio
