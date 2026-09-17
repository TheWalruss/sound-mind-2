#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/mind_shot.h"

using sound_mind::core::Clip;
using sound_mind::core::NamedMindShot;

namespace {

Clip makeTestClip() {
    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-5.0f, -6.0f, -7.0f, -8.0f};
    clip.sharedPhaseRadians = {0.1f, 0.2f, 0.3f, 0.4f};
    return clip;
}

}  // namespace

TEST_CASE("A NamedMindShot round-trips through JSON unchanged", "[core][mind_shot]") {
    NamedMindShot original;
    original.id = 7;
    original.name = "Piano Hit";
    original.clip = makeTestClip();

    const nlohmann::json json = original;
    const auto restored = json.get<NamedMindShot>();

    REQUIRE(restored.id == original.id);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.clip.frameCount == original.clip.frameCount);
    REQUIRE(restored.clip.binCount == original.clip.binCount);
    REQUIRE(restored.clip.leftMagnitudeDb == original.clip.leftMagnitudeDb);
    REQUIRE(restored.clip.rightMagnitudeDb == original.clip.rightMagnitudeDb);
    REQUIRE(restored.clip.sharedPhaseRadians == original.clip.sharedPhaseRadians);
}
