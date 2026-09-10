#include "sound_mind/studio/playback_controller.h"

#include <algorithm>

#include <QTimer>

namespace sound_mind::studio {

PlaybackController::PlaybackController(QObject* parent) : QObject(parent) {
    // ~30fps - matches MainWindow's own loopUpdateTimer_ cadence: frequent
    // enough for a moving position bar/playhead to read as smooth, without
    // repainting so often it competes noticeably for CPU time.
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &PlaybackController::poll);
}

void PlaybackController::load(sound_mind::codec::AudioBuffer audio) {
    engine_.stop();  // loadAudio() must not be called while playing - see its own docs.
    timer_->stop();
    engine_.loadAudio(std::move(audio));
    loaded_ = true;
    emit durationChanged(totalSeconds());
}

bool PlaybackController::isLoaded() const noexcept {
    return loaded_;
}

void PlaybackController::invalidate() noexcept {
    loaded_ = false;
}

void PlaybackController::play() {
    engine_.play();
    timer_->start();
}

void PlaybackController::pause() {
    engine_.pause();
    timer_->stop();
}

void PlaybackController::stop() {
    engine_.stop();
    loaded_ = false;
    timer_->stop();
}

void PlaybackController::seek(double positionSeconds) {
    if (!loaded_) {
        return;
    }
    const auto sampleRate = engine_.sampleRateHz();
    if (sampleRate == 0) {
        return;
    }
    engine_.seek(static_cast<std::size_t>(std::max(0.0, positionSeconds) * sampleRate));
    emitPosition();
}

bool PlaybackController::isPlaying() const noexcept {
    return engine_.isPlaying();
}

double PlaybackController::totalSeconds() const noexcept {
    const auto sampleRate = engine_.sampleRateHz();
    return sampleRate > 0 ? static_cast<double>(engine_.totalSamples()) / sampleRate : 0.0;
}

bool PlaybackController::setOutputDevice(const QString& deviceName) {
    return engine_.setPreferredOutputDevice(deviceName.toStdString());
}

QString PlaybackController::currentOutputDeviceName() const {
    return QString::fromStdString(engine_.currentOutputDeviceName());
}

std::vector<std::string> PlaybackController::availableOutputDeviceNames() {
    return engine_.availableOutputDeviceNames();
}

void PlaybackController::setVolume(int percent) {
    engine_.setVolume(static_cast<float>(percent) / 100.0f);
}

float PlaybackController::volume() const noexcept {
    return engine_.volume();
}

void PlaybackController::poll() {
    emitPosition();
    if (!engine_.isPlaying()) {
        // Playback reached the end on its own - stop polling rather than
        // continuing to tick against a position that's no longer advancing.
        timer_->stop();
    }
}

void PlaybackController::emitPosition() {
    const auto sampleRate = engine_.sampleRateHz();
    const double positionSeconds = sampleRate > 0 ? static_cast<double>(engine_.positionSamples()) / sampleRate : 0.0;
    emit positionChanged(positionSeconds);
}

}  // namespace sound_mind::studio
