#include "sound_mind/core/loop_engine.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/audio_device_list.h"

namespace sound_mind::core {

LoopEngine::LoopEngine(sound_mind::codec::StreamCodecConfig config, std::size_t loopLengthSamples,
                        AudioDeviceMode deviceMode)
    : juce::Thread("LoopEngine"),
      deviceMode_(deviceMode),
      config_(config),
      loopLengthSamples_(std::max<std::size_t>(loopLengthSamples, 1)),
      previewEncoder_(config) {
    backing_.sampleRateHz = config_.sampleRateHz;
    backing_.left.assign(loopLengthSamples_, 0.0f);
    backing_.right.assign(loopLengthSamples_, 0.0f);
    for (std::size_t take = 0; take < 2; ++take) {
        takeLeft_[take].assign(loopLengthSamples_, 0.0f);
        takeRight_[take].assign(loopLengthSamples_, 0.0f);
    }
}

LoopEngine::~LoopEngine() { stop(); }

void LoopEngine::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return;
    }

    for (std::size_t take = 0; take < 2; ++take) {
        std::fill(takeLeft_[take].begin(), takeLeft_[take].end(), 0.0f);
        std::fill(takeRight_[take].begin(), takeRight_[take].end(), 0.0f);
    }
    swapInStagedBacking();
    playingTake_.store(0, std::memory_order_relaxed);
    position_ = 0;
    recording_ = !frozen_.load(std::memory_order_relaxed);
    recordingNow_.store(recording_, std::memory_order_relaxed);
    publishedPosition_.store(0, std::memory_order_relaxed);
    takesCompleted_.store(0, std::memory_order_relaxed);
    encodedTakes_ = 0;
    previewTake_ = 0;
    previewFed_ = 0;
    previewEncoder_.reset();
    {
        const std::lock_guard<std::mutex> lock(imageMutex_);
        currentImage_ = sound_mind::codec::StreamImage{};
    }
    deviceProblem_.clear();

    if (deviceMode_ == AudioDeviceMode::Real) {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = juce::String(preferredInputDevice_);
        setup.outputDeviceName = juce::String(preferredOutputDevice_);
        setup.useDefaultInputChannels = true;
        setup.useDefaultOutputChannels = true;
        const juce::String error = deviceManager_.initialise(2, 2, nullptr, true, {}, &setup);
        deviceAvailable_ = error.isEmpty();
        if (deviceAvailable_) {
            deviceManager_.addAudioCallback(this);
        } else {
            deviceProblem_ = "Could not open the input/output devices: " + error.toStdString();
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

bool LoopEngine::isRunning() const noexcept { return running_.load(std::memory_order_relaxed); }

bool LoopEngine::isDeviceAvailable() const noexcept { return deviceAvailable_; }

const std::string& LoopEngine::deviceProblem() const noexcept { return deviceProblem_; }

std::vector<std::string> LoopEngine::availableInputDeviceNames() {
    return availableAudioDeviceNames(deviceManager_, true);
}

std::vector<std::string> LoopEngine::availableOutputDeviceNames() {
    return availableAudioDeviceNames(deviceManager_, false);
}

void LoopEngine::setPreferredInputDevice(std::string deviceName) { preferredInputDevice_ = std::move(deviceName); }

const std::string& LoopEngine::preferredInputDevice() const noexcept { return preferredInputDevice_; }

void LoopEngine::setPreferredOutputDevice(std::string deviceName) { preferredOutputDevice_ = std::move(deviceName); }

const std::string& LoopEngine::preferredOutputDevice() const noexcept { return preferredOutputDevice_; }

void LoopEngine::setInputGain(float gain) noexcept {
    inputGain_.store(std::clamp(gain, 0.0f, kMaxGain), std::memory_order_relaxed);
}

float LoopEngine::inputGain() const noexcept { return inputGain_.load(std::memory_order_relaxed); }

float LoopEngine::currentInputLevel() const noexcept { return currentInputLevel_.load(std::memory_order_relaxed); }

bool LoopEngine::setBackingAudio(sound_mind::codec::AudioBuffer&& audio) {
    if (audio.frameCount() != loopLengthSamples_ || audio.sampleRateHz != config_.sampleRateHz) {
        return false;
    }
    {
        const std::lock_guard<std::mutex> lock(stagedMutex_);
        stagedBacking_ = std::move(audio);
        hasStagedBacking_.store(true, std::memory_order_release);
    }
    if (!running_.load(std::memory_order_relaxed)) {
        swapInStagedBacking();  // idle: no audio thread to wait for.
    }
    return true;
}

void LoopEngine::setTakeGains(float left, float right) noexcept {
    takeGainLeft_.store(left, std::memory_order_relaxed);
    takeGainRight_.store(right, std::memory_order_relaxed);
}

void LoopEngine::setFrozen(bool frozen) noexcept { frozen_.store(frozen, std::memory_order_relaxed); }

bool LoopEngine::frozen() const noexcept { return frozen_.load(std::memory_order_relaxed); }

std::size_t LoopEngine::playbackPositionSamples() const noexcept {
    return publishedPosition_.load(std::memory_order_relaxed);
}

std::uint64_t LoopEngine::takesCompleted() const noexcept { return takesCompleted_.load(std::memory_order_acquire); }

sound_mind::codec::StreamImage LoopEngine::currentImage() const {
    const std::lock_guard<std::mutex> lock(imageMutex_);
    return currentImage_;
}

sound_mind::codec::StreamImage LoopEngine::currentPreviewImage() const { return previewEncoder_.snapshot(); }

sound_mind::codec::StreamImage LoopEngine::emptyImage() const {
    sound_mind::codec::AudioBuffer silence;
    silence.sampleRateHz = config_.sampleRateHz;
    silence.left.assign(loopLengthSamples_, 0.0f);
    silence.right.assign(loopLengthSamples_, 0.0f);
    return sound_mind::codec::encode(silence, config_);
}

void LoopEngine::swapInStagedBacking() noexcept {
    if (!hasStagedBacking_.load(std::memory_order_acquire)) {
        return;
    }
    // try_lock: the audio thread must never wait on the UI thread - if it's
    // mid-setBackingAudio(), the next pass start picks it up instead.
    std::unique_lock<std::mutex> lock(stagedMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }
    std::swap(backing_, stagedBacking_);  // vector pointer swaps only: no allocation.
    hasStagedBacking_.store(false, std::memory_order_release);
}

void LoopEngine::processBlock(const float* const* inputChannelData, int numInputChannels,
                               float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept {
    if (numSamples <= 0) {
        return;
    }
    const bool frozen = frozen_.load(std::memory_order_relaxed);
    if (frozen) {
        recording_ = false;  // see setFrozen() - stops immediately.
    }
    const bool haveInput = numInputChannels > 0 && inputChannelData != nullptr;
    const bool haveOutput = numOutputChannels > 0 && outputChannelData != nullptr;
    const float inputGain = inputGain_.load(std::memory_order_relaxed);
    const float takeGainLeft = takeGainLeft_.load(std::memory_order_relaxed);
    const float takeGainRight = takeGainRight_.load(std::memory_order_relaxed);
    int playing = playingTake_.load(std::memory_order_relaxed);
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        const auto playingIndex = static_cast<std::size_t>(playing);
        const auto recordingIndex = static_cast<std::size_t>(1 - playing);
        if (haveOutput) {
            outputChannelData[0][i] = backing_.left[position_] + takeLeft_[playingIndex][position_] * takeGainLeft;
            if (numOutputChannels > 1) {
                outputChannelData[1][i] =
                    backing_.right[position_] + takeRight_[playingIndex][position_] * takeGainRight;
            }
        }
        if (haveInput) {
            const float left = inputChannelData[0][i] * inputGain;
            const float right = (numInputChannels > 1 ? inputChannelData[1][i] : inputChannelData[0][i]) * inputGain;
            peak = std::max({peak, std::abs(left), std::abs(right)});
            if (recording_) {
                takeLeft_[recordingIndex][position_] = left;
                takeRight_[recordingIndex][position_] = right;
            }
        }

        ++position_;
        if (position_ >= loopLengthSamples_) {
            position_ = 0;
            if (recording_) {
                playing = 1 - playing;
                playingTake_.store(playing, std::memory_order_relaxed);
                takesCompleted_.fetch_add(1, std::memory_order_release);
            }
            recording_ = !frozen;
            swapInStagedBacking();
        }
    }

    if (haveInput) {
        currentInputLevel_.store(peak, std::memory_order_relaxed);
    }
    if (haveOutput) {
        for (int channel = 2; channel < numOutputChannels; ++channel) {
            std::fill_n(outputChannelData[channel], numSamples, 0.0f);
        }
    }
    publishedPosition_.store(position_, std::memory_order_relaxed);
    recordingNow_.store(recording_, std::memory_order_relaxed);
}

void LoopEngine::processPendingAudio() {
    const std::uint64_t completed = takesCompleted_.load(std::memory_order_acquire);
    const int playing = playingTake_.load(std::memory_order_relaxed);

    // A newly finished take is the one now playing: read-only for a whole
    // pass, so encoding it here never races the audio thread's writes.
    if (completed != encodedTakes_) {
        sound_mind::codec::AudioBuffer take;
        take.sampleRateHz = config_.sampleRateHz;
        take.left = takeLeft_[static_cast<std::size_t>(playing)];
        take.right = takeRight_[static_cast<std::size_t>(playing)];
        sound_mind::codec::StreamImage image = sound_mind::codec::encode(take, config_);
        {
            const std::lock_guard<std::mutex> lock(imageMutex_);
            currentImage_ = std::move(image);
        }
        encodedTakes_ = completed;
    }

    // The live preview follows the take being recorded: the samples before
    // the published position were written in this pass and won't change.
    if (completed != previewTake_) {
        previewEncoder_.reset();
        previewFed_ = 0;
        previewTake_ = completed;
    }
    if (!recordingNow_.load(std::memory_order_relaxed)) {
        // Frozen, or unfrozen but waiting for the next pass: nothing is
        // being recorded, so the preview shows nothing.
        if (previewFed_ > 0) {
            previewEncoder_.reset();
            previewFed_ = 0;
        }
        return;
    }
    const std::size_t recorded = publishedPosition_.load(std::memory_order_relaxed);
    if (recorded > previewFed_) {
        const auto recording = static_cast<std::size_t>(1 - playing);
        previewEncoder_.pushSamples(takeLeft_[recording].data() + previewFed_, takeRight_[recording].data() + previewFed_,
                                     recorded - previewFed_);
        previewFed_ = recorded;
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
