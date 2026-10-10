#include "sound_mind/core/grid_preset.h"

#include <stdexcept>

namespace sound_mind::core {

namespace {

std::string dashStyleToString(GridLinePresetStyle::DashStyle style) {
    switch (style) {
        case GridLinePresetStyle::DashStyle::Solid:
            return "Solid";
        case GridLinePresetStyle::DashStyle::Dash:
            return "Dash";
        case GridLinePresetStyle::DashStyle::Dot:
            return "Dot";
    }
    throw std::invalid_argument("Unknown GridLinePresetStyle::DashStyle");
}

GridLinePresetStyle::DashStyle dashStyleFromString(const std::string& text) {
    if (text == "Solid") {
        return GridLinePresetStyle::DashStyle::Solid;
    }
    if (text == "Dash") {
        return GridLinePresetStyle::DashStyle::Dash;
    }
    if (text == "Dot") {
        return GridLinePresetStyle::DashStyle::Dot;
    }
    throw std::invalid_argument("Unknown GridLinePresetStyle::DashStyle: " + text);
}

}  // namespace

void to_json(nlohmann::json& json, const GridLinePresetStyle& style) {
    json = nlohmann::json{{"colorR", style.colorR},
                           {"colorG", style.colorG},
                           {"colorB", style.colorB},
                           {"widthPixels", style.widthPixels},
                           {"dashStyle", dashStyleToString(style.dashStyle)}};
}

void from_json(const nlohmann::json& json, GridLinePresetStyle& style) {
    json.at("colorR").get_to(style.colorR);
    json.at("colorG").get_to(style.colorG);
    json.at("colorB").get_to(style.colorB);
    json.at("widthPixels").get_to(style.widthPixels);
    style.dashStyle = dashStyleFromString(json.at("dashStyle").get<std::string>());
}

void to_json(nlohmann::json& json, const FrequencyGridPresetConfig& config) {
    json = nlohmann::json{
        {"noteGridEnabled", config.noteGridEnabled},
        {"noteGridTemperament", config.noteGridTemperament},
        {"noteGridKey", config.noteGridKey},
        {"noteGridScale", config.noteGridScale},
        {"noteGridExcludedSteps", config.noteGridExcludedSteps},
        {"noteGridExcludedOctaves", config.noteGridExcludedOctaves},
        {"harmonicSeriesEnabled", config.harmonicSeriesEnabled},
        {"harmonicFundamentalHz", config.harmonicFundamentalHz},
        {"customFrequenciesEnabled", config.customFrequenciesEnabled},
        {"customFrequenciesHz", config.customFrequenciesHz},
        {"lineStyle", config.lineStyle},
    };
}

void from_json(const nlohmann::json& json, FrequencyGridPresetConfig& config) {
    json.at("noteGridEnabled").get_to(config.noteGridEnabled);
    json.at("noteGridTemperament").get_to(config.noteGridTemperament);
    json.at("noteGridKey").get_to(config.noteGridKey);
    json.at("noteGridScale").get_to(config.noteGridScale);
    json.at("noteGridExcludedSteps").get_to(config.noteGridExcludedSteps);
    json.at("noteGridExcludedOctaves").get_to(config.noteGridExcludedOctaves);
    json.at("harmonicSeriesEnabled").get_to(config.harmonicSeriesEnabled);
    json.at("harmonicFundamentalHz").get_to(config.harmonicFundamentalHz);
    json.at("customFrequenciesEnabled").get_to(config.customFrequenciesEnabled);
    json.at("customFrequenciesHz").get_to(config.customFrequenciesHz);
    json.at("lineStyle").get_to(config.lineStyle);
}

void to_json(nlohmann::json& json, const TimingGridPresetConfig& config) {
    json = nlohmann::json{
        {"mode", config.mode},
        {"intervalSeconds", config.intervalSeconds},
        {"tempoBeatFraction", config.tempoBeatFraction},
        {"lineStyle", config.lineStyle},
    };
}

void from_json(const nlohmann::json& json, TimingGridPresetConfig& config) {
    json.at("mode").get_to(config.mode);
    json.at("intervalSeconds").get_to(config.intervalSeconds);
    json.at("tempoBeatFraction").get_to(config.tempoBeatFraction);
    json.at("lineStyle").get_to(config.lineStyle);
}

void to_json(nlohmann::json& json, const NamedGridPreset& namedGridPreset) {
    json = nlohmann::json{{"id", namedGridPreset.id},
                           {"name", namedGridPreset.name},
                           {"frequencyGrid", namedGridPreset.frequencyGrid},
                           {"timingGrid", namedGridPreset.timingGrid},
                           {"snapToGridEnabled", namedGridPreset.snapToGridEnabled}};
}

void from_json(const nlohmann::json& json, NamedGridPreset& namedGridPreset) {
    json.at("id").get_to(namedGridPreset.id);
    json.at("name").get_to(namedGridPreset.name);
    json.at("frequencyGrid").get_to(namedGridPreset.frequencyGrid);
    json.at("timingGrid").get_to(namedGridPreset.timingGrid);
    json.at("snapToGridEnabled").get_to(namedGridPreset.snapToGridEnabled);
}

}  // namespace sound_mind::core
