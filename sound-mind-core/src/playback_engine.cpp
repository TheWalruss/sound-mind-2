#include "sound_mind/core/playback_engine.h"

#include <algorithm>

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

    for (int sample = 0; sample < numSamples; ++sample) {
        if (position < frameCount) {
            outputChannelData[0][sample] = audio_.left[position];
            if (numOutputChannels > 1) {
                outputChannelData[1][sample] = audio_.right[position];
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
