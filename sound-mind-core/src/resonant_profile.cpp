#include "sound_mind/core/resonant_profile.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedResonantProfile& namedProfile) {
    json = nlohmann::json{{"id", namedProfile.id}, {"name", namedProfile.name}, {"spectrum", namedProfile.spectrum}};
}

void from_json(const nlohmann::json& json, NamedResonantProfile& namedProfile) {
    json.at("id").get_to(namedProfile.id);
    json.at("name").get_to(namedProfile.name);
    json.at("spectrum").get_to(namedProfile.spectrum);
}

}  // namespace sound_mind::core
