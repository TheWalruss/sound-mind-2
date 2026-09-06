#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/layer.h"

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

TEST_CASE("A Layer round-trips through JSON", "[core][layer]") {
    Layer original(42, "Vocals", LayerType::Background);
    original.setOpacity(0.75f);

    const nlohmann::json json = original;
    const Layer restored = json.get<Layer>();

    REQUIRE(restored.id() == original.id());
    REQUIRE(restored.name() == original.name());
    REQUIRE(restored.type() == original.type());
    REQUIRE(restored.opacity() == original.opacity());
}
