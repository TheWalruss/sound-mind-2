#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::Clip;
using sound_mind::core::HealConfiguration;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::MindShotConfiguration;
using sound_mind::core::OrderChaosConfiguration;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::SmudgeConfiguration;
using sound_mind::core::SoftenConfiguration;
using sound_mind::core::StampMode;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::toolConfigurationFromJson;
using sound_mind::core::ToolType;

namespace {

Clip makeTestClip() {
    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.sharedPhaseRadians = {0.0f, 0.0f, 0.0f, 0.0f};
    return clip;
}

}  // namespace

TEST_CASE("A fresh ProceduralConfiguration is Procedural with a circular tip", "[core][tool_configuration]") {
    const ProceduralConfiguration config;
    REQUIRE(config.type() == ToolType::Procedural);
    REQUIRE(config.tipShape() == BrushTipShape::Circle);
}

TEST_CASE("A fresh ToolConfiguration has an empty (unsaved) name", "[core][tool_configuration]") {
    const ProceduralConfiguration config;
    REQUIRE(config.name().empty());
}

TEST_CASE("A fresh ToolConfiguration has a fresh, fully transparent default gradient", "[core][tool_configuration]") {
    const ProceduralConfiguration config;
    REQUIRE(config.defaultGradient().stops().size() == 2);
    REQUIRE(config.defaultGradient().stops().front().leftOpacity == 0.0f);
}

TEST_CASE("A ToolConfiguration can be named", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setName("My Brush");
    REQUIRE(config.name() == "My Brush");
}

TEST_CASE("A ProceduralConfiguration's tip shape can be changed", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setTipShape(BrushTipShape::Star);
    REQUIRE(config.tipShape() == BrushTipShape::Star);
}

TEST_CASE("A ToolConfiguration's falloff can be changed", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setFalloff(0.9f);
    REQUIRE(config.falloff() == 0.9f);
}

TEST_CASE("A ToolConfiguration's size can be changed", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setSize(2.5);
    REQUIRE(config.size() == 2.5);
}

TEST_CASE("A ToolConfiguration's default gradient can be mutated in place", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.defaultGradient().setLinkChannels(true);
    REQUIRE(config.defaultGradient().linkChannels());
}

TEST_CASE("A fresh ToolConfiguration's stamp mode is Stroke", "[core][tool_configuration]") {
    const ProceduralConfiguration config;
    REQUIRE(config.stampMode() == StampMode::Stroke);
}

TEST_CASE("A ToolConfiguration's stamp mode can be changed", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setStampMode(StampMode::AlongCurve);
    REQUIRE(config.stampMode() == StampMode::AlongCurve);
}

TEST_CASE("A ToolConfiguration's stamp interval can be changed", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setStampInterval(0.5);
    REQUIRE(config.stampInterval() == 0.5);
}

TEST_CASE("A ProceduralConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setName("My Brush");
    config.setTipShape(BrushTipShape::Star);
    config.setFalloff(0.75f);

    const std::unique_ptr<ToolConfiguration> clone = config.clone();
    REQUIRE(clone->type() == ToolType::Procedural);
    REQUIRE(clone->name() == "My Brush");
    REQUIRE(dynamic_cast<const ProceduralConfiguration&>(*clone).tipShape() == BrushTipShape::Star);
    REQUIRE(clone->falloff() == 0.75f);

    // Independent - mutating the original doesn't affect the clone.
    config.setName("Renamed");
    REQUIRE(clone->name() == "My Brush");
}

TEST_CASE("A ProceduralConfiguration round-trips through JSON", "[core][tool_configuration]") {
    ProceduralConfiguration config;
    config.setName("My Brush");
    config.setTipShape(BrushTipShape::Diamond);
    config.setFalloff(0.25f);
    config.setSize(3.0);
    config.setStampMode(StampMode::FrequencyAxis);
    config.setStampInterval(150.0);
    config.defaultGradient().setLinkChannels(true);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::Procedural);
    REQUIRE(roundTripped->name() == "My Brush");
    REQUIRE(dynamic_cast<const ProceduralConfiguration&>(*roundTripped).tipShape() == BrushTipShape::Diamond);
    REQUIRE(roundTripped->falloff() == 0.25f);
    REQUIRE(roundTripped->size() == 3.0);
    REQUIRE(roundTripped->stampMode() == StampMode::FrequencyAxis);
    REQUIRE(roundTripped->stampInterval() == 150.0);
    REQUIRE(roundTripped->defaultGradient().linkChannels());
}

