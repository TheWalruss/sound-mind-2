#include "sound_mind/core/device_test_tone_player.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace sound_mind::core {

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi;
}  // namespace

DeviceTestTonePlayer::DeviceTestTonePlayer(AudioDeviceMode deviceMode) : deviceMode_(deviceMode) {}

DeviceTestTonePlayer::~DeviceTestTonePlayer() { stop(); }

void DeviceTestTonePlayer::start(const std::string& outputDeviceName) {
    stop();
    phase_ = 0.0;
    deviceProblem_.clear();

    if (deviceMode_ == AudioDeviceMode::Real) {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.outputDeviceName = juce::String(outputDeviceName);
        setup.useDefaultOutputChannels = true;
        const juce::String error = deviceManager_.initialise(0, 2, nullptr, true, {}, &setup);
        deviceAvailable_ = error.isEmpty();
        if (deviceAvailable_) {
            deviceManager_.addAudioCallback(this);
        } else {
            deviceProblem_ = "Could not open the output device: " + error.toStdString();
        }
    }

    playing_.store(true, std::memory_order_relaxed);
}

void DeviceTestTonePlayer::stop() {
    if (!playing_.load(std::memory_order_relaxed)) {
        return;
    }

    if (deviceAvailable_) {
        deviceManager_.removeAudioCallback(this);
        deviceAvailable_ = false;
    }
    deviceProblem_.clear();

    playing_.store(false, std::memory_order_relaxed);
}

const std::string& DeviceTestTonePlayer::deviceProblem() const noexcept { return deviceProblem_; }

bool DeviceTestTonePlayer::isPlaying() const noexcept { return playing_.load(std::memory_order_relaxed); }

bool DeviceTestTonePlayer::isDeviceAvailable() const noexcept { return deviceAvailable_; }

void DeviceTestTonePlayer::setGain(float gain) noexcept {
    gain_.store(std::clamp(gain, 0.0f, kMaxGain), std::memory_order_relaxed);
}

float DeviceTestTonePlayer::gain() const noexcept { return gain_.load(std::memory_order_relaxed); }

void DeviceTestTonePlayer::processBlock(float* const* outputChannelData, int numOutputChannels,
                                         int numSamples) noexcept {
    if (numOutputChannels <= 0 || outputChannelData == nullptr || numSamples <= 0) {
        return;
    }

    if (!playing_.load(std::memory_order_relaxed)) {
        for (int channel = 0; channel < numOutputChannels; ++channel) {
            std::fill_n(outputChannelData[channel], numSamples, 0.0f);
        }
        return;
    }

    const double increment =
        kTwoPi * static_cast<double>(kToneFrequencyHz) / sampleRateHz_.load(std::memory_order_relaxed);
    const float gain = gain_.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i) {
        const float sample = kToneAmplitude * gain * static_cast<float>(std::sin(phase_));
        for (int channel = 0; channel < numOutputChannels; ++channel) {
            outputChannelData[channel][i] = sample;
        }
        phase_ += increment;
        if (phase_ >= kTwoPi) {
            phase_ -= kTwoPi;
        }
    }
}

void DeviceTestTonePlayer::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                             int /*numInputChannels*/, float* const* outputChannelData,
                                                             int numOutputChannels, int numSamples,
                                                             const juce::AudioIODeviceCallbackContext& /*context*/) {
    processBlock(outputChannelData, numOutputChannels, numSamples);
}

void DeviceTestTonePlayer::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    const double rate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    sampleRateHz_.store(rate > 0.0 ? rate : kSampleRateHz, std::memory_order_relaxed);
}

void DeviceTestTonePlayer::audioDeviceStopped() {}

}  // namespace sound_mind::core
