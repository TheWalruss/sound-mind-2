#include "sound_mind/core/resonance_profile.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedResonanceProfile& namedProfile) {
    json = nlohmann::json{{"id", namedProfile.id}, {"name", namedProfile.name}, {"spectrum", namedProfile.spectrum}};
}

void from_json(const nlohmann::json& json, NamedResonanceProfile& namedProfile) {
    json.at("id").get_to(namedProfile.id);
    json.at("name").get_to(namedProfile.name);
    json.at("spectrum").get_to(namedProfile.spectrum);
}

}  // namespace sound_mind::core
