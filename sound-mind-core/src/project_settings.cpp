#include "sound_mind/core/project_settings.h"

#include <cmath>

namespace sound_mind::core {

void to_json(nlohmann::json& json, const ProjectSettings& settings) {
    json = nlohmann::json{
        {"sampleRateHz", settings.sampleRateHz},
        {"frequencyScale", settings.frequencyScale},
        {"timestepMs", settings.timestepMs},
        {"canvasWidth", settings.canvasWidth},
        {"canvasHeight", settings.canvasHeight},
        {"referenceHz", settings.referenceHz},
        {"defaultTempoBpm", settings.defaultTempoBpm},
        {"binCount", settings.binCount},
        {"minFrequencyHz", settings.minFrequencyHz},
        {"maxFrequencyHz", settings.maxFrequencyHz},
    };
}

void from_json(const nlohmann::json& json, ProjectSettings& settings) {
    json.at("sampleRateHz").get_to(settings.sampleRateHz);
    json.at("frequencyScale").get_to(settings.frequencyScale);
    json.at("timestepMs").get_to(settings.timestepMs);
    json.at("canvasWidth").get_to(settings.canvasWidth);
    json.at("canvasHeight").get_to(settings.canvasHeight);
    json.at("referenceHz").get_to(settings.referenceHz);
    json.at("defaultTempoBpm").get_to(settings.defaultTempoBpm);

    // Lenient (ProjectSettings{}'s own defaults if absent), not .at() -
    // unlike every field above, these three didn't exist before v0.Y.11.1
    // (Create Project Wizard); requiring them here would make it a
    // breaking change to an already-established format (a real Y-bump
    // trigger per docs/sound-mind-roadmap.md's Versioning section) for
    // what's otherwise a purely additive one - a project file saved
    // before this milestone still loads fine, just with the same Stream
    // codec defaults it always implicitly used.
    settings.binCount = json.value("binCount", ProjectSettings{}.binCount);
    settings.minFrequencyHz = json.value("minFrequencyHz", ProjectSettings{}.minFrequencyHz);
    settings.maxFrequencyHz = json.value("maxFrequencyHz", ProjectSettings{}.maxFrequencyHz);
}

sound_mind::codec::StreamCodecConfig streamCodecConfigFor(const ProjectSettings& settings) {
    sound_mind::codec::StreamCodecConfig config;
    config.sampleRateHz = settings.sampleRateHz;
    config.hopLength =
        static_cast<std::uint32_t>(std::lround(settings.timestepMs / 1000.0 * static_cast<double>(settings.sampleRateHz)));
    config.binCount = settings.binCount;
    config.minFrequencyHz = settings.minFrequencyHz;
    config.maxFrequencyHz = settings.maxFrequencyHz;
    return config;
}

}  // namespace sound_mind::core
