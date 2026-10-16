#include "sound_mind/core/master_limiter.h"

#include <algorithm>
#include <cmath>

namespace sound_mind::core {

MasterLimiter::MasterLimiter(LookAhead lookAhead)
    : lookAheadOn_(lookAhead == LookAhead::On),
      delayLeft_(kMaxLookAheadSamples, 0.0f),
      delayRight_(kMaxLookAheadSamples, 0.0f),
      targets_(kMaxLookAheadSamples, 1.0f) {
    setSampleRate(44100.0);
}

void MasterLimiter::setSampleRate(double sampleRateHz) noexcept {
    if (sampleRateHz <= 0.0) {
        return;
    }
    lookAhead_ = std::clamp<std::size_t>(static_cast<std::size_t>(std::lround(0.0015 * sampleRateHz)), 1,
                                         kMaxLookAheadSamples);
    // Within ~2% of the target over the look-ahead; ~80 ms release.
    attack_ = 1.0 - std::exp(-4.0 / static_cast<double>(lookAhead_));
    release_ = 1.0 - std::exp(-1.0 / (0.08 * sampleRateHz));
    reset();
}

void MasterLimiter::setCeilingDb(float ceilingDb) noexcept {
    ceilingDb_.store(std::clamp(ceilingDb, -24.0f, 0.0f), std::memory_order_relaxed);
}

void MasterLimiter::reset() noexcept {
    std::fill(delayLeft_.begin(), delayLeft_.end(), 0.0f);
    std::fill(delayRight_.begin(), delayRight_.end(), 0.0f);
    std::fill(targets_.begin(), targets_.end(), 1.0f);
    head_ = 0;
    gain_ = 1.0;
}

void MasterLimiter::process(float* left, float* right, std::size_t count) noexcept {
    if (!enabled()) {
        return;
    }
    const auto ceiling = static_cast<float>(std::pow(10.0, ceilingDb() / 20.0));
    if (!lookAheadOn_) {
        for (std::size_t i = 0; i < count; ++i) {
            const float peak = std::max(std::abs(left[i]), right != nullptr ? std::abs(right[i]) : 0.0f);
            const double target = peak > ceiling ? static_cast<double>(ceiling / peak) : 1.0;
            gain_ = target < gain_ ? target : gain_ + (target - gain_) * release_;
            const auto gain = static_cast<float>(gain_);
            left[i] = std::clamp(left[i] * gain, -ceiling, ceiling);
            if (right != nullptr) {
                right[i] = std::clamp(right[i] * gain, -ceiling, ceiling);
            }
        }
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const float inLeft = left[i];
        const float inRight = right != nullptr ? right[i] : 0.0f;
        const float peak = std::max(std::abs(inLeft), std::abs(inRight));
        // The oldest sample leaves the delay line as the newest enters.
        const float outLeft = delayLeft_[head_];
        const float outRight = delayRight_[head_];
        delayLeft_[head_] = inLeft;
        delayRight_[head_] = inRight;
        targets_[head_] = peak > ceiling ? ceiling / peak : 1.0f;
        head_ = (head_ + 1) % lookAhead_;

        const float windowMinimum = *std::min_element(targets_.begin(), targets_.begin() + lookAhead_);
        gain_ += (windowMinimum - gain_) * (windowMinimum < gain_ ? attack_ : release_);
        const auto gain = static_cast<float>(gain_);
        left[i] = std::clamp(outLeft * gain, -ceiling, ceiling);
        if (right != nullptr) {
            right[i] = std::clamp(outRight * gain, -ceiling, ceiling);
        }
    }
}

void applyMasterLimiter(sound_mind::codec::AudioBuffer& audio, float ceilingDb) {
    const std::size_t count = audio.frameCount();
    if (count == 0) {
        return;
    }
    MasterLimiter limiter;
    limiter.setSampleRate(static_cast<double>(audio.sampleRateHz));
    limiter.setCeilingDb(ceilingDb);
    const std::size_t latency = limiter.latencySamples();
    const bool stereo = audio.right.size() == count;
    // Feed the look-ahead's worth of silence after the end, then drop the
    // first `latency` outputs, so the result lines up with the input.
    std::vector<float> left(audio.left);
    std::vector<float> right = stereo ? audio.right : std::vector<float>{};
    left.resize(count + latency, 0.0f);
    if (stereo) {
        right.resize(count + latency, 0.0f);
    }
    limiter.process(left.data(), stereo ? right.data() : nullptr, left.size());
    std::copy(left.begin() + static_cast<std::ptrdiff_t>(latency), left.end(), audio.left.begin());
    if (stereo) {
        std::copy(right.begin() + static_cast<std::ptrdiff_t>(latency), right.end(), audio.right.begin());
    }
}

}  // namespace sound_mind::core
