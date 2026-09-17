#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::StampMode;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::toolConfigurationFromJson;
using sound_mind::core::ToolType;

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

TEST_CASE("toolConfigurationFromJson() rejects an unrecognized type", "[core][tool_configuration]") {
    nlohmann::json json = ProceduralConfiguration{};
    json["type"] = "mindShot";
    REQUIRE_THROWS_AS(toolConfigurationFromJson(json), std::invalid_argument);
}
