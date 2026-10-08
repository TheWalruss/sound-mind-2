#pragma once

#include <chrono>
#include <cstdint>
#include <utility>

namespace sound_mind::codec_eval {

/// @brief The current process's peak working-set size, in bytes, since
///        process start - Windows-only (`PROCESS_MEMORY_COUNTERS::
///        PeakWorkingSetSize` via `GetProcessMemoryInfo()`, part of the
///        Windows SDK's `psapi.lib`, not a new third-party dependency).
///        Identical behavior on Arm64 and x64 - a plain Win32 API call,
///        nothing SIMD/architecture-specific.
/// @return The peak working-set size in bytes, or `0` if the query fails.
[[nodiscard]] std::uint64_t peakWorkingSetBytes();

/// @brief One operation's measured wall-clock time and approximate working-
///        set growth.
struct TimedMemoryResult {
    /// @brief Wall-clock elapsed time, in milliseconds.
    double elapsedMs = 0.0;
    /// @brief The process's peak-working-set high-water mark's own growth
    ///        across the call, in bytes - see measureTimeAndMemory()'s own
    ///        docs for why this is an approximation, not an exact
    ///        attribution of memory to this one call.
    std::uint64_t workingSetGrowthBytes = 0;
};

/// @brief Runs `operation` once, timing it and measuring how much the
///        process's peak-working-set high-water mark grew across the call.
///
/// **Approximate, not exact attribution**: `PeakWorkingSetSize` is a
/// whole-process, monotonically-non-decreasing high-water mark Windows
/// itself maintains - there's no Win32 API to reset or scope it to one call,
/// so "this call's own memory" is really "how much higher the process-wide
/// peak got while this call ran," which can read as `0` for a call that
/// allocates and frees within headroom an earlier, larger call already
/// established. Good enough to compare *relative* cost between codec
/// configurations in the same process run (a 1024-bin Pool encode growing
/// the peak far more than a 256-bin one is a real, meaningful signal); not a
/// substitute for a real memory profiler if exact per-call attribution ever
/// matters.
///
/// A single untimed call, not `sound-mind-benchmark`'s own warmup+sample-
/// mean methodology (`benchmark_timer.h`) - this tool's primary metric is
/// fidelity, and a whole-file encode/decode is expensive enough that
/// repeating it several times per (file, codec, hop length, bin count)
/// combination would make a routine sweep impractical; timing/memory here
/// are context for the fidelity numbers, not the object of statistical
/// rigor.
///
/// @param operation A no-argument callable to run exactly once.
/// @return The elapsed time and working-set growth.
template <typename Fn>
[[nodiscard]] TimedMemoryResult measureTimeAndMemory(Fn&& operation) {
    const std::uint64_t before = peakWorkingSetBytes();
    const auto start = std::chrono::steady_clock::now();
    std::forward<Fn>(operation)();
    const auto end = std::chrono::steady_clock::now();
    const std::uint64_t after = peakWorkingSetBytes();

    TimedMemoryResult result;
    result.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    result.workingSetGrowthBytes = (after > before) ? (after - before) : 0;
    return result;
}

}  // namespace sound_mind::codec_eval
