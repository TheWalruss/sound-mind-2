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
    original.fundamentalFrequencyHz = 261.63;
    original.startTimeOffsetSeconds = 0.05;

    const nlohmann::json json = original;
    const auto restored = json.get<NamedMindShot>();

    REQUIRE(restored.id == original.id);
    REQUIRE(restored.name == original.name);
    REQUIRE(restored.clip.frameCount == original.clip.frameCount);
    REQUIRE(restored.clip.binCount == original.clip.binCount);
    REQUIRE(restored.clip.leftMagnitudeDb == original.clip.leftMagnitudeDb);
    REQUIRE(restored.clip.rightMagnitudeDb == original.clip.rightMagnitudeDb);
    REQUIRE(restored.clip.sharedPhaseRadians == original.clip.sharedPhaseRadians);
    REQUIRE(restored.fundamentalFrequencyHz == original.fundamentalFrequencyHz);
    REQUIRE(restored.startTimeOffsetSeconds == original.startTimeOffsetSeconds);
}

TEST_CASE("A fresh NamedMindShot's fundamentalFrequencyHz/startTimeOffsetSeconds are both unset (0.0)",
          "[core][mind_shot]") {
    const NamedMindShot fresh;
    REQUIRE(fresh.fundamentalFrequencyHz == 0.0);
    REQUIRE(fresh.startTimeOffsetSeconds == 0.0);
}

TEST_CASE("A NamedMindShot loaded from JSON with no fundamentalFrequencyHz/startTimeOffsetSeconds keys falls back "
          "to unset",
          "[core][mind_shot]") {
    // A project saved before v0.Y.55.1's own prerequisite existed.
    nlohmann::json json = NamedMindShot{};
    json["id"] = 3;
    json["name"] = "Old Shot";
    json["clip"] = makeTestClip();
    json.erase("fundamentalFrequencyHz");
    json.erase("startTimeOffsetSeconds");

    const auto restored = json.get<NamedMindShot>();

    REQUIRE(restored.fundamentalFrequencyHz == 0.0);
    REQUIRE(restored.startTimeOffsetSeconds == 0.0);
}
