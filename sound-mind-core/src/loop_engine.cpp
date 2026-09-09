#include "sound_mind/core/loop_engine.h"

#include <algorithm>
#include <cstddef>

#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

LoopEngine::LoopEngine(sound_mind::codec::StreamCodecConfig config, std::size_t loopLengthSamples,
                        AudioDeviceMode deviceMode)
    : juce::Thread("LoopEngine"),
      deviceMode_(deviceMode),
      config_(config),
      // A 0-length loop would make the wrap arithmetic in processBlock()/
      // processPendingAudio() undefined (division/modulo by zero) - see the
      // constructor's own docs. Callers are expected never to actually pass
      // 0 in practice (a project's duration is always positive).
      loopLengthSamples_(std::max<std::size_t>(loopLengthSamples, 1)),
      captureRingLeft_(static_cast<std::size_t>(kRingCapacity), 0.0f),
      captureRingRight_(static_cast<std::size_t>(kRingCapacity), 0.0f) {
    for (int slot = 0; slot < 2; ++slot) {
        playbackLeft_[static_cast<std::size_t>(slot)].assign(loopLengthSamples_, 0.0f);
        playbackRight_[static_cast<std::size_t>(slot)].assign(loopLengthSamples_, 0.0f);
    }
}

LoopEngine::~LoopEngine() {
    stop();
}

void LoopEngine::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return;
    }

    // Reset all loop-cycle state, so a second start()/stop() cycle behaves
    // exactly like a freshly constructed engine - see start()'s own docs.
    pendingLoopLeft_.clear();
    pendingLoopRight_.clear();
    for (int slot = 0; slot < 2; ++slot) {
        std::fill(playbackLeft_[static_cast<std::size_t>(slot)].begin(),
                   playbackLeft_[static_cast<std::size_t>(slot)].end(), 0.0f);
        std::fill(playbackRight_[static_cast<std::size_t>(slot)].begin(),
                   playbackRight_[static_cast<std::size_t>(slot)].end(), 0.0f);
    }
    activeSlot_.store(0, std::memory_order_relaxed);
    readingSlot_ = 0;
    playPos_ = 0;
    capturedSamplesTotal_.store(0, std::memory_order_relaxed);
    loopsDecoded_.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(imageMutex_);
        currentImage_ = sound_mind::codec::StreamImage{};
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

