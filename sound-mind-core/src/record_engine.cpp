#include "sound_mind/core/record_engine.h"

#include <algorithm>

namespace sound_mind::core {

RecordEngine::RecordEngine(std::uint32_t sampleRateHz, AudioDeviceMode deviceMode)
    : deviceMode_(deviceMode),
      sampleRateHz_(sampleRateHz),
      captureRingLeft_(static_cast<std::size_t>(kRingCapacity), 0.0f),
      captureRingRight_(static_cast<std::size_t>(kRingCapacity), 0.0f) {
    capturedAudio_.sampleRateHz = sampleRateHz_;
}

RecordEngine::~RecordEngine() {
    stop();
}

void RecordEngine::start() {
    if (recording_.load(std::memory_order_relaxed)) {
        return;
    }

    capturedAudio_.left.clear();
    capturedAudio_.right.clear();
    captureFifo_.reset();

    if (deviceMode_ == AudioDeviceMode::Real) {
        // Input-only - Record doesn't monitor/pass audio through to an
        // output device (see the class docs' deferred-scope note).
        const juce::String error = deviceManager_.initialiseWithDefaultDevices(2, 0);
        deviceAvailable_ = error.isEmpty();
        if (deviceAvailable_) {
            deviceManager_.addAudioCallback(this);
        }
    }

    recording_.store(true, std::memory_order_relaxed);
}

void RecordEngine::stop() {
    if (!recording_.load(std::memory_order_relaxed)) {
        return;
    }

    if (deviceAvailable_) {
        deviceManager_.removeAudioCallback(this);
        deviceAvailable_ = false;
    }

    // Catch anything captured between the last drainAvailable() call and
    // just now, rather than silently discarding it.
    drainAvailable();

    recording_.store(false, std::memory_order_relaxed);
}

bool RecordEngine::isRecording() const noexcept {
    return recording_.load(std::memory_order_relaxed);
}

bool RecordEngine::isDeviceAvailable() const noexcept {
    return deviceAvailable_;
}

void RecordEngine::drainAvailable() {
    const int available = captureFifo_.getNumReady();
    if (available <= 0) {
        return;
    }

    const std::size_t oldSize = capturedAudio_.left.size();
    capturedAudio_.left.resize(oldSize + static_cast<std::size_t>(available));
    capturedAudio_.right.resize(oldSize + static_cast<std::size_t>(available));

    auto readScope = captureFifo_.read(available);
    int idx = 0;
    readScope.forEach([&](int ringIndex) {
        capturedAudio_.left[oldSize + static_cast<std::size_t>(idx)] = captureRingLeft_[static_cast<std::size_t>(ringIndex)];
        capturedAudio_.right[oldSize + static_cast<std::size_t>(idx)] =
            captureRingRight_[static_cast<std::size_t>(ringIndex)];
        ++idx;
    });
}

const sound_mind::codec::AudioBuffer& RecordEngine::capturedAudio() const noexcept {
    return capturedAudio_;
}

void RecordEngine::processBlock(const float* const* inputChannelData, int numInputChannels, int numSamples) noexcept {
    if (numSamples <= 0 || numInputChannels <= 0 || inputChannelData == nullptr) {
        return;
    }

    auto writeScope = captureFifo_.write(numSamples);
    int inputIndex = 0;
    writeScope.forEach([&](int ringIndex) {
        captureRingLeft_[static_cast<std::size_t>(ringIndex)] = inputChannelData[0][inputIndex];
        captureRingRight_[static_cast<std::size_t>(ringIndex)] =
            (numInputChannels > 1) ? inputChannelData[1][inputIndex] : inputChannelData[0][inputIndex];
        ++inputIndex;
    });
    // Any input that didn't fit (drainAvailable() falling behind) is
    // simply dropped, not blocked on - an audio callback must never wait.
}

void RecordEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                     float* const* outputChannelData, int numOutputChannels,
                                                     int numSamples, const juce::AudioIODeviceCallbackContext& /*context*/) {
    processBlock(inputChannelData, numInputChannels, numSamples);
    // Requested as an input-only device (see start()) - but silence
    // defensively in case a platform/driver hands back output channels
    // anyway.
    for (int channel = 0; channel < numOutputChannels; ++channel) {
        std::fill_n(outputChannelData[channel], numSamples, 0.0f);
    }
}

void RecordEngine::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) {}

void RecordEngine::audioDeviceStopped() {}

}  // namespace sound_mind::core
