/**
 * @file main.cpp
 * @brief Runs every registered `BenchmarkCase` and writes a JSON report -
 *        see `docs/sound-mind-benchmarking.md`.
 *
 * Build and run the **Release** configuration for meaningful numbers - a
 * Debug build's own timings (no optimization, iterator/bounds-checking
 * overhead) don't reflect what a real user's own machine experiences, and
 * this program warns loudly at startup if it detects it wasn't.
 */

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "sound_mind/benchmark/benchmark_report.h"
#include "sound_mind/benchmark/benchmark_timer.h"
#include "sound_mind/benchmark/scenarios.h"
#include "sound_mind/core/gpu_compute_availability.h"

namespace {

std::string currentUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm utcTime{};
#if defined(_WIN32)
    gmtime_s(&utcTime, &nowTimeT);
#else
    gmtime_r(&nowTimeT, &utcTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

/// @brief A filesystem-safe version of `currentUtcTimestamp()` (no colons,
/// which Windows paths reject) - used for the report's own file name.
std::string timestampForFileName(const std::string& isoTimestamp) {
    std::string result = isoTimestamp;
    for (char& c : result) {
        if (c == ':') {
            c = '-';
        }
    }
    return result;
}

void appendAllScenarios(std::vector<sound_mind::benchmark::BenchmarkCase>& cases) {
    const auto append = [&cases](std::vector<sound_mind::benchmark::BenchmarkCase> more) {
        cases.insert(cases.end(), std::make_move_iterator(more.begin()), std::make_move_iterator(more.end()));
    };
    append(sound_mind::benchmark::buildFilterScenarios());
    append(sound_mind::benchmark::buildPaintScenarios());
    append(sound_mind::benchmark::buildMindWaveScenarios());
    append(sound_mind::benchmark::buildCompositingScenarios());
}

}  // namespace

/// @brief Entry point - see this file's own `@file` docs above.
/// @return Always `0` - this tool has no failure mode of its own to
///         signal; a case's own exception (if any) propagates and
///         terminates the process instead of being caught here.
int main() {
    if (std::string(SOUND_MIND_BENCHMARK_BUILD_CONFIG) != "Release") {
        std::cerr << "WARNING: this is a '" << SOUND_MIND_BENCHMARK_BUILD_CONFIG
                  << "' build, not Release - timings below are NOT representative of real-world performance. "
                     "Build and run the Release configuration instead.\n\n";
    }

    std::vector<sound_mind::benchmark::BenchmarkCase> cases;
    appendAllScenarios(cases);

    sound_mind::benchmark::BenchmarkReport report;
    report.generatedAtUtc = currentUtcTimestamp();
    report.gitCommit = SOUND_MIND_BENCHMARK_GIT_COMMIT;
    report.buildConfiguration = SOUND_MIND_BENCHMARK_BUILD_CONFIG;

    std::cout << "Running " << cases.size() << " benchmark cases (plus a CPU-forced re-run for each GPU-eligible "
              << "one)...\n\n";

    for (const sound_mind::benchmark::BenchmarkCase& benchmarkCase : cases) {
        std::cout << "[" << benchmarkCase.category << "] " << benchmarkCase.name << " ... " << std::flush;

        const auto timing = sound_mind::benchmark::measure(benchmarkCase.run);
        std::cout << timing.meanMs << " ms (mean of " << timing.sampleCount << ")";

        sound_mind::benchmark::BenchmarkResult result;
        result.name = benchmarkCase.name;
        result.category = benchmarkCase.category;
        result.parameters = benchmarkCase.parameters;
        result.gpuEnabled = benchmarkCase.gpuEligible;
        result.timing = timing;
        report.results.push_back(result);

        if (benchmarkCase.gpuEligible) {
            sound_mind::core::setHardwareAccelerationEnabled(false);
            const auto cpuTiming = sound_mind::benchmark::measure(benchmarkCase.run);
            sound_mind::core::setHardwareAccelerationEnabled(true);
            std::cout << " | CPU-forced: " << cpuTiming.meanMs << " ms (mean of " << cpuTiming.sampleCount << ")";

            sound_mind::benchmark::BenchmarkResult cpuResult;
            cpuResult.name = benchmarkCase.name;
            cpuResult.category = benchmarkCase.category;
            cpuResult.parameters = benchmarkCase.parameters;
            cpuResult.gpuEnabled = false;
            cpuResult.timing = cpuTiming;
            report.results.push_back(cpuResult);
        }
        std::cout << "\n";
    }

    const std::filesystem::path resultsDir = "benchmark-results";
    std::filesystem::create_directories(resultsDir);
    const std::filesystem::path reportPath =
        resultsDir / ("report-" + timestampForFileName(report.generatedAtUtc) + ".json");

    std::ofstream file(reportPath);
    file << sound_mind::benchmark::toJson(report).dump(2);
    file.close();

    std::cout << "\nWrote " << report.results.size() << " results to " << reportPath.string() << "\n";
    return 0;
}
