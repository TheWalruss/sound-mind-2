#include "sound_mind/studio/playback_controller.h"

#include <algorithm>
#include <cmath>

#include <QTimer>

namespace sound_mind::studio {

PlaybackController::PlaybackController(QObject* parent, sound_mind::core::AudioDeviceMode deviceMode)
    : QObject(parent), engine_(deviceMode) {
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

void PlaybackController::setPlaybackRange(bool loopEnabled, double rangeEndSeconds, double loopBackSeconds) {
    if (!loaded_) {
        return;
    }
    const auto sampleRate = engine_.sampleRateHz();
    if (sampleRate == 0) {
        return;
    }
    const auto toSamples = [sampleRate](double seconds) {
        return static_cast<std::size_t>(std::max(0.0, seconds) * sampleRate);
    };
    engine_.setPlaybackRange(loopEnabled, toSamples(rangeEndSeconds), toSamples(loopBackSeconds));
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

QString PlaybackController::outputDeviceProblem() const {
    return QString::fromStdString(engine_.deviceProblem());
}

QString PlaybackController::ensureOutputReady(const QString& preferredDeviceName) {
    if (!engine_.deviceProblem().empty()) {
        engine_.setPreferredOutputDevice(preferredDeviceName.toStdString());
    }
    if (const QString problem = outputDeviceProblem(); !problem.isEmpty()) {
        return tr("No sound will be heard: %1 Pick a working output device in Configure Devices and click Test.")
            .arg(problem);
    }
    const double deviceRate = engine_.deviceSampleRateHz();
    const double contentRate = static_cast<double>(engine_.sampleRateHz());
    if (deviceRate > 0.0 && contentRate > 0.0 && std::abs(deviceRate - contentRate) > 1.0) {
        return tr("The output device runs at %1 Hz but this project is %2 Hz, so playback will sound "
                  "%3 and %4. Set the device to %2 Hz in your system sound settings.")
            .arg(deviceRate, 0, 'f', 0)
            .arg(contentRate, 0, 'f', 0)
            .arg(deviceRate > contentRate ? tr("faster") : tr("slower"))
            .arg(deviceRate > contentRate ? tr("higher in pitch") : tr("lower in pitch"));
    }
    if (engine_.volume() <= 0.0f) {
        return tr("The output gain is 0%, so playback is silent. Raise it in Configure Devices.");
    }
    return {};
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
