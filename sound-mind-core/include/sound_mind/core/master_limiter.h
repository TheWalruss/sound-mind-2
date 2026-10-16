#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::core {

/// @brief The limiter's default ceiling: -1 dBFS.
inline constexpr float kDefaultLimiterCeilingDb = -1.0f;

/**
 * @brief The master limiter at the very end of playback and export (Pre-Beta
 *        Plan `v0.Y.80.1`): keeps every output sample within the ceiling, so
 *        a mix that goes over full scale is turned down smoothly instead of
 *        hard-clipping.
 *
 * A short **look-ahead** limiter. Each incoming sample's required gain
 * (`ceiling / |peak|`, or 1) enters a window of `latencySamples()` (1.5 ms);
 * the gain glides down toward the window's minimum fast enough to arrive
 * before the peak leaves the delay line, then recovers slowly (release
 * ~80 ms). A final clamp at the ceiling catches the small remainder of any
 * glide, so the ceiling is never exceeded - a soft "brick wall".
 *
 * **Real-time safe:** every buffer is allocated in the constructor, sized for
 * up to `kMaxLookAheadSamples`; process() never allocates, locks or blocks.
 * setEnabled()/setCeilingDb() are atomic and may be called from any thread;
 * setSampleRate()/reset() must not race process().
 *
 * The output is delayed by `latencySamples()` while enabled; disabled, it
 * passes the input through untouched and undelayed.
 *
 * Built with `LookAhead::Off` (live playback's version, so the playhead stays
 * sample-exact) there is no delay at all: the gain drops instantly to what
 * the current sample needs and recovers with the same release - a little
 * harsher on sharp transients than the look-ahead, but never over the ceiling.
 */
class MasterLimiter {
public:
    /// @brief The longest look-ahead the preallocated buffers allow.
    static constexpr std::size_t kMaxLookAheadSamples = 1024;

    /// @brief Whether the limiter looks ahead (and so delays its output).
    enum class LookAhead {
        On,   ///< 1.5 ms look-ahead, smooth attack, `latencySamples()` delay.
        Off,  ///< Instant attack, no delay - `latencySamples()` is 0.
    };

    /// @brief Builds an enabled limiter at 44.1 kHz with the default ceiling.
    /// @param lookAhead Whether to look ahead - see `LookAhead`.
    explicit MasterLimiter(LookAhead lookAhead = LookAhead::On);

    /// @brief Sets the sample rate - derives the look-ahead (1.5 ms) and
    ///        release - and resets the state. Not real-time safe to race
    ///        process(); allocation-free.
    /// @param sampleRateHz The rate; non-positive is ignored.
    void setSampleRate(double sampleRateHz) noexcept;

    /// @brief Turns limiting on or off (atomic).
    /// @param enabled Whether to limit.
    void setEnabled(bool enabled) noexcept { enabled_.store(enabled, std::memory_order_relaxed); }

    /// @brief Whether limiting is on.
    /// @return The flag.
    [[nodiscard]] bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }

    /// @brief Sets the ceiling (atomic), clamped to `[-24, 0]` dBFS.
    /// @param ceilingDb The ceiling, dBFS.
    void setCeilingDb(float ceilingDb) noexcept;

    /// @brief The ceiling.
    /// @return The ceiling, dBFS.
    [[nodiscard]] float ceilingDb() const noexcept { return ceilingDb_.load(std::memory_order_relaxed); }

    /// @brief How many samples the output lags the input while enabled.
    /// @return The look-ahead, in samples (`0` with `LookAhead::Off`).
    [[nodiscard]] std::size_t latencySamples() const noexcept { return lookAheadOn_ ? lookAhead_ : 0; }

    /// @brief Clears the delay line and gain state (e.g. after a seek).
    void reset() noexcept;

    /**
     * @brief Limits `count` samples in place.
     * @param left The left channel.
     * @param right The right channel, or `nullptr` for mono.
     * @param count How many samples.
     * @note Real-time safe.
     */
    void process(float* left, float* right, std::size_t count) noexcept;

private:
    std::atomic<bool> enabled_{true};
    std::atomic<float> ceilingDb_{kDefaultLimiterCeilingDb};
    bool lookAheadOn_ = true;
    std::size_t lookAhead_ = 66;
    double attack_ = 0.0;
    double release_ = 0.0;
    std::vector<float> delayLeft_;
    std::vector<float> delayRight_;
    std::vector<float> targets_;
    std::size_t head_ = 0;
    double gain_ = 1.0;
};

/**
 * @brief Runs `audio` through a fresh `MasterLimiter` at `ceilingDb`, with
 *        its look-ahead compensated (the result lines up with the input and
 *        keeps its length) - the export path's version.
 * @param audio The audio to limit in place.
 * @param ceilingDb The ceiling, dBFS.
 */
void applyMasterLimiter(sound_mind::codec::AudioBuffer& audio, float ceilingDb = kDefaultLimiterCeilingDb);

}  // namespace sound_mind::core
