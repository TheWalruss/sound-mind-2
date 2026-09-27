#include "sound_mind/core/mind_shot.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedMindShot& namedMindShot) {
    json = nlohmann::json{{"id", namedMindShot.id},
                           {"name", namedMindShot.name},
                           {"clip", namedMindShot.clip},
                           {"fundamentalFrequencyHz", namedMindShot.fundamentalFrequencyHz},
                           {"startTimeOffsetSeconds", namedMindShot.startTimeOffsetSeconds}};
}

void from_json(const nlohmann::json& json, NamedMindShot& namedMindShot) {
    json.at("id").get_to(namedMindShot.id);
    json.at("name").get_to(namedMindShot.name);
    json.at("clip").get_to(namedMindShot.clip);
    // Absent in a project saved before v0.Y.55.1's own prerequisite existed -
    // falls back to "not set", matching both fields' own documented default.
    namedMindShot.fundamentalFrequencyHz = json.value("fundamentalFrequencyHz", 0.0);
    namedMindShot.startTimeOffsetSeconds = json.value("startTimeOffsetSeconds", 0.0);
}

}  // namespace sound_mind::core
