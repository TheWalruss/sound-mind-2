#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
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
    REQUIRE(settings.binCount == 512);
    REQUIRE(settings.minFrequencyHz == 20.0f);
    REQUIRE(settings.maxFrequencyHz == 16000.0f);
}

TEST_CASE("ProjectSettings round-trips through JSON", "[core][project_settings]") {
    ProjectSettings original;
    original.sampleRateHz = 48000;
    original.timestepMs = 5.0;
    original.canvasWidth = 2048;
    original.canvasHeight = 256;
    original.referenceHz = 432.0;
    original.defaultTempoBpm = 90.0;
    original.binCount = 256;
    original.minFrequencyHz = 30.0f;
    original.maxFrequencyHz = 18000.0f;

    const nlohmann::json json = original;
    const ProjectSettings restored = json.get<ProjectSettings>();

    REQUIRE(restored.sampleRateHz == original.sampleRateHz);
    REQUIRE(restored.frequencyScale == original.frequencyScale);
    REQUIRE(restored.timestepMs == original.timestepMs);
    REQUIRE(restored.canvasWidth == original.canvasWidth);
    REQUIRE(restored.canvasHeight == original.canvasHeight);
    REQUIRE(restored.referenceHz == original.referenceHz);
    REQUIRE(restored.defaultTempoBpm == original.defaultTempoBpm);
    REQUIRE(restored.binCount == original.binCount);
    REQUIRE(restored.minFrequencyHz == original.minFrequencyHz);
    REQUIRE(restored.maxFrequencyHz == original.maxFrequencyHz);
}

TEST_CASE("streamCodecConfigFor carries sample rate, bin count, and frequency range through unchanged",
          "[core][project_settings]") {
    ProjectSettings settings;
    settings.sampleRateHz = 48000;
    settings.binCount = 256;
    settings.minFrequencyHz = 30.0f;
    settings.maxFrequencyHz = 18000.0f;

    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    REQUIRE(config.sampleRateHz == settings.sampleRateHz);
    REQUIRE(config.binCount == settings.binCount);
    REQUIRE(config.minFrequencyHz == settings.minFrequencyHz);
    REQUIRE(config.maxFrequencyHz == settings.maxFrequencyHz);
}

TEST_CASE("streamCodecConfigFor derives hopLength from timestepMs and sampleRateHz", "[core][project_settings]") {
    ProjectSettings settings;
    settings.sampleRateHz = 44100;
    settings.timestepMs = 10.0;

    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    // 10ms at 44100Hz - matches stream_codec.h's own default hopLength docs.
    REQUIRE(config.hopLength == 441);
}

TEST_CASE("streamCodecConfigFor matches StreamCodecConfig's own defaults for a default-constructed ProjectSettings",
          "[core][project_settings]") {
    const auto config = sound_mind::core::streamCodecConfigFor(ProjectSettings{});
    const sound_mind::codec::StreamCodecConfig defaultConfig{};

    REQUIRE(config.sampleRateHz == defaultConfig.sampleRateHz);
    REQUIRE(config.hopLength == defaultConfig.hopLength);
    REQUIRE(config.binCount == defaultConfig.binCount);
    REQUIRE(config.minFrequencyHz == defaultConfig.minFrequencyHz);
    REQUIRE(config.maxFrequencyHz == defaultConfig.maxFrequencyHz);
}

TEST_CASE("ProjectSettings loads from JSON missing binCount/minFrequencyHz/maxFrequencyHz (a project saved before "
          "v0.Y.11.1) using their defaults",
          "[core][project_settings]") {
    nlohmann::json json{
        {"sampleRateHz", 44100},   {"frequencyScale", "log"}, {"timestepMs", 10.0},
        {"canvasWidth", 1024},     {"canvasHeight", 512},     {"referenceHz", 440.0},
        {"defaultTempoBpm", 120.0},
    };

    const ProjectSettings restored = json.get<ProjectSettings>();

    REQUIRE(restored.binCount == ProjectSettings{}.binCount);
    REQUIRE(restored.minFrequencyHz == ProjectSettings{}.minFrequencyHz);
    REQUIRE(restored.maxFrequencyHz == ProjectSettings{}.maxFrequencyHz);
}

TEST_CASE("FrequencyScale serializes to a readable string", "[core][project_settings]") {
    const nlohmann::json json = FrequencyScale::Log;
    REQUIRE(json == "log");
}