TEST_CASE("A ToolConfiguration loaded from JSON with no stampMode/stampInterval keys falls back to Stroke",
          "[core][tool_configuration]") {
    // A project saved before Stamp Intervals existed - its own strokes
    // must render identically after loading, not silently gain a new
    // (and, for their own already-committed geometry, meaningless)
    // stamp spacing.
    nlohmann::json json = ProceduralConfiguration{};
    json.erase("stampMode");
    json.erase("stampInterval");

    const std::unique_ptr<ToolConfiguration> loaded = toolConfigurationFromJson(json);

    REQUIRE(loaded->stampMode() == StampMode::Stroke);
    REQUIRE(loaded->stampInterval() == 0.1);
}

TEST_CASE("A ToolConfiguration loaded from JSON with the pre-rename \"continuous\" stampMode string still loads as "
          "Stroke",
          "[core][tool_configuration]") {
    // A project saved before the Continuous -> Stroke rename (v0.0.32.3) -
    // its own strokes must render identically after loading, not silently
    // fall back to some other mode because the old string is unrecognized.
    nlohmann::json json = ProceduralConfiguration{};
    json["stampMode"] = "continuous";

    const std::unique_ptr<ToolConfiguration> loaded = toolConfigurationFromJson(json);

    REQUIRE(loaded->stampMode() == StampMode::Stroke);
}

TEST_CASE("A ToolConfiguration round-trips StampMode::Stroke through JSON as \"stroke\", not \"continuous\"",
          "[core][tool_configuration]") {
    // The legacy "continuous" string is a read-only alias (see
    // ToolConfiguration.h's own NLOHMANN_JSON_SERIALIZE_ENUM comment) -
    // to_json() must always write the new "stroke" spelling going forward.
    ProceduralConfiguration config;
    config.setStampMode(StampMode::Stroke);

    const nlohmann::json json = config;

    REQUIRE(json.at("stampMode").get<std::string>() == "stroke");
}

TEST_CASE("A fresh InstrumentConfiguration is Instrument with a plausible default harmonic series",
          "[core][tool_configuration]") {
    const InstrumentConfiguration config;
    REQUIRE(config.type() == ToolType::Instrument);
    REQUIRE(config.harmonicStrengths() == std::vector<double>{1.0, 0.5, 0.25, 0.125});
    REQUIRE(config.inharmonicity() == 0.0);
}

TEST_CASE("An InstrumentConfiguration's harmonic strengths can be changed", "[core][tool_configuration]") {
    InstrumentConfiguration config;
    config.setHarmonicStrengths({1.0, 0.8, 0.6});
    REQUIRE(config.harmonicStrengths() == std::vector<double>{1.0, 0.8, 0.6});
}

TEST_CASE("An InstrumentConfiguration's inharmonicity can be changed", "[core][tool_configuration]") {
    InstrumentConfiguration config;
    config.setInharmonicity(0.02);
    REQUIRE(config.inharmonicity() == 0.02);
}

TEST_CASE("An InstrumentConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    InstrumentConfiguration config;
    config.setName("Bell");
    config.setHarmonicStrengths({1.0, 0.9, 0.3});
    config.setInharmonicity(0.015);

    const std::unique_ptr<ToolConfiguration> clone = config.clone();
    REQUIRE(clone->type() == ToolType::Instrument);
    REQUIRE(clone->name() == "Bell");
    const auto& clonedInstrument = dynamic_cast<const InstrumentConfiguration&>(*clone);
    REQUIRE(clonedInstrument.harmonicStrengths() == std::vector<double>{1.0, 0.9, 0.3});
    REQUIRE(clonedInstrument.inharmonicity() == 0.015);

    config.setInharmonicity(0.5);
    REQUIRE(clonedInstrument.inharmonicity() == 0.015);
}

TEST_CASE("An InstrumentConfiguration round-trips through JSON", "[core][tool_configuration]") {
    InstrumentConfiguration config;
    config.setName("Bell");
    config.setHarmonicStrengths({1.0, 0.9, 0.3, 0.1});
    config.setInharmonicity(0.015);
    config.setFalloff(0.4f);
    config.setSize(0.75);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::Instrument);
    REQUIRE(roundTripped->name() == "Bell");
    const auto& instrument = dynamic_cast<const InstrumentConfiguration&>(*roundTripped);
    REQUIRE(instrument.harmonicStrengths() == std::vector<double>{1.0, 0.9, 0.3, 0.1});
    REQUIRE(instrument.inharmonicity() == 0.015);
    REQUIRE(roundTripped->falloff() == 0.4f);
    REQUIRE(roundTripped->size() == 0.75);
}

