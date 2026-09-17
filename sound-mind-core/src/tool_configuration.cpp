#include "sound_mind/core/tool_configuration.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace sound_mind::core {

namespace {

/// @brief Writes every field `ToolConfiguration`'s own base class defines
/// into `json` - the fields every concrete subtype's own `to_json()`
/// branch needs, identically. Shared here rather than repeated per branch,
/// mirroring `operation_log.cpp`'s own `writeCommonOperationFields()`
/// precedent.
void writeCommonToolConfigurationFields(nlohmann::json& json, const ToolConfiguration& config) {
    json["type"] = config.type();
    json["name"] = config.name();
    json["falloff"] = config.falloff();
    json["size"] = config.size();
    json["stampMode"] = config.stampMode();
    json["stampInterval"] = config.stampInterval();
    json["defaultGradient"] = config.defaultGradient();
}

/// @brief The inverse of writeCommonToolConfigurationFields() - reads
/// `json`'s own base-class fields back into `config`. Shared here rather
/// than repeated per `toolConfigurationFromJson()` branch.
void readCommonToolConfigurationFields(const nlohmann::json& json, ToolConfiguration& config) {
    config.setName(json.at("name").get<std::string>());
    config.setFalloff(json.at("falloff").get<float>());
    config.setSize(json.at("size").get<double>());
    // Absent in a project saved before Stamp Intervals existed - falls
    // back to the same Stroke default/spacing every such project's
    // own strokes already painted with, so an old project's own strokes
    // render identically after loading.
    config.setStampMode(json.value("stampMode", StampMode::Stroke));
    config.setStampInterval(json.value("stampInterval", 0.1));
    config.defaultGradient() = json.at("defaultGradient").get<Gradient>();
}

}  // namespace

void to_json(nlohmann::json& json, const ToolConfiguration& config) {
    // A plain if/else-if dispatch, not a visitor - two concrete subtypes
    // is still few enough that a real dispatch mechanism would be
    // speculative machinery for a problem this doesn't have yet (see
    // `operation_log.cpp`'s own `to_json()` for the same reasoning);
    // revisit if a third subtype makes the chain unwieldy.
    if (const auto* procedural = dynamic_cast<const ProceduralConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        json["tipShape"] = procedural->tipShape();
    } else if (const auto* instrument = dynamic_cast<const InstrumentConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        json["harmonicStrengths"] = instrument->harmonicStrengths();
        json["inharmonicity"] = instrument->inharmonicity();
    } else if (const auto* mindShot = dynamic_cast<const MindShotConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        if (const auto sourceId = mindShot->sourceMindShotId(); sourceId.has_value()) {
            json["sourceMindShotId"] = *sourceId;
        }
        json["clip"] = mindShot->clip();
    } else if (const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        if (const auto sourceId = mindGrain->sourceMindGrainId(); sourceId.has_value()) {
            json["sourceMindGrainId"] = *sourceId;
        }
        json["sourceLayerId"] = mindGrain->sourceLayerId();
        json["bounds"] = mindGrain->bounds();
    } else if (dynamic_cast<const HealConfiguration*>(&config)) {
        // No subtype-specific fields at all - see HealConfiguration's own
        // docs on why.
        writeCommonToolConfigurationFields(json, config);
    } else if (dynamic_cast<const SoftenConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
    } else {
        // Defensive: every concrete subtype is handled above: if this is
        // ever reached, a new subtype was added without updating this
        // dispatch.
        throw std::invalid_argument("ToolConfiguration: unrecognized concrete subtype in to_json()");
    }
}

std::unique_ptr<ToolConfiguration> toolConfigurationFromJson(const nlohmann::json& json) {
    // The "type" field is also the pre-existing ToolType field every saved
    // ToolConfiguration has always carried (defaulting to Procedural
    // before Instrument existed) - reusing it as the polymorphic
    // discriminator keeps a pre-`v0.Y.32.1` project file's own
    // Procedural-only strokes loading unchanged.
    const ToolType type = json.at("type").get<ToolType>();
    std::unique_ptr<ToolConfiguration> config;
    if (type == ToolType::Procedural) {
        auto procedural = std::make_unique<ProceduralConfiguration>();
        procedural->setTipShape(json.at("tipShape").get<BrushTipShape>());
        config = std::move(procedural);
    } else if (type == ToolType::Instrument) {
        auto instrument = std::make_unique<InstrumentConfiguration>();
        instrument->setHarmonicStrengths(json.at("harmonicStrengths").get<std::vector<double>>());
        instrument->setInharmonicity(json.at("inharmonicity").get<double>());
        config = std::move(instrument);
    } else if (type == ToolType::MindShot) {
        auto mindShot = std::make_unique<MindShotConfiguration>();
        // "sourceMindShotId" is UI-only metadata (see its own docs) -
        // absent for a configuration whose clip was never set from a
        // library entry, so its own absence isn't a format error.
        const std::optional<MindShotId> sourceId =
            json.contains("sourceMindShotId") ? std::optional(json.at("sourceMindShotId").get<MindShotId>())
                                                : std::nullopt;
        mindShot->setClip(sourceId, json.at("clip").get<Clip>());
        config = std::move(mindShot);
    } else if (type == ToolType::MindGrain) {
        auto mindGrain = std::make_unique<MindGrainConfiguration>();
        // "sourceMindGrainId" is UI-only metadata, same as MindShot's own.
        const std::optional<MindGrainId> sourceId =
            json.contains("sourceMindGrainId") ? std::optional(json.at("sourceMindGrainId").get<MindGrainId>())
                                                 : std::nullopt;
        mindGrain->setReference(sourceId, json.at("sourceLayerId").get<LayerId>(),
                                 json.at("bounds").get<TimeFrequencyRect>());
        config = std::move(mindGrain);
    } else if (type == ToolType::Heal) {
        config = std::make_unique<HealConfiguration>();
    } else if (type == ToolType::Soften) {
        config = std::make_unique<SoftenConfiguration>();
    } else {
        throw std::invalid_argument("ToolConfiguration: unrecognized \"type\" in toolConfigurationFromJson()");
    }
    readCommonToolConfigurationFields(json, *config);
    return config;
}

}  // namespace sound_mind::core
