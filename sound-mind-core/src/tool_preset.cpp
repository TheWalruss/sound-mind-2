#include "sound_mind/core/tool_preset.h"

#include <stdexcept>

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedToolPreset& namedToolPreset) {
    if (!namedToolPreset.config) {
        throw std::invalid_argument("NamedToolPreset: cannot serialize an entry with no config");
    }
    json = nlohmann::json{
        {"id", namedToolPreset.id}, {"name", namedToolPreset.name}, {"config", *namedToolPreset.config}};
}

void from_json(const nlohmann::json& json, NamedToolPreset& namedToolPreset) {
    json.at("id").get_to(namedToolPreset.id);
    json.at("name").get_to(namedToolPreset.name);
    namedToolPreset.config = toolConfigurationFromJson(json.at("config"));
}

}  // namespace sound_mind::core
