#include <catch2/catch_test_macros.hpp>

#include "sound_mind/benchmark/benchmark_report.h"

using sound_mind::benchmark::BenchmarkReport;
using sound_mind::benchmark::BenchmarkResult;
using sound_mind::benchmark::TimingStatistics;
using sound_mind::benchmark::toJson;

namespace {

BenchmarkResult makeResult(const std::string& name) {
    BenchmarkResult result;
    result.name = name;
    result.category = "filter";
    result.parameters = {{"canvas", "medium"}};
    result.gpuEnabled = true;
    result.timing = TimingStatistics{10.0, 8.0, 14.0, 1.5, 5};
    return result;
}

}  // namespace

TEST_CASE("toJson includes the report's own schema version and metadata", "[benchmark][benchmark_report]") {
    BenchmarkReport report;
    report.generatedAtUtc = "2026-10-02T12:00:00Z";
    report.gitCommit = "abc1234";
    report.buildConfiguration = "Release";

    const auto json = toJson(report);

    REQUIRE(json.at("schemaVersion") == BenchmarkReport::kSchemaVersion);
    REQUIRE(json.at("generatedAtUtc") == "2026-10-02T12:00:00Z");
    REQUIRE(json.at("gitCommit") == "abc1234");
    REQUIRE(json.at("buildConfiguration") == "Release");
    REQUIRE(json.at("results").empty());
}

TEST_CASE("toJson serializes every result, preserving order", "[benchmark][benchmark_report]") {
    BenchmarkReport report;
    report.results.push_back(makeResult("first"));
    report.results.push_back(makeResult("second"));

    const auto json = toJson(report);

    REQUIRE(json.at("results").size() == 2);
    REQUIRE(json.at("results").at(0).at("name") == "first");
    REQUIRE(json.at("results").at(1).at("name") == "second");
}

TEST_CASE("toJson preserves a result's own parameters, gpuEnabled flag, and timing fields",
          "[benchmark][benchmark_report]") {
    BenchmarkReport report;
    report.results.push_back(makeResult("UniformBlur"));

    const auto json = toJson(report);
    const auto& result = json.at("results").at(0);

    REQUIRE(result.at("category") == "filter");
    REQUIRE(result.at("parameters").at("canvas") == "medium");
    REQUIRE(result.at("gpuEnabled") == true);
    REQUIRE(result.at("timing").at("meanMs") == 10.0);
    REQUIRE(result.at("timing").at("minMs") == 8.0);
    REQUIRE(result.at("timing").at("maxMs") == 14.0);
    REQUIRE(result.at("timing").at("stddevMs") == 1.5);
    REQUIRE(result.at("timing").at("sampleCount") == 5);
}
