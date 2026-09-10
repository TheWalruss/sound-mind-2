#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::ToolType;

TEST_CASE("A fresh ToolConfiguration is Procedural with a circular tip", "[core][tool_configuration]") {
    const ToolConfiguration config;
    REQUIRE(config.type() == ToolType::Procedural);
    REQUIRE(config.tipShape() == BrushTipShape::Circle);
}

TEST_CASE("A fresh ToolConfiguration has an empty (unsaved) name", "[core][tool_configuration]") {
    const ToolConfiguration config;
    REQUIRE(config.name().empty());
}

TEST_CASE("A fresh ToolConfiguration has a fresh, fully transparent default gradient", "[core][tool_configuration]") {
    const ToolConfiguration config;
    REQUIRE(config.defaultGradient().stops().size() == 2);
    REQUIRE(config.defaultGradient().stops().front().leftOpacity == 0.0f);
}

TEST_CASE("A ToolConfiguration can be named", "[core][tool_configuration]") {
    ToolConfiguration config;
    config.setName("My Brush");
    REQUIRE(config.name() == "My Brush");
}

TEST_CASE("A ToolConfiguration's tip shape can be changed", "[core][tool_configuration]") {
    ToolConfiguration config;
    config.setTipShape(BrushTipShape::Star);
    REQUIRE(config.tipShape() == BrushTipShape::Star);
}

TEST_CASE("A ToolConfiguration's falloff can be changed", "[core][tool_configuration]") {
    ToolConfiguration config;
    config.setFalloff(0.9f);
    REQUIRE(config.falloff() == 0.9f);
}

TEST_CASE("A ToolConfiguration's size can be changed", "[core][tool_configuration]") {
    ToolConfiguration config;
    config.setSize(2.5);
    REQUIRE(config.size() == 2.5);
}

TEST_CASE("A ToolConfiguration's default gradient can be mutated in place", "[core][tool_configuration]") {
    ToolConfiguration config;
    config.defaultGradient().setLinkChannels(true);
    REQUIRE(config.defaultGradient().linkChannels());
}

TEST_CASE("A ToolConfiguration round-trips through JSON", "[core][tool_configuration]") {
    ToolConfiguration config;
    config.setName("My Brush");
    config.setTipShape(BrushTipShape::Diamond);
    config.setFalloff(0.25f);
    config.setSize(3.0);
    config.defaultGradient().setLinkChannels(true);

    const nlohmann::json json = config;
    const ToolConfiguration roundTripped = json.get<ToolConfiguration>();

    REQUIRE(roundTripped.type() == ToolType::Procedural);
    REQUIRE(roundTripped.name() == "My Brush");
    REQUIRE(roundTripped.tipShape() == BrushTipShape::Diamond);
    REQUIRE(roundTripped.falloff() == 0.25f);
    REQUIRE(roundTripped.size() == 3.0);
    REQUIRE(roundTripped.defaultGradient().linkChannels());
}
