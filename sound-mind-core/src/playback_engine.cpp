#include "sound_mind/core/playback_engine.h"

#include <algorithm>

#include "sound_mind/core/audio_device_list.h"

namespace sound_mind::core {

PlaybackEngine::PlaybackEngine(AudioDeviceMode deviceMode) {
    if (deviceMode == AudioDeviceMode::None) {
        return;
    }
    const juce::String error = deviceManager_.initialiseWithDefaultDevices(0, 2);
    deviceAvailable_ = error.isEmpty();
    if (deviceAvailable_) {
        deviceManager_.addAudioCallback(this);
    }
}

PlaybackEngine::~PlaybackEngine() {
    if (deviceAvailable_) {
        deviceManager_.removeAudioCallback(this);
    }
}

void PlaybackEngine::loadAudio(sound_mind::codec::AudioBuffer audio) {
    audio_ = std::move(audio);
    position_.store(0, std::memory_order_relaxed);
    loopEnabled_.store(false, std::memory_order_relaxed);
    rangeEndSamples_.store(audio_.frameCount(), std::memory_order_relaxed);
    loopBackSamples_.store(0, std::memory_order_relaxed);
}

void PlaybackEngine::play() {
    playing_.store(true, std::memory_order_relaxed);
}

void PlaybackEngine::pause() {
    playing_.store(false, std::memory_order_relaxed);
}

void PlaybackEngine::stop() {
    playing_.store(false, std::memory_order_relaxed);
    position_.store(0, std::memory_order_relaxed);
}

bool PlaybackEngine::isPlaying() const noexcept {
    return playing_.load(std::memory_order_relaxed);
}

bool PlaybackEngine::isDeviceAvailable() const noexcept {
    return deviceAvailable_;
}

std::vector<std::string> PlaybackEngine::availableOutputDeviceNames() {
    return availableAudioDeviceNames(deviceManager_, false);
}

bool PlaybackEngine::setPreferredOutputDevice(const std::string& deviceName) {
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager_.getAudioDeviceSetup(setup);
    setup.outputDeviceName = juce::String(deviceName);
    setup.useDefaultOutputChannels = true;
    const juce::String error = deviceManager_.setAudioDeviceSetup(setup, true);
    if (!error.isEmpty()) {
        return false;
    }
    deviceAvailable_ = true;
    return true;
}

std::string PlaybackEngine::currentOutputDeviceName() const {
    if (!deviceAvailable_) {
        return {};
    }
    return deviceManager_.getAudioDeviceSetup().outputDeviceName.toStdString();
}

void PlaybackEngine::setVolume(float volume) noexcept {
    volume_.store(std::clamp(volume, 0.0f, kMaxVolume), std::memory_order_relaxed);
}

float PlaybackEngine::volume() const noexcept {
    return volume_.load(std::memory_order_relaxed);
}

std::size_t PlaybackEngine::positionSamples() const noexcept {
    return position_.load(std::memory_order_relaxed);
}

std::size_t PlaybackEngine::totalSamples() const noexcept {
    return audio_.frameCount();
}

std::uint32_t PlaybackEngine::sampleRateHz() const noexcept {
    return audio_.sampleRateHz;
}

void PlaybackEngine::seek(std::size_t sampleIndex) noexcept {
    std::size_t position = std::min(sampleIndex, audio_.frameCount());
    const std::size_t rangeEnd = std::min(rangeEndSamples_.load(std::memory_order_relaxed), audio_.frameCount());
    if (position >= rangeEnd) {
        const RangeCrossing crossing = resolveRangeCrossing(rangeEnd);
        position = crossing.position;
        if (crossing.halted) {
            playing_.store(false, std::memory_order_relaxed);
        }
    }
    position_.store(position, std::memory_order_relaxed);
}

void PlaybackEngine::setPlaybackRange(bool loopEnabled, std::size_t rangeEndSamples,
                                       std::size_t loopBackSamples) noexcept {
    loopEnabled_.store(loopEnabled, std::memory_order_relaxed);
    rangeEndSamples_.store(rangeEndSamples, std::memory_order_relaxed);
    loopBackSamples_.store(loopBackSamples, std::memory_order_relaxed);
}

PlaybackEngine::RangeCrossing PlaybackEngine::resolveRangeCrossing(std::size_t rangeEnd) const noexcept {
    const bool loopEnabled = loopEnabled_.load(std::memory_order_relaxed);
    const std::size_t loopBack = loopBackSamples_.load(std::memory_order_relaxed);
    if (loopEnabled && loopBack < rangeEnd) {
        return {loopBack, false};
    }
    return {rangeEnd, true};
}

void PlaybackEngine::renderBlock(float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept {
    if (numOutputChannels <= 0 || numSamples <= 0) {
        return;
    }

    if (!playing_.load(std::memory_order_relaxed)) {
        for (int channel = 0; channel < numOutputChannels; ++channel) {
            std::fill_n(outputChannelData[channel], numSamples, 0.0f);
        }
        return;
    }

    std::size_t position = position_.load(std::memory_order_relaxed);
    const std::size_t frameCount = audio_.frameCount();
    const std::size_t rangeEnd = std::min(rangeEndSamples_.load(std::memory_order_relaxed), frameCount);
    const float volume = volume_.load(std::memory_order_relaxed);

    // Resolved once up front too (not just on the in-loop crossing below) -
    // covers a position that's already at or past rangeEnd as this block
    // starts (a degenerate rangeEnd == 0, or a race against a very recent
    // setPlaybackRange() call), so this can never get stuck rendering
    // silence forever without ever actually halting.
    bool halted = false;
    if (position >= rangeEnd) {
        const RangeCrossing crossing = resolveRangeCrossing(rangeEnd);
        position = crossing.position;
        halted = crossing.halted;
    }

    for (int sample = 0; sample < numSamples; ++sample) {
        if (!halted && position < rangeEnd) {
            outputChannelData[0][sample] = audio_.left[position] * volume;
            if (numOutputChannels > 1) {
                outputChannelData[1][sample] = audio_.right[position] * volume;
            }
            for (int channel = 2; channel < numOutputChannels; ++channel) {
                outputChannelData[channel][sample] = 0.0f;
            }
            ++position;
            // A loop-back landing mid-block keeps filling the rest of this
            // same block with real audio from the new position - the whole
            // point of resolving this here rather than in MainWindow's own
            // position-polling timer (see the class docs' Decision #217
            // note): no silence at the seam at all, not just a shorter gap.
            if (position >= rangeEnd) {
                const RangeCrossing crossing = resolveRangeCrossing(rangeEnd);
                position = crossing.position;
                halted = crossing.halted;
            }
        } else {
            for (int channel = 0; channel < numOutputChannels; ++channel) {
                outputChannelData[channel][sample] = 0.0f;
            }
        }
    }

    position_.store(position, std::memory_order_relaxed);
    if (halted) {
        playing_.store(false, std::memory_order_relaxed);
    }
}

void PlaybackEngine::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                       int /*numInputChannels*/, float* const* outputChannelData,
                                                       int numOutputChannels, int numSamples,
                                                       const juce::AudioIODeviceCallbackContext& /*context*/) {
    renderBlock(outputChannelData, numOutputChannels, numSamples);
}

void PlaybackEngine::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) {}

void PlaybackEngine::audioDeviceStopped() {}

}  // namespace sound_mind::core
