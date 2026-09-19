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
/// @brief Writes `mindWaveId` under `key`, only if present - the same
/// "only write if present" convention `filter_configuration.cpp`'s own
/// identical helper already establishes, duplicated here rather than
/// shared across modules for the same reason `MindWaveId` itself is
/// duplicated (see this header's own docs on that alias).
void writeOptionalMindWaveId(nlohmann::json& json, const char* key, std::optional<MindWaveId> mindWaveId) {
    if (mindWaveId.has_value()) {
        json[key] = *mindWaveId;
    }
}

/// @brief The inverse of writeOptionalMindWaveId() - `std::nullopt` if
/// `key` is absent (a configuration saved before `v0.Y.39.1` was never
/// bound anyway).
std::optional<MindWaveId> readOptionalMindWaveId(const nlohmann::json& json, const char* key) {
    return json.contains(key) ? std::optional(json.at(key).get<MindWaveId>()) : std::nullopt;
}

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
        writeOptionalMindWaveId(json, "vibratoMindWaveId", instrument->vibratoMindWave());
        json["vibratoDepthSemitones"] = instrument->vibratoDepthSemitones();
        writeOptionalMindWaveId(json, "tremoloMindWaveId", instrument->tremoloMindWave());
        json["tremoloDepth"] = instrument->tremoloDepth();
    } else if (const auto* mindShot = dynamic_cast<const MindShotConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        if (const auto sourceId = mindShot->sourceMindShotId(); sourceId.has_value()) {
            json["sourceMindShotId"] = *sourceId;
        }
        json["clip"] = mindShot->clip();
        json["blendMode"] = mindShot->blendMode();
    } else if (const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        if (const auto sourceId = mindGrain->sourceMindGrainId(); sourceId.has_value()) {
            json["sourceMindGrainId"] = *sourceId;
        }
        json["sourceLayerId"] = mindGrain->sourceLayerId();
        json["bounds"] = mindGrain->bounds();
        json["blendMode"] = mindGrain->blendMode();
    } else if (dynamic_cast<const HealConfiguration*>(&config)) {
        // No subtype-specific fields at all - see HealConfiguration's own
        // docs on why.
        writeCommonToolConfigurationFields(json, config);
    } else if (dynamic_cast<const SoftenConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
    } else if (dynamic_cast<const SmudgeConfiguration*>(&config)) {
        // Same reasoning as Heal/Soften - see SmudgeConfiguration's own docs.
        writeCommonToolConfigurationFields(json, config);
    } else if (const auto* orderChaos = dynamic_cast<const OrderChaosConfiguration*>(&config)) {
        writeCommonToolConfigurationFields(json, config);
        json["amount"] = orderChaos->amount();
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
        // Vibrato/tremolo were added in v0.Y.39.1, after InstrumentConfiguration
        // had already shipped and been saved in real project files - loaded
        // leniently (json.value(...)/readOptionalMindWaveId()) so older saved
        // projects with none of these fields still load cleanly, unbound.
        instrument->setVibratoMindWave(readOptionalMindWaveId(json, "vibratoMindWaveId"));
        instrument->setVibratoDepthSemitones(json.value("vibratoDepthSemitones", 0.5));
        instrument->setTremoloMindWave(readOptionalMindWaveId(json, "tremoloMindWaveId"));
        instrument->setTremoloDepth(json.value("tremoloDepth", 0.3));
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
        // Lenient (defaults to Overwrite if absent) - didn't exist before
        // v0.Y.37.1 (Deferred Blend Modes); a configuration saved before
        // this milestone was implicitly always a hard overwrite anyway.
        mindShot->setBlendMode(json.value("blendMode", BlendMode::Overwrite));
        config = std::move(mindShot);
    } else if (type == ToolType::MindGrain) {
        auto mindGrain = std::make_unique<MindGrainConfiguration>();
        // "sourceMindGrainId" is UI-only metadata, same as MindShot's own.
        const std::optional<MindGrainId> sourceId =
            json.contains("sourceMindGrainId") ? std::optional(json.at("sourceMindGrainId").get<MindGrainId>())
                                                 : std::nullopt;
        mindGrain->setReference(sourceId, json.at("sourceLayerId").get<LayerId>(),
                                 json.at("bounds").get<TimeFrequencyRect>());
        // Lenient, same reasoning as MindShotConfiguration's own above.
        mindGrain->setBlendMode(json.value("blendMode", BlendMode::Overwrite));
        config = std::move(mindGrain);
    } else if (type == ToolType::Heal) {
        config = std::make_unique<HealConfiguration>();
    } else if (type == ToolType::Soften) {
        config = std::make_unique<SoftenConfiguration>();
    } else if (type == ToolType::Smudge) {
        config = std::make_unique<SmudgeConfiguration>();
    } else if (type == ToolType::OrderChaos) {
        auto orderChaos = std::make_unique<OrderChaosConfiguration>();
        orderChaos->setAmount(json.at("amount").get<double>());
        config = std::move(orderChaos);
    } else {
        throw std::invalid_argument("ToolConfiguration: unrecognized \"type\" in toolConfigurationFromJson()");
    }
    readCommonToolConfigurationFields(json, *config);
    return config;
}

}  // namespace sound_mind::core
