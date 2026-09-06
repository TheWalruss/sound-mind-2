#include "sound_mind/core/project_settings.h"

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
}

}  // namespace sound_mind::core
