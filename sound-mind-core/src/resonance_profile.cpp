#include "sound_mind/core/resonance_profile.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedResonanceProfile& namedProfile) {
    json = nlohmann::json{{"id", namedProfile.id},
                           {"name", namedProfile.name},
                           {"spectrum", namedProfile.spectrum},
                           {"sourceCurve", namedProfile.sourceCurve}};
}

void from_json(const nlohmann::json& json, NamedResonanceProfile& namedProfile) {
    json.at("id").get_to(namedProfile.id);
    json.at("name").get_to(namedProfile.name);
    json.at("spectrum").get_to(namedProfile.spectrum);
    // Lenient (defaults to an empty graph if absent) - didn't exist before
    // this field was added; an entry saved before it had no source curve
    // to lose.
    namedProfile.sourceCurve = CurveGraph{};
    if (json.contains("sourceCurve")) {
        json.at("sourceCurve").get_to(namedProfile.sourceCurve);
    }
}

}  // namespace sound_mind::core
