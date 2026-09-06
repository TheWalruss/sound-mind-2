#include "sound_mind/core/layer.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const Layer& layer) {
    json = nlohmann::json{
        {"id", layer.id_},
        {"name", layer.name_},
        {"type", layer.type_},
        {"opacity", layer.opacity_},
    };
}

void from_json(const nlohmann::json& json, Layer& layer) {
    json.at("id").get_to(layer.id_);
    json.at("name").get_to(layer.name_);
    json.at("type").get_to(layer.type_);
    json.at("opacity").get_to(layer.opacity_);
}

}  // namespace sound_mind::core
