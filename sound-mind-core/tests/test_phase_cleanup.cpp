#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/phase_cleanup.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyPhaseCleanup;

namespace {

StreamCodecConfig makeTestConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;  // ~10ms/frame.
    config.binCount = 4;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

StreamImage makeBlankContent(const StreamCodecConfig& config, std::uint32_t frameCount) {
    StreamImage content;
    content.config = config;
    content.frameCount = frameCount;
    content.leftMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    content.rightMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    content.sharedPhaseRadians.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    return content;
}

}  // namespace

TEST_CASE("applyPhaseCleanup zeroes phase where both channels are at or below the silence floor",
          "[core][phase_cleanup]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 2);

    // Cell 0: both channels silent, with nonzero "junk" phase to clean up.
    content.leftMagnitudeDb[0] = -96.0f;
    content.rightMagnitudeDb[0] = -96.0f;
    content.sharedPhaseRadians[0] = 2.5f;

    applyPhaseCleanup(content);

    CHECK(content.sharedPhaseRadians[0] == 0.0f);
}

TEST_CASE("applyPhaseCleanup leaves a cell untouched when either channel is louder than the silence floor",
          "[core][phase_cleanup]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 2);

    // Cell 1: left channel is audible, right is silent - the cell as a whole
    // still carries real signal, so its phase must survive untouched.
    content.leftMagnitudeDb[1] = -40.0f;
    content.rightMagnitudeDb[1] = -96.0f;
    content.sharedPhaseRadians[1] = 1.25f;

    applyPhaseCleanup(content);

    CHECK(content.sharedPhaseRadians[1] == 1.25f);
}

TEST_CASE("applyPhaseCleanup leaves a cell untouched when both channels are only slightly above the floor",
          "[core][phase_cleanup]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 1);

    content.leftMagnitudeDb[0] = -95.9f;
    content.rightMagnitudeDb[0] = -95.9f;
    content.sharedPhaseRadians[0] = 0.75f;

    applyPhaseCleanup(content);

    CHECK(content.sharedPhaseRadians[0] == 0.75f);
}

TEST_CASE("applyPhaseCleanup only rewrites phase, never the magnitude channels themselves",
          "[core][phase_cleanup]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 1);

    content.leftMagnitudeDb[0] = -120.0f;
    content.rightMagnitudeDb[0] = -200.0f;
    content.sharedPhaseRadians[0] = -1.5f;

    applyPhaseCleanup(content);

    CHECK(content.leftMagnitudeDb[0] == -120.0f);
    CHECK(content.rightMagnitudeDb[0] == -200.0f);
    CHECK(content.sharedPhaseRadians[0] == 0.0f);
}
