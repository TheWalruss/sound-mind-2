#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/layer.h"

using sound_mind::codec::PoolImage;
using sound_mind::codec::StreamImage;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::Layer;
using sound_mind::core::LayerType;

TEST_CASE("A Layer reports the id, name, and type it was constructed with", "[core][layer]") {
    const Layer layer(7, "Vocals", LayerType::Normal);
    REQUIRE(layer.id() == 7);
    REQUIRE(layer.name() == "Vocals");
    REQUIRE(layer.type() == LayerType::Normal);
}

TEST_CASE("A Layer defaults to full opacity", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE(layer.opacity() == 1.0f);
}

TEST_CASE("A Layer can be renamed", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setName("Drums");
    REQUIRE(layer.name() == "Drums");
}

TEST_CASE("A Layer's opacity can be changed", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setOpacity(0.5f);
    REQUIRE(layer.opacity() == 0.5f);
}

TEST_CASE("A Layer's opacity is unbound by default", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE_FALSE(layer.opacityMindWave().has_value());
}

TEST_CASE("A Layer's opacity can be bound to (and unbound from) a MindWave", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setOpacityMindWave(sound_mind::core::MindWaveId{7});
    REQUIRE(layer.opacityMindWave() == sound_mind::core::MindWaveId{7});

    layer.setOpacityMindWave(std::nullopt);
    REQUIRE_FALSE(layer.opacityMindWave().has_value());
}

TEST_CASE("A Layer defaults to visible", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE(layer.visible());
}

TEST_CASE("A Layer's visibility can be changed", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setVisible(false);
    REQUIRE_FALSE(layer.visible());
}

TEST_CASE("A Layer defaults to no horizontal translation", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE(layer.translationColumns() == 0);
}

TEST_CASE("A Layer's horizontal translation can be changed", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setTranslationColumns(-50);
    REQUIRE(layer.translationColumns() == -50);
}

TEST_CASE("A Layer defaults to no horizontal rescale", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE(layer.rescaleFactor() == 1.0);
}

TEST_CASE("A Layer's horizontal rescale can be changed", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setRescaleFactor(2.5);
    REQUIRE(layer.rescaleFactor() == 2.5);
}

TEST_CASE("A Layer round-trips through JSON", "[core][layer]") {
    Layer original(42, "Vocals", LayerType::Background);
    original.setOpacity(0.75f);
    original.setOpacityMindWave(sound_mind::core::MindWaveId{9});
    original.setVisible(false);
    original.setTranslationColumns(120);
    original.setRescaleFactor(1.5);
    original.filterConfiguration().setType(FilterType::Sharpen);
    original.filterConfiguration().setSharpenAmount(2.0f);

    const nlohmann::json json = original;
    const Layer restored = json.get<Layer>();

    REQUIRE(restored.id() == original.id());
    REQUIRE(restored.name() == original.name());
    REQUIRE(restored.type() == original.type());
    REQUIRE(restored.opacity() == original.opacity());
    REQUIRE(restored.opacityMindWave() == original.opacityMindWave());
    REQUIRE(restored.visible() == original.visible());
    REQUIRE(restored.translationColumns() == original.translationColumns());
    REQUIRE(restored.rescaleFactor() == original.rescaleFactor());
    REQUIRE(restored.filterConfiguration().type() == FilterType::Sharpen);
    REQUIRE(restored.filterConfiguration().sharpenAmount() == 2.0f);
}

TEST_CASE("A Layer loads from JSON missing opacityMindWaveId (a layer saved before "
          "v0.Y.31.1 Installment C1) as unbound",
          "[core][layer]") {
    const nlohmann::json json{
        {"id", 1}, {"name", "Untitled"}, {"type", "normal"}, {"opacity", 1.0f}, {"visible", true},
    };

    const Layer restored = json.get<Layer>();

    REQUIRE_FALSE(restored.opacityMindWave().has_value());
}

TEST_CASE("A Layer loads from JSON missing visible (a layer saved before v0.Y.13.1) as visible",
          "[core][layer]") {
    const nlohmann::json json{
        {"id", 1}, {"name", "Untitled"}, {"type", "normal"}, {"opacity", 1.0f},
    };

    const Layer restored = json.get<Layer>();

    REQUIRE(restored.visible());
}

TEST_CASE("A Layer loads from JSON missing translationColumns/rescaleFactor "
          "(a layer saved before v0.Y.21.1) as untranslated and unrescaled",
          "[core][layer]") {
    const nlohmann::json json{
        {"id", 1}, {"name", "Untitled"}, {"type", "normal"}, {"opacity", 1.0f}, {"visible", true},
    };

    const Layer restored = json.get<Layer>();

    REQUIRE(restored.translationColumns() == 0);
    REQUIRE(restored.rescaleFactor() == 1.0);
}

TEST_CASE("A Layer defaults to a fresh FilterConfiguration", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE(layer.filterConfiguration().type() == FilterType::FrequencyAxisGradient);
}

TEST_CASE("A Layer's filter configuration can be mutated in place", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Filter);
    layer.filterConfiguration().setType(FilterType::UniformBlur);
    REQUIRE(layer.filterConfiguration().type() == FilterType::UniformBlur);
}

TEST_CASE("A Layer loads from JSON missing filterConfiguration (a layer saved before v0.Y.28.1) "
          "with a fresh default one",
          "[core][layer]") {
    const nlohmann::json json{
        {"id", 1}, {"name", "Untitled"}, {"type", "normal"}, {"opacity", 1.0f}, {"visible", true},
    };

    const Layer restored = json.get<Layer>();

    REQUIRE(restored.filterConfiguration().type() == FilterType::FrequencyAxisGradient);
}

TEST_CASE("A Layer has no content by default", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE_FALSE(layer.content().has_value());
}

TEST_CASE("A Layer's content can be set", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    StreamImage content;
    content.config.binCount = 4;
    content.frameCount = 2;

    layer.setContent(content);

    REQUIRE(layer.content().has_value());
    CHECK(layer.content()->config.binCount == 4);
    CHECK(layer.content()->frameCount == 2);
}

TEST_CASE("A Layer's content is not part of its JSON representation", "[core][layer]") {
    // Per Layer::content()'s docs: cached content is persisted separately,
    // as its own Stream file under the project's media/ folder - not
    // inlined into the layer's JSON. Project::save()/load() are what
    // actually read and write that file; a bare Layer<->JSON round-trip
    // should never carry content either way.
    Layer original(1, "Untitled", LayerType::Normal);
    original.setContent(StreamImage{});

    const nlohmann::json json = original;
    const Layer restored = json.get<Layer>();

    REQUIRE(original.content().has_value());
    REQUIRE_FALSE(restored.content().has_value());
}

TEST_CASE("A Layer has no Pool content by default", "[core][layer]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    REQUIRE_FALSE(layer.poolContent().has_value());
}

TEST_CASE("A Layer's Pool content can be set", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    PoolImage content;
    content.config.binCount = 4;
    content.frameCount = 2;

    layer.setPoolContent(content);

    REQUIRE(layer.poolContent().has_value());
    CHECK(layer.poolContent()->config.binCount == 4);
    CHECK(layer.poolContent()->frameCount == 2);
}

TEST_CASE("A Layer's Pool content is not part of its JSON representation", "[core][layer]") {
    Layer original(1, "Untitled", LayerType::Normal);
    original.setPoolContent(PoolImage{});

    const nlohmann::json json = original;
    const Layer restored = json.get<Layer>();

    REQUIRE(original.poolContent().has_value());
    REQUIRE_FALSE(restored.poolContent().has_value());
}