void LoopEngine::stop() {
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

bool LoopEngine::isRunning() const noexcept {
    return running_.load(std::memory_order_relaxed);
}

bool LoopEngine::isDeviceAvailable() const noexcept {
    return deviceAvailable_;
}

void LoopEngine::setKeepLooping(bool keepLooping) noexcept {
    keepLooping_.store(keepLooping, std::memory_order_relaxed);
}

bool LoopEngine::keepLooping() const noexcept {
    return keepLooping_.load(std::memory_order_relaxed);
}

sound_mind::codec::StreamImage LoopEngine::currentImage() const {
    std::lock_guard<std::mutex> lock(imageMutex_);
    return currentImage_;
}

std::uint64_t LoopEngine::loopsCaptured() const noexcept {
    return capturedSamplesTotal_.load(std::memory_order_relaxed) / static_cast<std::uint64_t>(loopLengthSamples_);
}

std::uint64_t LoopEngine::loopsBehind() const noexcept {
    const std::uint64_t captured = loopsCaptured();
    const std::uint64_t decoded = loopsDecoded_.load(std::memory_order_relaxed);
    return captured > decoded ? captured - decoded : 0;
}

void LoopEngine::processBlock(const float* const* inputChannelData, int numInputChannels,
                               float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept {
    if (numSamples <= 0) {
        return;
    }

    // Capture side: while "Keep looping" is on, newly captured input is
    // simply never recorded - see setKeepLooping()'s docs - so the last
    // successfully processed loop just keeps replaying, untouched, below.
    if (!keepLooping_.load(std::memory_order_relaxed) && numInputChannels > 0 && inputChannelData != nullptr) {
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
        capturedSamplesTotal_.fetch_add(static_cast<std::uint64_t>(numSamples), std::memory_order_relaxed);
    }

    if (numOutputChannels <= 0 || outputChannelData == nullptr) {
        return;
    }

    // Playback side: read sequentially through readingSlot_, wrapping - and
    // only then re-checking which slot is active - at this loop's own
    // boundary. See the class docs' latency note for exactly what this
    // means for how soon a captured loop is actually heard.
    for (int i = 0; i < numSamples; ++i) {
        outputChannelData[0][i] = playbackLeft_[static_cast<std::size_t>(readingSlot_)][playPos_];
        if (numOutputChannels > 1) {
            outputChannelData[1][i] = playbackRight_[static_cast<std::size_t>(readingSlot_)][playPos_];
        }
        ++playPos_;
        if (playPos_ >= loopLengthSamples_) {
            playPos_ = 0;
            readingSlot_ = activeSlot_.load(std::memory_order_acquire);
        }
    }
    for (int channel = 2; channel < numOutputChannels; ++channel) {
        std::fill_n(outputChannelData[channel], numSamples, 0.0f);
    }
}

void LoopEngine::processPendingAudio() {
    const int available = captureFifo_.getNumReady();
    if (available > 0) {
        const std::size_t previousSize = pendingLoopLeft_.size();
        pendingLoopLeft_.resize(previousSize + static_cast<std::size_t>(available));
        pendingLoopRight_.resize(previousSize + static_cast<std::size_t>(available));
        auto readScope = captureFifo_.read(available);
        int idx = 0;
        readScope.forEach([&](int ringIndex) {
            pendingLoopLeft_[previousSize + static_cast<std::size_t>(idx)] =
                captureRingLeft_[static_cast<std::size_t>(ringIndex)];
            pendingLoopRight_[previousSize + static_cast<std::size_t>(idx)] =
                captureRingRight_[static_cast<std::size_t>(ringIndex)];
            ++idx;
        });
    }

    // Process every whole loop now accumulated - normally at most one, but
    // more than one can be queued if the worker has fallen behind (see
    // loopsBehind()'s docs) or a caller (a test, in particular) feeds
    // several loops' worth of capture in a single processBlock() call
    // before ever draining it.
    while (pendingLoopLeft_.size() >= loopLengthSamples_) {
        sound_mind::codec::AudioBuffer captured;
        captured.sampleRateHz = config_.sampleRateHz;
        captured.left.assign(pendingLoopLeft_.begin(), pendingLoopLeft_.begin() + static_cast<long>(loopLengthSamples_));
        captured.right.assign(pendingLoopRight_.begin(),
                                pendingLoopRight_.begin() + static_cast<long>(loopLengthSamples_));
        pendingLoopLeft_.erase(pendingLoopLeft_.begin(), pendingLoopLeft_.begin() + static_cast<long>(loopLengthSamples_));
        pendingLoopRight_.erase(pendingLoopRight_.begin(),
                                  pendingLoopRight_.begin() + static_cast<long>(loopLengthSamples_));

        sound_mind::codec::StreamImage image = sound_mind::codec::encode(captured, config_);
        const sound_mind::codec::AudioBuffer decoded = sound_mind::codec::decode(image);

        const int nextSlot = 1 - activeSlot_.load(std::memory_order_relaxed);
        auto& nextLeft = playbackLeft_[static_cast<std::size_t>(nextSlot)];
        auto& nextRight = playbackRight_[static_cast<std::size_t>(nextSlot)];
        const std::size_t copyCount = std::min(loopLengthSamples_, decoded.frameCount());
        std::copy_n(decoded.left.data(), copyCount, nextLeft.begin());
        std::copy_n(decoded.right.data(), copyCount, nextRight.begin());
        if (copyCount < loopLengthSamples_) {
            // decode() is documented to trim to the exact sample count it
            // was given to encode - this only guards against that contract
            // ever changing out from under this class.
            std::fill(nextLeft.begin() + static_cast<long>(copyCount), nextLeft.end(), 0.0f);
            std::fill(nextRight.begin() + static_cast<long>(copyCount), nextRight.end(), 0.0f);
        }
        activeSlot_.store(nextSlot, std::memory_order_release);

        {
            std::lock_guard<std::mutex> lock(imageMutex_);
            currentImage_ = std::move(image);
        }
        loopsDecoded_.fetch_add(1, std::memory_order_relaxed);
    }
}

void LoopEngine::run() {
    while (!threadShouldExit()) {
        processPendingAudio();
        wait(5);
    }
}

void LoopEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                   float* const* outputChannelData, int numOutputChannels,
                                                   int numSamples, const juce::AudioIODeviceCallbackContext& /*context*/) {
    processBlock(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
}

void LoopEngine::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) {}

void LoopEngine::audioDeviceStopped() {}

}  // namespace sound_mind::core
