#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const ToolConfiguration& config) {
    json = nlohmann::json{{"type", config.type_},
                           {"name", config.name_},
                           {"tipShape", config.tipShape_},
                           {"falloff", config.falloff_},
                           {"size", config.size_},
                           {"stampMode", config.stampMode_},
                           {"stampInterval", config.stampInterval_},
                           {"defaultGradient", config.defaultGradient_}};
}

void from_json(const nlohmann::json& json, ToolConfiguration& config) {
    json.at("type").get_to(config.type_);
    json.at("name").get_to(config.name_);
    json.at("tipShape").get_to(config.tipShape_);
    json.at("falloff").get_to(config.falloff_);
    json.at("size").get_to(config.size_);
    // Absent in a project saved before Stamp Intervals existed - falls
    // back to the same Continuous default/spacing every such project's
    // own strokes already painted with, so an old project's own strokes
    // render identically after loading.
    config.stampMode_ = json.value("stampMode", StampMode::Continuous);
    config.stampInterval_ = json.value("stampInterval", 0.1);
    json.at("defaultGradient").get_to(config.defaultGradient_);
}

}  // namespace sound_mind::core
