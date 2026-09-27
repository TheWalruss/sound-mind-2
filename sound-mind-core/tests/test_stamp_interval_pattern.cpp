#include <stdexcept>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/stamp_interval_pattern.h"

using sound_mind::core::parseStampIntervalPattern;
using sound_mind::core::resolveStampIntervalPattern;
using sound_mind::core::stampIntervalPatternFor;
using sound_mind::core::StampIntervalToken;

namespace {
constexpr double kBpm = 120.0;  // 0.5s per beat.
}  // namespace

TEST_CASE("parseStampIntervalPattern parses a single millisecond token", "[core][stamp_interval_pattern]") {
    const auto tokens = parseStampIntervalPattern("100ms");

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].value == Catch::Approx(100.0));
    CHECK(tokens[0].isBeats == false);
}

TEST_CASE("parseStampIntervalPattern parses a single beats token", "[core][stamp_interval_pattern]") {
    const auto tokens = parseStampIntervalPattern("2b");

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].value == Catch::Approx(2.0));
    CHECK(tokens[0].isBeats == true);
}

TEST_CASE("parseStampIntervalPattern parses multiple alternating tokens in order",
          "[core][stamp_interval_pattern]") {
    const auto tokens = parseStampIntervalPattern("100ms 200ms 50ms");

    REQUIRE(tokens.size() == 3);
    CHECK(tokens[0].value == Catch::Approx(100.0));
    CHECK(tokens[1].value == Catch::Approx(200.0));
    CHECK(tokens[2].value == Catch::Approx(50.0));
}

TEST_CASE("parseStampIntervalPattern allows mixing ms and b tokens in one pattern",
          "[core][stamp_interval_pattern]") {
    const auto tokens = parseStampIntervalPattern("100ms 1b");

    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].isBeats == false);
    CHECK(tokens[1].isBeats == true);
}

TEST_CASE("parseStampIntervalPattern tolerates extra whitespace between tokens",
          "[core][stamp_interval_pattern]") {
    const auto tokens = parseStampIntervalPattern("  100ms    200ms  ");

    REQUIRE(tokens.size() == 2);
}

TEST_CASE("parseStampIntervalPattern rejects an empty pattern", "[core][stamp_interval_pattern]") {
    CHECK_THROWS_AS(parseStampIntervalPattern(""), std::invalid_argument);
    CHECK_THROWS_AS(parseStampIntervalPattern("   "), std::invalid_argument);
}

TEST_CASE("parseStampIntervalPattern rejects a token with no unit suffix", "[core][stamp_interval_pattern]") {
    CHECK_THROWS_AS(parseStampIntervalPattern("100"), std::invalid_argument);
}

TEST_CASE("parseStampIntervalPattern rejects a non-positive value", "[core][stamp_interval_pattern]") {
    CHECK_THROWS_AS(parseStampIntervalPattern("0ms"), std::invalid_argument);
    CHECK_THROWS_AS(parseStampIntervalPattern("-5ms"), std::invalid_argument);
}

TEST_CASE("parseStampIntervalPattern rejects a malformed number", "[core][stamp_interval_pattern]") {
    CHECK_THROWS_AS(parseStampIntervalPattern("abcms"), std::invalid_argument);
}

TEST_CASE("stampIntervalPatternFor round-trips through parseStampIntervalPattern", "[core][stamp_interval_pattern]") {
    const std::vector<StampIntervalToken> tokens = {{100.0, false}, {2.0, true}, {50.0, false}};

    const std::string rendered = stampIntervalPatternFor(tokens);
    const auto reparsed = parseStampIntervalPattern(rendered);

    REQUIRE(reparsed.size() == 3);
    CHECK(reparsed[0].value == Catch::Approx(100.0));
    CHECK(reparsed[0].isBeats == false);
    CHECK(reparsed[1].value == Catch::Approx(2.0));
    CHECK(reparsed[1].isBeats == true);
    CHECK(reparsed[2].value == Catch::Approx(50.0));
    CHECK(reparsed[2].isBeats == false);
}

TEST_CASE("stampIntervalPatternFor returns empty text for an empty token list", "[core][stamp_interval_pattern]") {
    CHECK(stampIntervalPatternFor({}).empty());
}

TEST_CASE("resolveStampIntervalPattern converts milliseconds to seconds", "[core][stamp_interval_pattern]") {
    const std::vector<StampIntervalToken> tokens = {{100.0, false}, {250.0, false}};

    const auto seconds = resolveStampIntervalPattern(tokens, kBpm);

    REQUIRE(seconds.size() == 2);
    CHECK(seconds[0] == Catch::Approx(0.1));
    CHECK(seconds[1] == Catch::Approx(0.25));
}

TEST_CASE("resolveStampIntervalPattern converts beats to seconds using the given tempo",
          "[core][stamp_interval_pattern]") {
    const std::vector<StampIntervalToken> tokens = {{1.0, true}, {2.0, true}};

    const auto seconds = resolveStampIntervalPattern(tokens, kBpm);  // 120 bpm -> 0.5s per beat.

    REQUIRE(seconds.size() == 2);
    CHECK(seconds[0] == Catch::Approx(0.5));
    CHECK(seconds[1] == Catch::Approx(1.0));
}

TEST_CASE("resolveStampIntervalPattern handles a mixed pattern", "[core][stamp_interval_pattern]") {
    const std::vector<StampIntervalToken> tokens = {{100.0, false}, {1.0, true}};

    const auto seconds = resolveStampIntervalPattern(tokens, kBpm);

    REQUIRE(seconds.size() == 2);
    CHECK(seconds[0] == Catch::Approx(0.1));
    CHECK(seconds[1] == Catch::Approx(0.5));
}
