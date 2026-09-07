#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/layer.h"

using sound_mind::codec::StreamImage;
using sound_mind::codec::toRgbImage;
using sound_mind::core::Layer;
using sound_mind::core::LayerType;
using sound_mind::core::renderLayer;

TEST_CASE("renderLayer returns nullopt for a layer with no content", "[core][compositor]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    CHECK_FALSE(renderLayer(layer).has_value());
}

TEST_CASE("renderLayer renders a layer's content the same way toRgbImage does", "[core][compositor]") {
    StreamImage content;
    content.config.binCount = 2;
    content.frameCount = 2;
    content.leftMagnitudeDb = {0.0f, -50.0f, -30.0f, -96.0f};
    content.rightMagnitudeDb = {-10.0f, -60.0f, -40.0f, -96.0f};
    content.sharedPhaseRadians = {0.0f, 1.0f, -1.0f, 2.0f};

    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(content);

    const auto rendered = renderLayer(layer);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(content);
    CHECK(rendered->width == expected.width);
    CHECK(rendered->height == expected.height);
    CHECK(rendered->pixels == expected.pixels);
}
