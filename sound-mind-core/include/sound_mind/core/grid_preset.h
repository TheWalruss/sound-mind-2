#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/music_theory.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedGridPreset` within a Project - see
/// `MindShotId`'s own docs for the same pattern, applied here to a saved
/// Overlay Grid configuration.
using GridPresetId = std::uint64_t;

/**
 * @brief A reference line's drawn appearance, Qt-independent - the
 *        `sound-mind-core` counterpart to `sound_mind::studio`'s own
 *        `FrequencyGridConfig`/`TimingGridConfig` line-drawing fields
 *        (`lineColor`/`lineWidthPixels`/`lineStyle`), which use `QColor`/
 *        `Qt::PenStyle` directly - types this Qt-free library can't
 *        depend on (see `docs/sound-mind-architecture.md`'s Grid Preset
 *        decision). `sound-mind-studio`'s own `gridPresetConversion.h`
 *        converts between the two when saving/loading a preset.
 */
struct GridLinePresetStyle {
    /// @brief Red channel, `[0, 255]`.
    std::uint8_t colorR = 211;
    /// @brief Green channel, `[0, 255]`.
    std::uint8_t colorG = 211;
    /// @brief Blue channel, `[0, 255]`. Defaults match `Qt::lightGray`
    /// (211, 211, 211) - `FrequencyGridConfig`'s/`TimingGridConfig`'s own
    /// default `lineColor`.
    std::uint8_t colorB = 211;
    /// @brief Line width, in pixels.
    double widthPixels = 1.0;

    /// @brief Which of the three dash styles `GridPanel` actually offers
    ///        (see that panel's own `kDashStyles`) - not every
    ///        `Qt::PenStyle` value, just the ones a hand-drawn reference
    ///        line reasonably needs.
    enum class DashStyle {
        Solid,
        Dash,
        Dot,
    };

    /// @brief This style's own dash pattern.
    DashStyle dashStyle = DashStyle::Solid;
};

/// @brief Serializes a line style to its JSON representation.
void to_json(nlohmann::json& json, const GridLinePresetStyle& style);

/// @brief Parses a line style from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, GridLinePresetStyle& style);

/**
 * @brief The saved, Qt-independent counterpart to `sound_mind::studio`'s
 *        own `FrequencyGridConfig` - every field name/meaning matches
 *        that struct's own docs exactly; only the line-style fields
 *        differ in representation (`GridLinePresetStyle` instead of
 *        `QColor`/`double`/`Qt::PenStyle` spread across three separate
 *        fields).
 */
struct FrequencyGridPresetConfig {
    /// @brief See `sound_mind::studio::FrequencyGridConfig::noteGridEnabled`'s own docs.
    bool noteGridEnabled = false;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::noteGridTemperament`'s own docs.
    Temperament noteGridTemperament = Temperament::Equal12;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::noteGridKey`'s own docs.
    PitchClass noteGridKey = PitchClass::C;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::noteGridScale`'s own docs.
    ScaleType noteGridScale = ScaleType::Chromatic;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::noteGridExcludedSteps`'s own docs.
    std::set<int> noteGridExcludedSteps;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::noteGridExcludedOctaves`'s own docs.
    std::set<int> noteGridExcludedOctaves;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::harmonicSeriesEnabled`'s own docs.
    bool harmonicSeriesEnabled = false;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::harmonicFundamentalHz`'s own docs.
    double harmonicFundamentalHz = 110.0;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::customFrequenciesEnabled`'s own docs.
    bool customFrequenciesEnabled = false;
    /// @brief See `sound_mind::studio::FrequencyGridConfig::customFrequenciesHz`'s own docs.
    std::vector<double> customFrequenciesHz;
    /// @brief This grid's line color/width/dash style - see `GridLinePresetStyle`'s own docs.
    GridLinePresetStyle lineStyle;
};

/// @brief Serializes a Frequency Grid preset config to its JSON representation.
void to_json(nlohmann::json& json, const FrequencyGridPresetConfig& config);

/// @brief Parses a Frequency Grid preset config from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, FrequencyGridPresetConfig& config);

/**
 * @brief The saved, Qt-independent counterpart to
 *        `sound_mind::studio::TimingGridMode` - see that enum's own docs.
 */
enum class GridTimingPresetMode {
    Off,
    Interval,
    Tempo,
};

/// @brief Serializes a Timing Grid preset mode to its JSON representation (by name).
NLOHMANN_JSON_SERIALIZE_ENUM(GridTimingPresetMode, {
                                                         {GridTimingPresetMode::Off, "Off"},
                                                         {GridTimingPresetMode::Interval, "Interval"},
                                                         {GridTimingPresetMode::Tempo, "Tempo"},
                                                     })

/**
 * @brief The saved, Qt-independent counterpart to `sound_mind::studio`'s
 *        own `TimingGridConfig` - every field name/meaning matches that
 *        struct's own docs exactly, aside from `mode`'s/`lineStyle`'s own
 *        representation (see `GridTimingPresetMode`'s/
 *        `GridLinePresetStyle`'s own docs).
 */
struct TimingGridPresetConfig {
    /// @brief See `sound_mind::studio::TimingGridConfig::mode`'s own docs.
    GridTimingPresetMode mode = GridTimingPresetMode::Off;
    /// @brief See `sound_mind::studio::TimingGridConfig::intervalSeconds`'s own docs.
    double intervalSeconds = 1.0;
    /// @brief See `sound_mind::studio::TimingGridConfig::tempoBeatFraction`'s own docs.
    double tempoBeatFraction = 1.0;
    /// @brief This grid's line color/width/dash style - see `GridLinePresetStyle`'s own docs.
    GridLinePresetStyle lineStyle;
};

/// @brief Serializes a Timing Grid preset config to its JSON representation.
void to_json(nlohmann::json& json, const TimingGridPresetConfig& config);

/// @brief Parses a Timing Grid preset config from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, TimingGridPresetConfig& config);

/**
 * @brief A named, permanently-stored Overlay Grid configuration in a
 *        Project's own library - the `GridPanel` counterpart to
 *        `NamedToolPreset`, direct user feedback: "give the user the
 *        option to save/load named grid presets, just like tool
 *        configuration presets."
 *
 * Bundles the Frequency Grid, the Timing Grid, and Snap to Grid together
 * as one saved unit - the same three controls `docs/sound-mind-design.md`'s
 * "Overlay Grids" groups under one heading. Axis Labels (`GridPanel`'s own
 * `VerticalAxisLabelMode`/`HorizontalAxisLabelMode`) are deliberately
 * excluded: they're a separate design-doc section ("Axis Labels", not
 * "Overlay Grids") and a pure axis-display choice, not part of "the
 * grid" the design doc or the user's own request describes.
 */
struct NamedGridPreset {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addGridPreset()`, not meant to be picked by hand.
    GridPresetId id = 0;

    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;

    /// @brief The saved Frequency Grid configuration.
    FrequencyGridPresetConfig frequencyGrid;

    /// @brief The saved Timing Grid configuration.
    TimingGridPresetConfig timingGrid;

    /// @brief The saved Snap to Grid checkbox state.
    bool snapToGridEnabled = false;
};

/// @brief Serializes a named grid preset to its JSON representation.
void to_json(nlohmann::json& json, const NamedGridPreset& namedGridPreset);

/// @brief Parses a named grid preset from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedGridPreset& namedGridPreset);

}  // namespace sound_mind::core
