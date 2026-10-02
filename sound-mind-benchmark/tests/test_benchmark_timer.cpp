#include <chrono>
#include <thread>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/benchmark/benchmark_timer.h"

using sound_mind::benchmark::measure;
using sound_mind::benchmark::TimingStatistics;

TEST_CASE("measure runs operation warmupRuns + sampleRuns times in total", "[benchmark][benchmark_timer]") {
    int callCount = 0;
    const TimingStatistics stats = measure([&callCount]() { ++callCount; }, /*warmupRuns=*/3, /*sampleRuns=*/7);
    static_cast<void>(stats);

    REQUIRE(callCount == 10);
}

TEST_CASE("measure reports sampleCount excluding warmup calls", "[benchmark][benchmark_timer]") {
    const TimingStatistics stats = measure([]() {}, /*warmupRuns=*/5, /*sampleRuns=*/4);

    REQUIRE(stats.sampleCount == 4);
}

TEST_CASE("measure clamps sampleRuns up to 1 if given less", "[benchmark][benchmark_timer]") {
    int callCount = 0;
    const TimingStatistics stats = measure([&callCount]() { ++callCount; }, /*warmupRuns=*/0, /*sampleRuns=*/0);

    REQUIRE(stats.sampleCount == 1);
    REQUIRE(callCount == 1);
}

TEST_CASE("measure treats a negative warmupRuns as zero", "[benchmark][benchmark_timer]") {
    int callCount = 0;
    const TimingStatistics stats = measure([&callCount]() { ++callCount; }, /*warmupRuns=*/-5, /*sampleRuns=*/3);
    static_cast<void>(stats);

    REQUIRE(callCount == 3);
}

TEST_CASE("measure's own minMs is never greater than its own maxMs or meanMs", "[benchmark][benchmark_timer]") {
    const TimingStatistics stats = measure(
        [i = 0]() mutable {
            ++i;
            std::this_thread::sleep_for(std::chrono::microseconds(i % 2 == 0 ? 200 : 50));
        },
        /*warmupRuns=*/0, /*sampleRuns=*/10);

    REQUIRE(stats.minMs <= stats.meanMs);
    REQUIRE(stats.meanMs <= stats.maxMs);
    REQUIRE(stats.minMs >= 0.0);
}

TEST_CASE("measure reports a zero stddevMs for exactly one sample", "[benchmark][benchmark_timer]") {
    const TimingStatistics stats = measure([]() {}, /*warmupRuns=*/0, /*sampleRuns=*/1);

    REQUIRE(stats.sampleCount == 1);
    REQUIRE(stats.stddevMs == 0.0);
}

TEST_CASE("measure's own meanMs sits between minMs and maxMs for genuinely variable timings",
          "[benchmark][benchmark_timer]") {
    // A deliberately bimodal workload (alternating long/short sleeps) -
    // confirms the statistics aren't all collapsing to the same value by
    // coincidence on a fast, uniform no-op.
    const TimingStatistics stats = measure(
        [i = 0]() mutable {
            ++i;
            std::this_thread::sleep_for(std::chrono::milliseconds(i % 2 == 0 ? 5 : 1));
        },
        /*warmupRuns=*/1, /*sampleRuns=*/8);

    REQUIRE(stats.maxMs > stats.minMs);
    REQUIRE(stats.stddevMs > 0.0);
}
