#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/tool_preset.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::NamedToolPreset;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::ToolType;

TEST_CASE("A fresh NamedToolPreset has no config", "[core][tool_preset]") {
    const NamedToolPreset fresh;
    REQUIRE(fresh.id == 0);
    REQUIRE(fresh.name.empty());
    REQUIRE(fresh.config == nullptr);
}

TEST_CASE("A NamedToolPreset's copy constructor deep-clones its own config, independent of the original",
          "[core][tool_preset]") {
    NamedToolPreset original;
    original.id = 5;
    original.name = "My Brush";
    auto procedural = std::make_unique<ProceduralConfiguration>();
    procedural->setTipShape(BrushTipShape::Star);
    original.config = std::move(procedural);

    const NamedToolPreset copy = original;

    REQUIRE(copy.id == 5);
    REQUIRE(copy.name == "My Brush");
    REQUIRE(copy.config != nullptr);
    REQUIRE(copy.config.get() != original.config.get());  // A real clone, not the same pointer.
    REQUIRE(dynamic_cast<const ProceduralConfiguration&>(*copy.config).tipShape() == BrushTipShape::Star);

    // Mutating the original's own config doesn't affect the copy.
    dynamic_cast<ProceduralConfiguration&>(*original.config).setTipShape(BrushTipShape::Circle);
    REQUIRE(dynamic_cast<const ProceduralConfiguration&>(*copy.config).tipShape() == BrushTipShape::Star);
}

TEST_CASE("A NamedToolPreset's copy assignment deep-clones its own config", "[core][tool_preset]") {
    NamedToolPreset original;
    original.id = 5;
    original.name = "My Brush";
    original.config = std::make_unique<ProceduralConfiguration>();

    NamedToolPreset copy;
    copy = original;

    REQUIRE(copy.name == "My Brush");
    REQUIRE(copy.config != nullptr);
    REQUIRE(copy.config.get() != original.config.get());
}

TEST_CASE("A NamedToolPreset round-trips through JSON, config included", "[core][tool_preset]") {
    NamedToolPreset original;
    original.id = 9;
    original.name = "Piano Brush";
    auto procedural = std::make_unique<ProceduralConfiguration>();
    procedural->setTipShape(BrushTipShape::Diamond);
    procedural->setName("Piano Brush");
    original.config = std::move(procedural);

    const nlohmann::json json = original;
    const auto restored = json.get<NamedToolPreset>();

    REQUIRE(restored.id == 9);
    REQUIRE(restored.name == "Piano Brush");
    REQUIRE(restored.config != nullptr);
    REQUIRE(restored.config->type() == ToolType::Procedural);
    REQUIRE(dynamic_cast<const ProceduralConfiguration&>(*restored.config).tipShape() == BrushTipShape::Diamond);
}

TEST_CASE("Serializing a NamedToolPreset with no config throws", "[core][tool_preset]") {
    const NamedToolPreset empty;
    nlohmann::json json;
    REQUIRE_THROWS_AS(to_json(json, empty), std::invalid_argument);
}
