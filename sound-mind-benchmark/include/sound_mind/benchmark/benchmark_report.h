#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/benchmark/benchmark_timer.h"

namespace sound_mind::benchmark {

/**
 * @brief One `BenchmarkCase`'s own finished measurement, ready to be
 *        written out - see `BenchmarkReport`'s own docs.
 */
struct BenchmarkResult {
    /// @brief This result's own `BenchmarkCase::name` - see that field's
    ///        own docs.
    std::string name;
    /// @brief This result's own `BenchmarkCase::category` - see that
    ///        field's own docs.
    std::string category;
    /// @brief This result's own `BenchmarkCase::parameters`, copied
    ///        verbatim - see that field's own docs.
    nlohmann::json parameters;

    /// @brief Whether the GPU-accelerated path was active for this
    ///        specific measurement - see `BenchmarkCase::gpuEligible`'s
    ///        own docs. `false` for every case that isn't GPU-eligible at
    ///        all (meaningless either way); for one that is, `main.cpp`
    ///        produces exactly two `BenchmarkResult`s with the same
    ///        `name`/`category`/`parameters` and opposite `gpuEnabled`
    ///        values, letting a later analysis pass pair them up by
    ///        matching everything else.
    bool gpuEnabled = false;

    /// @brief This measurement's own timing statistics.
    TimingStatistics timing;
};

/**
 * @brief A whole benchmark run's own output - every `BenchmarkResult`
 *        plus metadata identifying when/how it was produced, written to a
 *        single JSON file by `main.cpp` - see
 *        `docs/sound-mind-benchmarking.md`'s own "JSON schema" section
 *        for the full field-by-field reference this struct/`toJson()`
 *        together define.
 *
 * **Write-only, deliberately** - no `fromJson()` exists yet, since
 * nothing in this codebase consumes a report back in yet (the Studio-side
 * "performance hints" feature `docs/sound-mind-design.md`'s own
 * "Performance Hints" section describes is still future work - see that
 * section's own docs on why it's speculative, unimplemented machinery
 * for now). Adding one when that day comes is a small, additive change;
 * writing one now, with nothing to exercise it, would just be unused code.
 */
struct BenchmarkReport {
    /// @brief Bumped whenever a field is added, removed, or changes
    ///        meaning - lets a future consumer detect and reject (or
    ///        migrate) a report produced by an incompatible older/newer
    ///        version of this tool, rather than silently misreading it.
    static constexpr int kSchemaVersion = 1;

    /// @brief When this report was generated, ISO 8601 UTC
    ///        (`YYYY-MM-DDTHH:MM:SSZ`) - lets two reports be ordered/
    ///        compared without relying on filesystem metadata.
    std::string generatedAtUtc;

    /// @brief The short commit hash this binary was built from, or
    ///        `"unknown"` if it couldn't be determined at configure time
    ///        (no git checkout, git not on `PATH`) - see this project's
    ///        own `CMakeLists.txt` for how it's captured.
    std::string gitCommit;

    /// @brief The CMake build configuration this binary was compiled as
    ///        (`"Release"`, `"Debug"`, ...) - timings from anything but a
    ///        genuine `Release` build aren't representative and should be
    ///        flagged as such by whatever reads this report (see
    ///        `docs/sound-mind-benchmarking.md`'s own "Running it" section
    ///        on why `main.cpp` itself also warns about this at runtime).
    std::string buildConfiguration;

    /// @brief Every case this run measured, in the order they ran.
    std::vector<BenchmarkResult> results;
};

/// @brief Serializes `report` to its JSON representation - see
///        `docs/sound-mind-benchmarking.md`'s own "JSON schema" section.
[[nodiscard]] nlohmann::json toJson(const BenchmarkReport& report);

}  // namespace sound_mind::benchmark
