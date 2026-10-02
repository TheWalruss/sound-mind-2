#include "sound_mind/benchmark/benchmark_timer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

namespace sound_mind::benchmark {

TimingStatistics measure(const std::function<void()>& operation, int warmupRuns, int sampleRuns) {
    for (int i = 0; i < warmupRuns; ++i) {
        operation();
    }

    const int clampedSampleRuns = std::max(1, sampleRuns);
    std::vector<double> samplesMs;
    samplesMs.reserve(static_cast<std::size_t>(clampedSampleRuns));
    for (int i = 0; i < clampedSampleRuns; ++i) {
        const auto start = std::chrono::steady_clock::now();
        operation();
        const auto end = std::chrono::steady_clock::now();
        samplesMs.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    TimingStatistics stats;
    stats.sampleCount = samplesMs.size();
    stats.minMs = *std::min_element(samplesMs.begin(), samplesMs.end());
    stats.maxMs = *std::max_element(samplesMs.begin(), samplesMs.end());
    stats.meanMs = std::accumulate(samplesMs.begin(), samplesMs.end(), 0.0) / static_cast<double>(samplesMs.size());

    if (samplesMs.size() > 1) {
        double sumSquaredDeviation = 0.0;
        for (const double sampleMs : samplesMs) {
            const double deviation = sampleMs - stats.meanMs;
            sumSquaredDeviation += deviation * deviation;
        }
        stats.stddevMs = std::sqrt(sumSquaredDeviation / static_cast<double>(samplesMs.size() - 1));
    }

    return stats;
}

}  // namespace sound_mind::benchmark
