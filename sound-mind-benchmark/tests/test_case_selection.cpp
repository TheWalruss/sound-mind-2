#include <catch2/catch_test_macros.hpp>

#include "sound_mind/benchmark/scenarios.h"

using sound_mind::benchmark::BenchmarkCase;
using sound_mind::benchmark::casesInCategories;

namespace {

std::vector<BenchmarkCase> threeCases() {
    return {BenchmarkCase{"a", "codec", {}, false, [] {}}, BenchmarkCase{"b", "filter", {}, false, [] {}},
            BenchmarkCase{"c", "analysis", {}, false, [] {}}};
}

}  // namespace

TEST_CASE("casesInCategories keeps only the named categories, in order", "[benchmark][selection]") {
    const auto kept = casesInCategories(threeCases(), {"analysis", "codec"});
    REQUIRE(kept.size() == 2);
    CHECK(kept[0].name == "a");
    CHECK(kept[1].name == "c");
}

TEST_CASE("casesInCategories keeps everything for an empty filter", "[benchmark][selection]") {
    CHECK(casesInCategories(threeCases(), {}).size() == 3);
    CHECK(casesInCategories(threeCases(), {"nothing"}).empty());
}

TEST_CASE("The new scenario builders produce cases in their own categories", "[benchmark][selection]") {
    for (const auto& benchmarkCase : sound_mind::benchmark::buildCodecScenarios()) {
        CHECK(benchmarkCase.category == "codec");
    }
    CHECK(sound_mind::benchmark::buildCodecScenarios().size() == 6);
    const auto audio = sound_mind::benchmark::musicLikeAudio(0.5, 44100, 1);
    CHECK(audio.frameCount() == 22050);
    CHECK(audio.right.size() == audio.left.size());
}
