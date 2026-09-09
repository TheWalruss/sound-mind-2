#include "sound_mind/core/layer.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const Layer& layer) {
    json = nlohmann::json{
        {"id", layer.id_},
        {"name", layer.name_},
        {"type", layer.type_},
        {"opacity", layer.opacity_},
        {"visible", layer.visible_},
    };
}

void from_json(const nlohmann::json& json, Layer& layer) {
    json.at("id").get_to(layer.id_);
    json.at("name").get_to(layer.name_);
    json.at("type").get_to(layer.type_);
    json.at("opacity").get_to(layer.opacity_);
    // Lenient (defaults to true if absent) - didn't exist before v0.Y.13.1
    // (Layers Panel); requiring it here would make it a breaking change
    // to an already-established format, per the same reasoning
    // ProjectSettings' own new fields used in v0.Y.11.1 - a layer saved
    // before this milestone was implicitly always visible anyway.
    layer.visible_ = json.value("visible", true);
}

}  // namespace sound_mind::core
