#include "sound_mind/core/filter_configuration.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const FilterConfiguration& config) {
    json = nlohmann::json{{"type", config.type_},
                           {"blurSigma", config.blurSigma_},
                           {"medianSize", config.medianSize_},
                           {"directionalBlurLength", config.directionalBlurLength_},
                           {"directionalBlurAngleDegrees", config.directionalBlurAngleDegrees_},
                           {"sharpenAmount", config.sharpenAmount_},
                           {"toneCurvePoints", config.toneCurvePoints_},
                           {"frequencyGradient", config.frequencyGradient_}};
}

void from_json(const nlohmann::json& json, FilterConfiguration& config) {
    json.at("type").get_to(config.type_);
    json.at("blurSigma").get_to(config.blurSigma_);
    json.at("medianSize").get_to(config.medianSize_);
    json.at("directionalBlurLength").get_to(config.directionalBlurLength_);
    json.at("directionalBlurAngleDegrees").get_to(config.directionalBlurAngleDegrees_);
    json.at("sharpenAmount").get_to(config.sharpenAmount_);
    json.at("toneCurvePoints").get_to(config.toneCurvePoints_);
    json.at("frequencyGradient").get_to(config.frequencyGradient_);
}

}  // namespace sound_mind::core