TEST_CASE("A fresh MindShotConfiguration is MindShot with no Mind Shot selected", "[core][tool_configuration]") {
    const MindShotConfiguration config;
    REQUIRE(config.type() == ToolType::MindShot);
    REQUIRE(config.sourceMindShotId() == std::nullopt);
    REQUIRE(config.clip().frameCount == 0);
    REQUIRE(config.clip().binCount == 0);
}

TEST_CASE("MindShotConfiguration::setClip() sets the source id and the clip", "[core][tool_configuration]") {
    MindShotConfiguration config;
    config.setClip(sound_mind::core::MindShotId{7}, makeTestClip());

    REQUIRE(config.sourceMindShotId() == sound_mind::core::MindShotId{7});
    REQUIRE(config.clip().frameCount == 2);
    REQUIRE(config.clip().binCount == 2);
    REQUIRE(config.clip().leftMagnitudeDb == std::vector<float>{-1.0f, -2.0f, -3.0f, -4.0f});
}

TEST_CASE("A MindShotConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    MindShotConfiguration config;
    config.setName("Piano Hit Brush");
    config.setClip(sound_mind::core::MindShotId{3}, makeTestClip());

    const std::unique_ptr<ToolConfiguration> clone = config.clone();
    REQUIRE(clone->type() == ToolType::MindShot);
    REQUIRE(clone->name() == "Piano Hit Brush");
    const auto& clonedMindShot = dynamic_cast<const MindShotConfiguration&>(*clone);
    REQUIRE(clonedMindShot.sourceMindShotId() == sound_mind::core::MindShotId{3});
    REQUIRE(clonedMindShot.clip().frameCount == 2);

    config.setClip(sound_mind::core::MindShotId{99}, Clip{});
    REQUIRE(clonedMindShot.sourceMindShotId() == sound_mind::core::MindShotId{3});
    REQUIRE(clonedMindShot.clip().frameCount == 2);
}

TEST_CASE("A MindShotConfiguration round-trips through JSON, source id included", "[core][tool_configuration]") {
    MindShotConfiguration config;
    config.setName("Piano Hit Brush");
    config.setClip(sound_mind::core::MindShotId{3}, makeTestClip());
    config.setFalloff(0.6f);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::MindShot);
    REQUIRE(roundTripped->name() == "Piano Hit Brush");
    const auto& mindShot = dynamic_cast<const MindShotConfiguration&>(*roundTripped);
    REQUIRE(mindShot.sourceMindShotId() == sound_mind::core::MindShotId{3});
    REQUIRE(mindShot.clip().frameCount == 2);
    REQUIRE(mindShot.clip().binCount == 2);
    REQUIRE(mindShot.clip().leftMagnitudeDb == std::vector<float>{-1.0f, -2.0f, -3.0f, -4.0f});
    REQUIRE(roundTripped->falloff() == 0.6f);
}

TEST_CASE("A MindShotConfiguration round-trips through JSON with no source id", "[core][tool_configuration]") {
    // A configuration whose clip was never set from a library entry -
    // sourceMindShotId() must round-trip as nullopt, not some default id.
    MindShotConfiguration config;
    nlohmann::json json = config;
    json.erase("sourceMindShotId");  // not written in the first place, but confirm the read side too.

    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    const auto& mindShot = dynamic_cast<const MindShotConfiguration&>(*roundTripped);
    REQUIRE(mindShot.sourceMindShotId() == std::nullopt);
}

TEST_CASE("A fresh MindGrainConfiguration is MindGrain with no Mind Grain selected", "[core][tool_configuration]") {
    const MindGrainConfiguration config;
    REQUIRE(config.type() == ToolType::MindGrain);
    REQUIRE(config.sourceMindGrainId() == std::nullopt);
    REQUIRE(config.sourceLayerId() == 0);
}

TEST_CASE("MindGrainConfiguration::setReference() sets the source id, layer, and bounds",
          "[core][tool_configuration]") {
    MindGrainConfiguration config;
    const sound_mind::core::TimeFrequencyRect bounds{0.5, 1.5, 200.0, 800.0};
    config.setReference(sound_mind::core::MindGrainId{7}, sound_mind::core::LayerId{3}, bounds);

    REQUIRE(config.sourceMindGrainId() == sound_mind::core::MindGrainId{7});
    REQUIRE(config.sourceLayerId() == sound_mind::core::LayerId{3});
    REQUIRE(config.bounds().startTimeSeconds == 0.5);
    REQUIRE(config.bounds().endTimeSeconds == 1.5);
    REQUIRE(config.bounds().lowFrequencyHz == 200.0);
    REQUIRE(config.bounds().highFrequencyHz == 800.0);
}

