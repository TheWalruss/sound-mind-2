#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/loudness_analysis.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::averageLoudnessDb;
using sound_mind::core::computeLoudnessProfile;
using sound_mind::core::peakLoudnessDb;

namespace {

/// @brief A StreamImage with every bin/frame set to the same left and
///        right dB value, so computeLoudnessProfile()'s own per-column
///        average across bins should equal that same value again -
///        useful as this file's own simplest possible correctness check.
StreamImage makeUniformContent(std::uint32_t frameCount, std::uint32_t binCount, float uniformDb) {
    StreamImage content;
    content.config.binCount = binCount;
    content.frameCount = frameCount;
    content.leftMagnitudeDb.assign(std::size_t{frameCount} * binCount, uniformDb);
    content.rightMagnitudeDb.assign(std::size_t{frameCount} * binCount, uniformDb);
    content.sharedPhaseRadians.assign(std::size_t{frameCount} * binCount, 0.0f);
    return content;
}

}  // namespace

TEST_CASE("computeLoudnessProfile returns one value per frame", "[core][loudness_analysis]") {
    const StreamImage content = makeUniformContent(8, 16, -12.0f);

    const std::vector<float> profile = computeLoudnessProfile(content);

    REQUIRE(profile.size() == content.frameCount);
}

TEST_CASE("computeLoudnessProfile reproduces a uniform level exactly", "[core][loudness_analysis]") {
    const StreamImage content = makeUniformContent(4, 8, -18.0f);

    const std::vector<float> profile = computeLoudnessProfile(content);

    for (const float value : profile) {
        CHECK(value == Catch::Approx(-18.0f).margin(0.01));
    }
}

TEST_CASE("computeLoudnessProfile returns empty for content with no frames or bins", "[core][loudness_analysis]") {
    StreamImage emptyFrames = makeUniformContent(0, 8, -10.0f);
    StreamImage emptyBins = makeUniformContent(8, 0, -10.0f);

    REQUIRE(computeLoudnessProfile(emptyFrames).empty());
    REQUIRE(computeLoudnessProfile(emptyBins).empty());
}

TEST_CASE("computeLoudnessProfile picks up a louder column against a quiet background", "[core][loudness_analysis]") {
    StreamImage content = makeUniformContent(4, 8, -40.0f);
    for (std::uint32_t bin = 0; bin < content.config.binCount; ++bin) {
        const std::size_t index = std::size_t{bin} * content.frameCount + 2;
        content.leftMagnitudeDb[index] = -3.0f;
        content.rightMagnitudeDb[index] = -3.0f;
    }

    const std::vector<float> profile = computeLoudnessProfile(content);

    CHECK(profile[2] > profile[0]);
    CHECK(profile[2] > profile[1]);
    CHECK(profile[2] > profile[3]);
}

TEST_CASE("averageLoudnessDb of a uniform profile equals that same value", "[core][loudness_analysis]") {
    const std::vector<float> profile(6, -20.0f);

    CHECK(averageLoudnessDb(profile) == Catch::Approx(-20.0f).margin(0.01));
}

TEST_CASE("averageLoudnessDb of an empty profile is a very quiet floor value", "[core][loudness_analysis]") {
    CHECK(averageLoudnessDb({}) < -100.0f);
}

TEST_CASE("peakLoudnessDb returns the single loudest column", "[core][loudness_analysis]") {
    const std::vector<float> profile{-30.0f, -12.0f, -25.0f, -40.0f};

    CHECK(peakLoudnessDb(profile) == Catch::Approx(-12.0f).margin(0.01));
}

TEST_CASE("peakLoudnessDb of an empty profile is a very quiet floor value", "[core][loudness_analysis]") {
    CHECK(peakLoudnessDb({}) < -100.0f);
}
