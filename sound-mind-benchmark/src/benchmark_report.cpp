#include "sound_mind/benchmark/benchmark_report.h"

namespace sound_mind::benchmark {

namespace {

nlohmann::json toJson(const TimingStatistics& timing) {
    nlohmann::json json;
    json["meanMs"] = timing.meanMs;
    json["minMs"] = timing.minMs;
    json["maxMs"] = timing.maxMs;
    json["stddevMs"] = timing.stddevMs;
    json["sampleCount"] = timing.sampleCount;
    return json;
}

nlohmann::json toJson(const BenchmarkResult& result) {
    nlohmann::json json;
    json["name"] = result.name;
    json["category"] = result.category;
    json["parameters"] = result.parameters;
    json["gpuEnabled"] = result.gpuEnabled;
    json["timing"] = toJson(result.timing);
    return json;
}

}  // namespace

nlohmann::json toJson(const BenchmarkReport& report) {
    nlohmann::json json;
    json["schemaVersion"] = BenchmarkReport::kSchemaVersion;
    json["generatedAtUtc"] = report.generatedAtUtc;
    json["gitCommit"] = report.gitCommit;
    json["buildConfiguration"] = report.buildConfiguration;
    json["results"] = nlohmann::json::array();
    for (const BenchmarkResult& result : report.results) {
        json["results"].push_back(toJson(result));
    }
    return json;
}

}  // namespace sound_mind::benchmark