TEST_CASE("A MindGrainConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    MindGrainConfiguration config;
    config.setName("Rain Texture Brush");
    config.setReference(sound_mind::core::MindGrainId{3}, sound_mind::core::LayerId{4},
                         sound_mind::core::TimeFrequencyRect{0.0, 1.0, 100.0, 200.0});

    const std::unique_ptr<ToolConfiguration> clone = config.clone();
    REQUIRE(clone->type() == ToolType::MindGrain);
    REQUIRE(clone->name() == "Rain Texture Brush");
    const auto& clonedMindGrain = dynamic_cast<const MindGrainConfiguration&>(*clone);
    REQUIRE(clonedMindGrain.sourceMindGrainId() == sound_mind::core::MindGrainId{3});
    REQUIRE(clonedMindGrain.sourceLayerId() == sound_mind::core::LayerId{4});

    config.setReference(sound_mind::core::MindGrainId{99}, sound_mind::core::LayerId{50},
                         sound_mind::core::TimeFrequencyRect{});
    REQUIRE(clonedMindGrain.sourceMindGrainId() == sound_mind::core::MindGrainId{3});
    REQUIRE(clonedMindGrain.sourceLayerId() == sound_mind::core::LayerId{4});
}

TEST_CASE("A MindGrainConfiguration round-trips through JSON, source id included", "[core][tool_configuration]") {
    MindGrainConfiguration config;
    config.setName("Rain Texture Brush");
    config.setReference(sound_mind::core::MindGrainId{3}, sound_mind::core::LayerId{4},
                         sound_mind::core::TimeFrequencyRect{0.5, 1.5, 200.0, 800.0});
    config.setFalloff(0.6f);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::MindGrain);
    REQUIRE(roundTripped->name() == "Rain Texture Brush");
    const auto& mindGrain = dynamic_cast<const MindGrainConfiguration&>(*roundTripped);
    REQUIRE(mindGrain.sourceMindGrainId() == sound_mind::core::MindGrainId{3});
    REQUIRE(mindGrain.sourceLayerId() == sound_mind::core::LayerId{4});
    REQUIRE(mindGrain.bounds().startTimeSeconds == 0.5);
    REQUIRE(mindGrain.bounds().highFrequencyHz == 800.0);
    REQUIRE(roundTripped->falloff() == 0.6f);
}

TEST_CASE("A MindGrainConfiguration round-trips through JSON with no source id", "[core][tool_configuration]") {
    // A configuration whose reference was never set from a library entry -
    // sourceMindGrainId() must round-trip as nullopt, not some default id.
    MindGrainConfiguration config;
    config.setReference(std::nullopt, sound_mind::core::LayerId{4}, sound_mind::core::TimeFrequencyRect{});
    nlohmann::json json = config;
    json.erase("sourceMindGrainId");  // not written in the first place, but confirm the read side too.

    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    const auto& mindGrain = dynamic_cast<const MindGrainConfiguration&>(*roundTripped);
    REQUIRE(mindGrain.sourceMindGrainId() == std::nullopt);
    REQUIRE(mindGrain.sourceLayerId() == sound_mind::core::LayerId{4});
}

TEST_CASE("A fresh HealConfiguration is Heal", "[core][tool_configuration]") {
    const HealConfiguration config;
    REQUIRE(config.type() == ToolType::Heal);
}

TEST_CASE("A HealConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    HealConfiguration config;
    config.setName("Defect Eraser");
    config.setFalloff(0.4f);
    config.setSize(0.1);

    const std::unique_ptr<ToolConfiguration> clone = config.clone();

    REQUIRE(clone->type() == ToolType::Heal);
    REQUIRE(clone->name() == "Defect Eraser");
    REQUIRE(clone->falloff() == 0.4f);
    REQUIRE(clone->size() == 0.1);

    config.setName("Renamed");
    REQUIRE(clone->name() == "Defect Eraser");
}

TEST_CASE("A HealConfiguration round-trips through JSON", "[core][tool_configuration]") {
    HealConfiguration config;
    config.setName("Defect Eraser");
    config.setFalloff(0.4f);
    config.setSize(0.1);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::Heal);
    REQUIRE(roundTripped->name() == "Defect Eraser");
    REQUIRE(roundTripped->falloff() == 0.4f);
    REQUIRE(roundTripped->size() == 0.1);
}

TEST_CASE("A fresh SoftenConfiguration is Soften", "[core][tool_configuration]") {
    const SoftenConfiguration config;
    REQUIRE(config.type() == ToolType::Soften);
}

