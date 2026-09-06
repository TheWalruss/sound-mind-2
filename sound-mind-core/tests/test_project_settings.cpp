#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/project_settings.h"

using sound_mind::core::FrequencyScale;
using sound_mind::core::ProjectSettings;

TEST_CASE("ProjectSettings has sensible defaults", "[core][project_settings]") {
    const ProjectSettings settings;
    REQUIRE(settings.sampleRateHz == 44100);
    REQUIRE(settings.frequencyScale == FrequencyScale::Log);
    REQUIRE(settings.canvasWidth == 1024);
    REQUIRE(settings.canvasHeight == 512);
    REQUIRE(settings.referenceHz == 440.0);
}

TEST_CASE("ProjectSettings round-trips through JSON", "[core][project_settings]") {
    ProjectSettings original;
    original.sampleRateHz = 48000;
    original.timestepMs = 5.0;
    original.canvasWidth = 2048;
    original.canvasHeight = 256;
    original.referenceHz = 432.0;
    original.defaultTempoBpm = 90.0;

    const nlohmann::json json = original;
    const ProjectSettings restored = json.get<ProjectSettings>();

    REQUIRE(restored.sampleRateHz == original.sampleRateHz);
    REQUIRE(restored.frequencyScale == original.frequencyScale);
    REQUIRE(restored.timestepMs == original.timestepMs);
    REQUIRE(restored.canvasWidth == original.canvasWidth);
    REQUIRE(restored.canvasHeight == original.canvasHeight);
    REQUIRE(restored.referenceHz == original.referenceHz);
    REQUIRE(restored.defaultTempoBpm == original.defaultTempoBpm);
}

TEST_CASE("FrequencyScale serializes to a readable string", "[core][project_settings]") {
    const nlohmann::json json = FrequencyScale::Log;
    REQUIRE(json == "log");
}
