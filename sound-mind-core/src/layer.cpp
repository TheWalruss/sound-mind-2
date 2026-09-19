#include "sound_mind/core/layer.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const Layer& layer) {
    json = nlohmann::json{
        {"id", layer.id_},
        {"name", layer.name_},
        {"type", layer.type_},
        {"opacity", layer.opacity_},
        {"visible", layer.visible_},
        {"translationColumns", layer.translationColumns_},
        {"rescaleFactor", layer.rescaleFactor_},
        {"filterConfiguration", layer.filterConfiguration_},
        {"blendMode", layer.blendMode_},
    };
    // Same "only write if present" convention path.cpp's own
    // handleIn/handleOut already establish for an std::optional field.
    if (layer.opacityMindWave_.has_value()) {
        json["opacityMindWaveId"] = *layer.opacityMindWave_;
    }
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
    // Lenient (defaults to untranslated/unrescaled if absent), same
    // reasoning as visible_ above - didn't exist before v0.Y.21.1 (Layer
    // Time Alignment); a layer saved before this milestone was implicitly
    // untranslated and unrescaled anyway.
    layer.translationColumns_ = json.value("translationColumns", std::int64_t{0});
    layer.rescaleFactor_ = json.value("rescaleFactor", 1.0);
    // Lenient (defaults to a fresh FilterConfiguration if absent), same
    // reasoning as visible_/translationColumns_/rescaleFactor_ above -
    // didn't exist before v0.Y.28.1 (Filter Layers); a layer saved before
    // this milestone was never a Filter-type layer anyway, so its own
    // (meaningless, for that layer) default filter configuration is a
    // harmless fallback.
    layer.filterConfiguration_ = json.value("filterConfiguration", FilterConfiguration{});
    // Lenient (defaults to unbound if absent), same reasoning as the
    // fields above - didn't exist before v0.Y.31.1 Installment C1; a
    // layer saved before this milestone was never MindWave-bound anyway.
    layer.opacityMindWave_ = json.contains("opacityMindWaveId")
                                  ? std::optional(json.at("opacityMindWaveId").get<MindWaveId>())
                                  : std::nullopt;
    // Lenient (defaults to Normal if absent), same reasoning as the fields
    // above - didn't exist before v0.Y.37.1 (Deferred Blend Modes); a layer
    // saved before this milestone was implicitly always Normal anyway.
    layer.blendMode_ = json.value("blendMode", BlendMode::Normal);
}

}  // namespace sound_mind::core
