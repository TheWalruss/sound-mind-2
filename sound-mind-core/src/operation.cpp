#include "sound_mind/core/operation.h"

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

TimeFrequencyRect translated(const TimeFrequencyRect& rect, double deltaTimeSeconds,
                              double deltaFrequencyHz) noexcept {
    TimeFrequencyRect result = rect;
    result.startTimeSeconds += deltaTimeSeconds;
    result.endTimeSeconds += deltaTimeSeconds;
    result.lowFrequencyHz += deltaFrequencyHz;
    result.highFrequencyHz += deltaFrequencyHz;
    return result;
}

}  // namespace sound_mind::core