TEST_CASE("A SoftenConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    SoftenConfiguration config;
    config.setName("Smooth Pass");
    config.setFalloff(0.6f);
    config.setSize(0.2);

    const std::unique_ptr<ToolConfiguration> clone = config.clone();

    REQUIRE(clone->type() == ToolType::Soften);
    REQUIRE(clone->name() == "Smooth Pass");
    REQUIRE(clone->falloff() == 0.6f);
    REQUIRE(clone->size() == 0.2);

    config.setName("Renamed");
    REQUIRE(clone->name() == "Smooth Pass");
}

TEST_CASE("A SoftenConfiguration round-trips through JSON", "[core][tool_configuration]") {
    SoftenConfiguration config;
    config.setName("Smooth Pass");
    config.setFalloff(0.6f);
    config.setSize(0.2);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::Soften);
    REQUIRE(roundTripped->name() == "Smooth Pass");
    REQUIRE(roundTripped->falloff() == 0.6f);
    REQUIRE(roundTripped->size() == 0.2);
}

TEST_CASE("A fresh SmudgeConfiguration is Smudge", "[core][tool_configuration]") {
    const SmudgeConfiguration config;
    REQUIRE(config.type() == ToolType::Smudge);
}

TEST_CASE("A SmudgeConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    SmudgeConfiguration config;
    config.setName("Drag Along");
    config.setFalloff(0.5f);
    config.setSize(0.15);

    const std::unique_ptr<ToolConfiguration> clone = config.clone();

    REQUIRE(clone->type() == ToolType::Smudge);
    REQUIRE(clone->name() == "Drag Along");
    REQUIRE(clone->falloff() == 0.5f);
    REQUIRE(clone->size() == 0.15);

    config.setName("Renamed");
    REQUIRE(clone->name() == "Drag Along");
}

TEST_CASE("A SmudgeConfiguration round-trips through JSON", "[core][tool_configuration]") {
    SmudgeConfiguration config;
    config.setName("Drag Along");
    config.setFalloff(0.5f);
    config.setSize(0.15);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::Smudge);
    REQUIRE(roundTripped->name() == "Drag Along");
    REQUIRE(roundTripped->falloff() == 0.5f);
    REQUIRE(roundTripped->size() == 0.15);
}

TEST_CASE("A fresh OrderChaosConfiguration is OrderChaos with amount() at 0", "[core][tool_configuration]") {
    const OrderChaosConfiguration config;
    REQUIRE(config.type() == ToolType::OrderChaos);
    REQUIRE(config.amount() == 0.0);
}

TEST_CASE("OrderChaosConfiguration::setAmount() sets amount()", "[core][tool_configuration]") {
    OrderChaosConfiguration config;
    config.setAmount(-0.75);
    REQUIRE(config.amount() == -0.75);
    config.setAmount(0.4);
    REQUIRE(config.amount() == 0.4);
}

TEST_CASE("An OrderChaosConfiguration's clone() is an independent, equal copy", "[core][tool_configuration]") {
    OrderChaosConfiguration config;
    config.setName("Toward Order");
    config.setAmount(0.6);

    const std::unique_ptr<ToolConfiguration> clone = config.clone();

    REQUIRE(clone->type() == ToolType::OrderChaos);
    REQUIRE(clone->name() == "Toward Order");
    const auto& clonedOrderChaos = dynamic_cast<const OrderChaosConfiguration&>(*clone);
    REQUIRE(clonedOrderChaos.amount() == 0.6);

    config.setAmount(-0.9);
    REQUIRE(clonedOrderChaos.amount() == 0.6);
}

TEST_CASE("An OrderChaosConfiguration round-trips through JSON", "[core][tool_configuration]") {
    OrderChaosConfiguration config;
    config.setName("Toward Chaos");
    config.setAmount(-0.3);
    config.setFalloff(0.2f);

    const nlohmann::json json = config;
    const std::unique_ptr<ToolConfiguration> roundTripped = toolConfigurationFromJson(json);

    REQUIRE(roundTripped->type() == ToolType::OrderChaos);
    REQUIRE(roundTripped->name() == "Toward Chaos");
    REQUIRE(roundTripped->falloff() == 0.2f);
    const auto& orderChaos = dynamic_cast<const OrderChaosConfiguration&>(*roundTripped);
    REQUIRE(orderChaos.amount() == -0.3);
}

TEST_CASE("toolConfigurationFromJson() rejects an unrecognized type", "[core][tool_configuration]") {
    nlohmann::json json = ProceduralConfiguration{};
    json["type"] = "clone";  // a real ToolType value, but not yet a real tool - see its own docs.
    REQUIRE_THROWS_AS(toolConfigurationFromJson(json), std::invalid_argument);
}
