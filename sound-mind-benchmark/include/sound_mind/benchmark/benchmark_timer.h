#pragma once

#include <cstddef>
#include <functional>

namespace sound_mind::benchmark {

/**
 * @brief One `BenchmarkCase`'s own measured timing, in milliseconds,
 *        across `sampleCount` repeated calls - see `measure()`'s own docs.
 */
struct TimingStatistics {
    /// @brief Arithmetic mean across every sampled call.
    double meanMs = 0.0;
    /// @brief The fastest sampled call - often the most representative
    ///        single number for "how fast can this actually go", since
    ///        slower samples are more likely to reflect an unrelated
    ///        scheduling hiccup than this operation getting genuinely
    ///        slower.
    double minMs = 0.0;
    /// @brief The slowest sampled call.
    double maxMs = 0.0;
    /// @brief Sample standard deviation (`n - 1` denominator); `0.0` if
    ///        `sampleCount < 2` (nothing to compute a spread over).
    double stddevMs = 0.0;
    /// @brief How many timed calls these statistics were computed over
    ///        (after discarding warmup calls) - always at least `1`.
    std::size_t sampleCount = 0;
};

/**
 * @brief Times `operation`, called `warmupRuns` times first (discarded,
 *        letting allocators/caches/branch predictors reach steady state)
 *        then `sampleRuns` times (timed) - see `BenchmarkCase::run`'s own
 *        docs on the "safe to call repeatedly" contract this relies on.
 *
 * Uses `std::chrono::steady_clock` (monotonic, immune to wall-clock
 * adjustments mid-run - the only sensible choice for measuring an
 * elapsed duration, as opposed to `system_clock`'s own "what time is it"
 * role).
 *
 * @param operation The callable to time.
 * @param warmupRuns How many untimed calls to make first; a negative
 *        value is treated as `0`.
 * @param sampleRuns How many timed calls to make; clamped up to `1` if
 *        given less (there is no meaningful "statistics over zero
 *        samples" result).
 * @return The resulting statistics.
 */
[[nodiscard]] TimingStatistics measure(const std::function<void()>& operation, int warmupRuns = 2,
                                        int sampleRuns = 5);

}  // namespace sound_mind::benchmark
