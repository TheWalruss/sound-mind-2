#include "sound_mind/core/operation.h"

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

// Defined out-of-line (rather than `= default` in the header) so the
// vtable and RTTI for this polymorphic base aren't re-emitted in every
// translation unit that includes operation.h.
Operation::~Operation() = default;

void to_json(nlohmann::json& json, const TimeFrequencyRect& rect) {
    json = nlohmann::json{
        {"startTimeSeconds", rect.startTimeSeconds},
        {"endTimeSeconds", rect.endTimeSeconds},
        {"lowFrequencyHz", rect.lowFrequencyHz},
        {"highFrequencyHz", rect.highFrequencyHz},
    };
}

void from_json(const nlohmann::json& json, TimeFrequencyRect& rect) {
    json.at("startTimeSeconds").get_to(rect.startTimeSeconds);
    json.at("endTimeSeconds").get_to(rect.endTimeSeconds);
    json.at("lowFrequencyHz").get_to(rect.lowFrequencyHz);
    json.at("highFrequencyHz").get_to(rect.highFrequencyHz);
}

TimeFrequencyRect translated(const TimeFrequencyRect& rect, double deltaTimeSeconds, double deltaFrequencyBins,
                              const sound_mind::codec::StreamCodecConfig& config) noexcept {
    // Each bound's own frequency shifts via its own bin position - see
    // translateFrequencyByBins()'s own docs for why that, not a shared Hz
    // offset, is what keeps the rect's own screen-space shape intact.
    TimeFrequencyRect result = rect;
    result.startTimeSeconds += deltaTimeSeconds;
    result.endTimeSeconds += deltaTimeSeconds;
    result.lowFrequencyHz = translateFrequencyByBins(static_cast<float>(rect.lowFrequencyHz), deltaFrequencyBins, config);
    result.highFrequencyHz =
        translateFrequencyByBins(static_cast<float>(rect.highFrequencyHz), deltaFrequencyBins, config);
    return result;
}

}  // namespace sound_mind::core
