#include "sound_mind/core/mind_shot.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedMindShot& namedMindShot) {
    json = nlohmann::json{
        {"id", namedMindShot.id}, {"name", namedMindShot.name}, {"clip", namedMindShot.clip}};
}

void from_json(const nlohmann::json& json, NamedMindShot& namedMindShot) {
    json.at("id").get_to(namedMindShot.id);
    json.at("name").get_to(namedMindShot.name);
    json.at("clip").get_to(namedMindShot.clip);
}

}  // namespace sound_mind::core
