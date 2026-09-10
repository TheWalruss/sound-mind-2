#include "sound_mind/core/paste_operation.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const Clip& clip) {
    json = nlohmann::json{
        {"frameCount", clip.frameCount},
        {"binCount", clip.binCount},
        {"leftMagnitudeDb", clip.leftMagnitudeDb},
        {"rightMagnitudeDb", clip.rightMagnitudeDb},
        {"sharedPhaseRadians", clip.sharedPhaseRadians},
    };
}

void from_json(const nlohmann::json& json, Clip& clip) {
    json.at("frameCount").get_to(clip.frameCount);
    json.at("binCount").get_to(clip.binCount);
    json.at("leftMagnitudeDb").get_to(clip.leftMagnitudeDb);
    json.at("rightMagnitudeDb").get_to(clip.rightMagnitudeDb);
    json.at("sharedPhaseRadians").get_to(clip.sharedPhaseRadians);
}

}  // namespace sound_mind::core
