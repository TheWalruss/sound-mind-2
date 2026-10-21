#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <QObject>
#include <QString>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/core/background_task.h"
#include "sound_mind/core/macro.h"
#include "sound_mind/core/macro_renderer.h"

class QTimer;

namespace sound_mind::studio {

class CanvasWidget;
class PlaybackController;

/**
 * @brief Macro → Play Macro (`v0.1.44.3`, `docs/sound-mind-design.md`'s
 *        *Macros*): plays a recorded performance back seamlessly, rendering
 *        it ahead on a background thread and never touching the project.
 *
 * A `MacroRenderer` renders segments on a `BackgroundTask`, up to
 * kMaxAheadSeconds ahead of the playhead. The UI thread (a ~33 ms timer)
 * moves finished segments into a playback stream
 * (`PlaybackController::beginStream()`/`appendStream()`) and keeps their
 * canvas images. Playback starts once kStartBufferSeconds are ready (or the
 * whole performance, if shorter); from then on the canvas shows each
 * segment's image when its moment comes (`CanvasWidget::setFrameOverride()`)
 * with the playhead line where it was recorded. If rendering falls behind,
 * the stream waits silently rather than skipping.
 *
 * Stopping - stop(), the end of the performance, or a rendering failure -
 * clears the override and emits finished().
 *
 * UI-thread only (apart from the worker it owns).
 */
class MacroPlayer : public QObject {
    Q_OBJECT

public:
    /// @brief How much must be rendered before playback starts (seconds).
    static constexpr double kStartBufferSeconds = 2.0;
    /// @brief How far ahead of the playhead rendering may get (seconds).
    static constexpr double kMaxAheadSeconds = 20.0;

    /**
     * @param playbackController Non-owning; plays the stream. Must outlive
     *        this.
     * @param canvas Non-owning; shows the frames. Must outlive this.
     * @param parent Standard QObject parent.
     */
    MacroPlayer(PlaybackController* playbackController, CanvasWidget* canvas, QObject* parent = nullptr);
    ~MacroPlayer() override;

    /**
     * @brief Starts rendering `renderer`'s performance; playback follows
     *        once enough is ready. Stops anything already playing first.
     * @param renderer A renderer, not yet prepared.
     */
    void start(std::unique_ptr<sound_mind::core::MacroRenderer> renderer);

    /// @brief Stops: rendering, playback and the frame override. Emits
    ///        finished() if it was active.
    void stop();

    /// @brief Whether a performance is being rendered or played.
    /// @return `true` between start() and stopping.
    [[nodiscard]] bool isActive() const noexcept { return active_; }

    /// @brief Whether playback has begun (enough was buffered).
    /// @return `true` once it has.
    [[nodiscard]] bool hasStartedPlaying() const noexcept { return startedPlaying_; }

    /// @brief How much has been rendered and handed to the stream.
    /// @return Seconds of the performance.
    [[nodiscard]] double bufferedSeconds() const noexcept { return appendedSeconds_; }

    /**
     * @brief Moves finished segments into the stream and updates what's
     *        shown - what the timer does every ~33 ms; public so tests can
     *        drive it.
     */
    void tick();

signals:
    /// @brief Playing began or ended.
    void activeChanged(bool active);

    /// @brief It stopped. `error` is empty for a normal end or stop().
    void finished(const QString& error);

private:
    struct Ready {
        double startSeconds = 0.0;
        double durationSeconds = 0.0;
        sound_mind::codec::RgbImage canvas;
        sound_mind::codec::AudioBuffer audio;
    };

    void finish(const QString& error);

    PlaybackController* playbackController_;
    CanvasWidget* canvas_;
    QTimer* timer_;
    std::shared_ptr<sound_mind::core::MacroRenderer> renderer_;
    std::unique_ptr<sound_mind::core::BackgroundTask> task_;
    sound_mind::core::Macro macro_;
    double projectSeconds_ = 0.0;

    // Shared with the worker.
    std::mutex mutex_;
    std::deque<Ready> ready_;
    std::atomic<double> playheadSeconds_{0.0};
    std::atomic<bool> workerDone_{false};
    QString workerError_;

    // UI thread only.
    bool active_ = false;
    bool startedPlaying_ = false;
    double appendedSeconds_ = 0.0;
    std::deque<std::pair<double, sound_mind::codec::RgbImage>> frames_;
};

}  // namespace sound_mind::studio
