#include "sound_mind/core/filter_preset.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedFilterPreset& namedFilterPreset) {
    json = nlohmann::json{
        {"id", namedFilterPreset.id}, {"name", namedFilterPreset.name}, {"config", namedFilterPreset.config}};
}

void from_json(const nlohmann::json& json, NamedFilterPreset& namedFilterPreset) {
    json.at("id").get_to(namedFilterPreset.id);
    json.at("name").get_to(namedFilterPreset.name);
    json.at("config").get_to(namedFilterPreset.config);
}

}  // namespace sound_mind::core
