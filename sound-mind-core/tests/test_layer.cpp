#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"

using sound_mind::codec::PoolImage;
using sound_mind::codec::StreamImage;
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
