#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/midi_program_mapping.h"

using sound_mind::core::MidiProgramMapping;
using sound_mind::core::ToolPresetId;

TEST_CASE("A fresh MidiProgramMapping has no Tool Preset and no duration/pitch modification",
          "[core][midi_program_mapping]") {
    const MidiProgramMapping fresh;
    REQUIRE(fresh.programNumber == 0);
    REQUIRE_FALSE(fresh.toolPresetId.has_value());
    REQUIRE(fresh.durationScale == 1.0);
    REQUIRE(fresh.pitchOffsetSemitones == 0.0);
}

TEST_CASE("A MidiProgramMapping round-trips through JSON, Tool Preset id included", "[core][midi_program_mapping]") {
    MidiProgramMapping original;
    original.programNumber = 4;
    original.toolPresetId = ToolPresetId{7};
    original.durationScale = 1.5;
    original.pitchOffsetSemitones = -12.0;

    const nlohmann::json json = original;
    const auto restored = json.get<MidiProgramMapping>();

    REQUIRE(restored.programNumber == 4);
    REQUIRE(restored.toolPresetId == ToolPresetId{7});
    REQUIRE(restored.durationScale == 1.5);
    REQUIRE(restored.pitchOffsetSemitones == -12.0);
}

TEST_CASE("A MidiProgramMapping with no Tool Preset round-trips through JSON without a toolPresetId key",
          "[core][midi_program_mapping]") {
    MidiProgramMapping original;
    original.programNumber = 10;

    const nlohmann::json json = original;
    REQUIRE_FALSE(json.contains("toolPresetId"));

    const auto restored = json.get<MidiProgramMapping>();
    REQUIRE_FALSE(restored.toolPresetId.has_value());
}

TEST_CASE("A MidiProgramMapping loads leniently when durationScale/pitchOffsetSemitones are absent",
          "[core][midi_program_mapping]") {
    const nlohmann::json json = {{"programNumber", 2}};

    const auto restored = json.get<MidiProgramMapping>();

    REQUIRE(restored.programNumber == 2);
    REQUIRE(restored.durationScale == 1.0);
    REQUIRE(restored.pitchOffsetSemitones == 0.0);
}
