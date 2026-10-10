#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/grid_preset.h"

using namespace sound_mind::core;

TEST_CASE("GridLinePresetStyle round-trips through JSON", "[core][grid_preset]") {
    GridLinePresetStyle style;
    style.colorR = 10;
    style.colorG = 20;
    style.colorB = 30;
    style.widthPixels = 2.5;
    style.dashStyle = GridLinePresetStyle::DashStyle::Dash;

    const nlohmann::json json = style;
    const GridLinePresetStyle restored = json.get<GridLinePresetStyle>();

    REQUIRE(restored.colorR == 10);
    REQUIRE(restored.colorG == 20);
    REQUIRE(restored.colorB == 30);
    REQUIRE(restored.widthPixels == 2.5);
    REQUIRE(restored.dashStyle == GridLinePresetStyle::DashStyle::Dash);
}

TEST_CASE("FrequencyGridPresetConfig round-trips through JSON", "[core][grid_preset]") {
    FrequencyGridPresetConfig config;
    config.noteGridEnabled = true;
    config.noteGridTemperament = Temperament::Equal24;
    config.noteGridKey = PitchClass::FSharp;
    config.noteGridScale = ScaleType::HarmonicMinor;
    config.noteGridExcludedSteps = {1, 3, 5};
    config.noteGridExcludedOctaves = {-1, 8};
    config.harmonicSeriesEnabled = true;
    config.harmonicFundamentalHz = 55.0;
    config.customFrequenciesEnabled = true;
    config.customFrequenciesHz = {440.0, 880.0};
    config.lineStyle.dashStyle = GridLinePresetStyle::DashStyle::Dot;

    const nlohmann::json json = config;
    const FrequencyGridPresetConfig restored = json.get<FrequencyGridPresetConfig>();

    REQUIRE(restored.noteGridEnabled == true);
    REQUIRE(restored.noteGridTemperament == Temperament::Equal24);
    REQUIRE(restored.noteGridKey == PitchClass::FSharp);
    REQUIRE(restored.noteGridScale == ScaleType::HarmonicMinor);
    REQUIRE(restored.noteGridExcludedSteps == std::set<int>{1, 3, 5});
    REQUIRE(restored.noteGridExcludedOctaves == std::set<int>{-1, 8});
    REQUIRE(restored.harmonicSeriesEnabled == true);
    REQUIRE(restored.harmonicFundamentalHz == 55.0);
    REQUIRE(restored.customFrequenciesEnabled == true);
    REQUIRE(restored.customFrequenciesHz == std::vector<double>{440.0, 880.0});
    REQUIRE(restored.lineStyle.dashStyle == GridLinePresetStyle::DashStyle::Dot);
}

TEST_CASE("TimingGridPresetConfig round-trips through JSON", "[core][grid_preset]") {
    TimingGridPresetConfig config;
    config.mode = GridTimingPresetMode::Tempo;
    config.intervalSeconds = 2.0;
    config.tempoBeatFraction = 0.25;
    config.lineStyle.colorR = 5;

    const nlohmann::json json = config;
    const TimingGridPresetConfig restored = json.get<TimingGridPresetConfig>();

    REQUIRE(restored.mode == GridTimingPresetMode::Tempo);
    REQUIRE(restored.intervalSeconds == 2.0);
    REQUIRE(restored.tempoBeatFraction == 0.25);
    REQUIRE(restored.lineStyle.colorR == 5);
}

TEST_CASE("NamedGridPreset round-trips through JSON", "[core][grid_preset]") {
    NamedGridPreset preset;
    preset.id = 42;
    preset.name = "My Grid";
    preset.frequencyGrid.noteGridEnabled = true;
    preset.timingGrid.mode = GridTimingPresetMode::Interval;
    preset.snapToGridEnabled = true;

    const nlohmann::json json = preset;
    const NamedGridPreset restored = json.get<NamedGridPreset>();

    REQUIRE(restored.id == 42);
    REQUIRE(restored.name == "My Grid");
    REQUIRE(restored.frequencyGrid.noteGridEnabled == true);
    REQUIRE(restored.timingGrid.mode == GridTimingPresetMode::Interval);
    REQUIRE(restored.snapToGridEnabled == true);
}
