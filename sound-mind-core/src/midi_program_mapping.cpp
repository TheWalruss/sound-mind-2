#include "sound_mind/core/midi_program_mapping.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const MidiProgramMapping& mapping) {
    json = nlohmann::json{
        {"programNumber", mapping.programNumber},
        {"durationScale", mapping.durationScale},
        {"pitchOffsetSemitones", mapping.pitchOffsetSemitones},
    };
    if (mapping.toolPresetId.has_value()) {
        json["toolPresetId"] = *mapping.toolPresetId;
    }
}

void from_json(const nlohmann::json& json, MidiProgramMapping& mapping) {
    json.at("programNumber").get_to(mapping.programNumber);
    mapping.durationScale = json.value("durationScale", 1.0);
    mapping.pitchOffsetSemitones = json.value("pitchOffsetSemitones", 0.0);
    mapping.toolPresetId =
        json.contains("toolPresetId") ? std::optional(json.at("toolPresetId").get<ToolPresetId>()) : std::nullopt;
}

}  // namespace sound_mind::core
