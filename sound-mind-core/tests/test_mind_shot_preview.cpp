#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/core/mind_shot_preview.h"

using namespace sound_mind::core;
using sound_mind::codec::decode;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::toRgbImage;

namespace {

Clip makeTestClip() {
    Clip clip;
    clip.frameCount = 4;
    clip.binCount = 3;
    clip.leftMagnitudeDb.assign(12, -20.0f);
    clip.rightMagnitudeDb.assign(12, -20.0f);
    clip.sharedPhaseRadians.assign(12, 0.0f);
    return clip;
}

}  // namespace

TEST_CASE("streamImageFromClip carries the clip's own shape and cell data verbatim", "[core][mind_shot_preview]") {
    const Clip clip = makeTestClip();
    StreamCodecConfig config;
    config.binCount = 999;  // Deliberately wrong - must be overridden by the clip's own binCount.

    const sound_mind::codec::StreamImage image = streamImageFromClip(clip, config);

    REQUIRE(image.frameCount == clip.frameCount);
    REQUIRE(image.config.binCount == clip.binCount);
    REQUIRE(image.leftMagnitudeDb == clip.leftMagnitudeDb);
    REQUIRE(image.rightMagnitudeDb == clip.rightMagnitudeDb);
    REQUIRE(image.sharedPhaseRadians == clip.sharedPhaseRadians);
    REQUIRE(image.sampleCount == static_cast<std::uint64_t>(clip.frameCount) * config.hopLength);
}

TEST_CASE("streamImageFromClip's result decodes to real audio of the expected length",
          "[core][mind_shot_preview]") {
    const Clip clip = makeTestClip();
    StreamCodecConfig config;
    config.hopLength = 100;

    const sound_mind::codec::StreamImage image = streamImageFromClip(clip, config);
    const sound_mind::codec::AudioBuffer audio = decode(image);

    REQUIRE(audio.frameCount() == image.sampleCount);
    REQUIRE(audio.left.size() == audio.right.size());
}

TEST_CASE("streamImageFromClip's result renders to an RGB image of the expected dimensions",
          "[core][mind_shot_preview]") {
    const Clip clip = makeTestClip();
    const sound_mind::codec::StreamImage image = streamImageFromClip(clip, StreamCodecConfig{});

    const sound_mind::codec::RgbImage rgb = toRgbImage(image);

    REQUIRE(rgb.width == clip.frameCount);
    REQUIRE(rgb.height == clip.binCount);
}
