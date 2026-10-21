#include "sound_mind/studio/macro_player.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include <QTimer>

#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/playback_controller.h"

namespace sound_mind::studio {

MacroPlayer::MacroPlayer(PlaybackController* playbackController, CanvasWidget* canvas, QObject* parent)
    : QObject(parent), playbackController_(playbackController), canvas_(canvas), timer_(new QTimer(this)) {
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MacroPlayer::tick);
}

MacroPlayer::~MacroPlayer() {
    if (task_) {
        task_->requestCancel();
        task_->waitForFinished();
    }
}

void MacroPlayer::start(std::unique_ptr<sound_mind::core::MacroRenderer> renderer) {
    stop();
    renderer_ = std::move(renderer);
    macro_ = renderer_->macro();
    projectSeconds_ = renderer_->projectSeconds();
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        ready_.clear();
        workerError_.clear();
    }
    frames_.clear();
    appendedSeconds_ = 0.0;
    startedPlaying_ = false;
    playheadSeconds_ = 0.0;
    workerDone_ = false;
    active_ = true;

    const std::uint32_t rate = renderer_->macro().settings.sampleRateHz;
    playbackController_->beginStream(
        static_cast<std::size_t>(std::llround(macro_.durationSeconds * static_cast<double>(rate))), rate);

    std::shared_ptr<sound_mind::core::MacroRenderer> worker = renderer_;
    task_ = std::make_unique<sound_mind::core::BackgroundTask>([this, worker](sound_mind::core::CancellationToken& token) {
        worker->setShouldCancel([&token]() { return token.cancellationRequested(); });
        std::string failure;
        if (worker->prepare(&failure)) {
            while (auto segment = worker->next(&failure)) {
                // Don't run too far ahead of what's being heard.
                while (!token.cancellationRequested() &&
                       segment->startSeconds - playheadSeconds_.load() > kMaxAheadSeconds) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                const std::lock_guard<std::mutex> lock(mutex_);
                ready_.push_back({segment->startSeconds, segment->durationSeconds, std::move(segment->canvas),
                                  std::move(segment->audio)});
            }
        }
        if (!failure.empty() && !token.cancellationRequested()) {
            const std::lock_guard<std::mutex> lock(mutex_);
            workerError_ = QString::fromStdString(failure);
        }
        workerDone_ = true;
    });
    task_->start();
    timer_->start();
    emit activeChanged(true);
}

void MacroPlayer::tick() {
    if (!active_) {
        return;
    }
    std::deque<Ready> arrived;
    QString error;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        arrived.swap(ready_);
        error = workerError_;
    }
    for (Ready& segment : arrived) {
        playbackController_->appendStream(segment.audio);
        appendedSeconds_ = segment.startSeconds + segment.durationSeconds;
        frames_.emplace_back(segment.startSeconds, std::move(segment.canvas));
    }
    if (!error.isEmpty()) {
        finish(error);
        return;
    }
    const bool allRendered = workerDone_.load() && arrived.empty();
    if (!startedPlaying_ && (appendedSeconds_ >= std::min(kStartBufferSeconds, macro_.durationSeconds) || allRendered)) {
        if (frames_.empty()) {
            finish(tr("Nothing was played while this macro recorded."));
            return;
        }
        startedPlaying_ = true;
        canvas_->setFrameOverride(frames_.front().second);
        playbackController_->play();
    }
    if (!startedPlaying_) {
        return;
    }

    const double now = playbackController_->positionSeconds();
    playheadSeconds_ = now;
    // The latest frame whose moment has come; earlier ones are done with.
    while (frames_.size() > 1 && frames_[1].first <= now + 1e-9) {
        frames_.pop_front();
        canvas_->setFrameOverride(frames_.front().second);
    }
    if (projectSeconds_ > 0.0) {
        canvas_->setPlayheadFraction(sound_mind::core::macroPlayheadAt(macro_, now).position / projectSeconds_);
    }
    const bool atEnd = now >= macro_.durationSeconds - 1e-3;
    if ((atEnd && allRendered) || (!playbackController_->isPlaying() && atEnd)) {
        finish(QString());
    }
}

void MacroPlayer::stop() {
    if (active_) {
        finish(QString());
    }
}

void MacroPlayer::finish(const QString& error) {
    timer_->stop();
    if (task_) {
        task_->requestCancel();
        task_->waitForFinished();
        task_.reset();
    }
    renderer_.reset();
    frames_.clear();
    playbackController_->stop();
    canvas_->setFrameOverride(std::nullopt);
    canvas_->setPlayheadFraction(std::nullopt);
    const bool wasActive = active_;
    active_ = false;
    startedPlaying_ = false;
    if (wasActive) {
        emit activeChanged(false);
        emit finished(error);
    }
}

}  // namespace sound_mind::studio
