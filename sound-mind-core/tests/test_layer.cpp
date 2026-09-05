#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/layer.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerType;

TEST_CASE("A Layer reports the id, name, and type it was constructed with", "[core][layer]") {
    const Layer layer(7, "Vocals", LayerType::Normal);
    REQUIRE(layer.id() == 7);
    REQUIRE(layer.name() == "Vocals");
    REQUIRE(layer.type() == LayerType::Normal);
}

TEST_CASE("A Layer can be renamed", "[core][layer]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setName("Drums");
    REQUIRE(layer.name() == "Drums");
}
