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
    const float volume = volume_.load(std::memory_order_relaxed);

    for (int sample = 0; sample < numSamples; ++sample) {
        if (position < frameCount) {
            outputChannelData[0][sample] = audio_.left[position] * volume;
            if (numOutputChannels > 1) {
                outputChannelData[1][sample] = audio_.right[position] * volume;
            }
            for (int channel = 2; channel < numOutputChannels; ++channel) {
                outputChannelData[channel][sample] = 0.0f;
            }
            ++position;
        } else {
            for (int channel = 0; channel < numOutputChannels; ++channel) {
                outputChannelData[channel][sample] = 0.0f;
            }
        }
    }

    position_.store(position, std::memory_order_relaxed);
    if (position >= frameCount) {
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
