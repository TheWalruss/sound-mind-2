#include "sound_mind/core/live_engine.h"

#include <algorithm>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

LiveEngine::LiveEngine(sound_mind::codec::StreamCodecConfig config, AudioDeviceMode deviceMode)
    : juce::Thread("LiveEngine"),
      deviceMode_(deviceMode),
      config_(config),
      encoder_(config),
      captureRingLeft_(static_cast<std::size_t>(kRingCapacity), 0.0f),
      captureRingRight_(static_cast<std::size_t>(kRingCapacity), 0.0f),
      playbackRingLeft_(static_cast<std::size_t>(kRingCapacity), 0.0f),
      playbackRingRight_(static_cast<std::size_t>(kRingCapacity), 0.0f) {}

LiveEngine::~LiveEngine() {
    stop();
}

void LiveEngine::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return;
    }

    if (deviceMode_ == AudioDeviceMode::Real) {
        const juce::String error = deviceManager_.initialiseWithDefaultDevices(2, 2);
        deviceAvailable_ = error.isEmpty();
        if (deviceAvailable_) {
            deviceManager_.addAudioCallback(this);
        }
    }

    running_.store(true, std::memory_order_relaxed);
    startThread();
}

void LiveEngine::stop() {
    if (!running_.load(std::memory_order_relaxed)) {
        return;
    }

    stopThread(1000);

    if (deviceAvailable_) {
        deviceManager_.removeAudioCallback(this);
        deviceAvailable_ = false;
    }

    running_.store(false, std::memory_order_relaxed);
}

bool LiveEngine::isRunning() const noexcept {
    return running_.load(std::memory_order_relaxed);
}

bool LiveEngine::isDeviceAvailable() const noexcept {
    return deviceAvailable_;
}

sound_mind::codec::StreamImage LiveEngine::currentImage() const {
    return encoder_.snapshot();
}

void LiveEngine::processBlock(const float* const* inputChannelData, int numInputChannels,
                               float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept {
    if (numSamples <= 0) {
        return;
    }

    if (numInputChannels > 0 && inputChannelData != nullptr) {
        auto writeScope = captureFifo_.write(numSamples);
        int inputIndex = 0;
        writeScope.forEach([&](int ringIndex) {
            captureRingLeft_[static_cast<std::size_t>(ringIndex)] = inputChannelData[0][inputIndex];
            captureRingRight_[static_cast<std::size_t>(ringIndex)] =
                (numInputChannels > 1) ? inputChannelData[1][inputIndex] : inputChannelData[0][inputIndex];
            ++inputIndex;
        });
        // Any input samples that didn't fit (the worker thread falling
        // behind) are simply dropped, not blocked on - an audio callback
        // must never wait. A rare, accepted degradation, not a crash.
    }

    if (numOutputChannels <= 0 || outputChannelData == nullptr) {
        return;
    }

    auto readScope = playbackFifo_.read(numSamples);
    int outputIndex = 0;
    readScope.forEach([&](int ringIndex) {
        outputChannelData[0][outputIndex] = playbackRingLeft_[static_cast<std::size_t>(ringIndex)];
        if (numOutputChannels > 1) {
            outputChannelData[1][outputIndex] = playbackRingRight_[static_cast<std::size_t>(ringIndex)];
        }
        ++outputIndex;
    });
    // Under-run (the worker hasn't produced enough decoded audio yet, e.g.
    // right at start-up): silence rather than stale/garbage samples.
    for (; outputIndex < numSamples; ++outputIndex) {
        outputChannelData[0][outputIndex] = 0.0f;
        if (numOutputChannels > 1) {
            outputChannelData[1][outputIndex] = 0.0f;
        }
    }
    for (int channel = 2; channel < numOutputChannels; ++channel) {
        std::fill_n(outputChannelData[channel], numSamples, 0.0f);
    }
}

void LiveEngine::processPendingAudio() {
    const int available = captureFifo_.getNumReady();
    if (available > 0) {
        std::vector<float> scratchLeft(static_cast<std::size_t>(available));
        std::vector<float> scratchRight(static_cast<std::size_t>(available));
        auto readScope = captureFifo_.read(available);
        int idx = 0;
        readScope.forEach([&](int ringIndex) {
            scratchLeft[static_cast<std::size_t>(idx)] = captureRingLeft_[static_cast<std::size_t>(ringIndex)];
            scratchRight[static_cast<std::size_t>(idx)] = captureRingRight_[static_cast<std::size_t>(ringIndex)];
            ++idx;
        });
        encoder_.pushSamples(scratchLeft.data(), scratchRight.data(), scratchLeft.size());
    }

    const sound_mind::codec::StreamImage image = encoder_.snapshot();
    if (image.frameCount == 0) {
        return;
    }

    // Re-decode the whole accumulated history (see the class docs' "known
    // limitation" note) and publish only its stable prefix - the last
    // `fftSize - hopLength` samples of any decode are still subject to
    // change as more frames arrive, so they're recomputed next pass
    // instead of published early.
    const sound_mind::codec::AudioBuffer decoded = sound_mind::codec::decode(image);
    const auto stableBound =
        std::min<std::size_t>(std::size_t{image.frameCount} * config_.hopLength, decoded.frameCount());
    if (stableBound <= stableSamplesEmitted_) {
        return;
    }

    const std::size_t newCount = stableBound - stableSamplesEmitted_;
    auto writeScope = playbackFifo_.write(static_cast<int>(newCount));
    int idx = 0;
    writeScope.forEach([&](int ringIndex) {
        const std::size_t sourceIndex = stableSamplesEmitted_ + static_cast<std::size_t>(idx);
        playbackRingLeft_[static_cast<std::size_t>(ringIndex)] = decoded.left[sourceIndex];
        playbackRingRight_[static_cast<std::size_t>(ringIndex)] = decoded.right[sourceIndex];
        ++idx;
    });
    stableSamplesEmitted_ += static_cast<std::size_t>(writeScope.blockSize1 + writeScope.blockSize2);
}

void LiveEngine::run() {
    while (!threadShouldExit()) {
        processPendingAudio();
        wait(5);
    }
}

void LiveEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                   float* const* outputChannelData, int numOutputChannels,
                                                   int numSamples, const juce::AudioIODeviceCallbackContext& /*context*/) {
    processBlock(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
}

void LiveEngine::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) {}

void LiveEngine::audioDeviceStopped() {}

}  // namespace sound_mind::core
