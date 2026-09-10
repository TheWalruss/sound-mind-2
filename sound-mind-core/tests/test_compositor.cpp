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
    CHECK_FALSE(renderLayer(layer, 2).has_value());
}

TEST_CASE("renderLayer renders a layer's content the same way toRgbImage does when canvasWidth "
          "matches the content and the layer is untranslated/unrescaled",
          "[core][compositor]") {
    StreamImage content;
    content.config.binCount = 2;
    content.frameCount = 2;
    content.leftMagnitudeDb = {0.0f, -50.0f, -30.0f, -96.0f};
    content.rightMagnitudeDb = {-10.0f, -60.0f, -40.0f, -96.0f};
    content.sharedPhaseRadians = {0.0f, 1.0f, -1.0f, 2.0f};

    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(content);

    const auto rendered = renderLayer(layer, /*canvasWidth=*/2);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(content);
    CHECK(rendered->width == expected.width);
    CHECK(rendered->height == expected.height);
    CHECK(rendered->pixels == expected.pixels);
}

namespace {

/// @brief A single-bin, per-column-distinctive StreamImage: column `i`
/// renders as a grayscale-ish value derived from `i`, so shifting/scaling
/// its columns around is easy to check pixel-by-pixel below. Column 0
/// (0 dB) is brightest; each later column is 20 dB quieter.
StreamImage makeGradientContent(std::uint32_t columnCount) {
    StreamImage content;
    content.config.binCount = 1;
    content.frameCount = columnCount;
    content.leftMagnitudeDb.resize(columnCount);
    content.rightMagnitudeDb.resize(columnCount);
    content.sharedPhaseRadians.assign(columnCount, 0.0f);
    for (std::uint32_t i = 0; i < columnCount; ++i) {
        content.leftMagnitudeDb[i] = -static_cast<float>(i) * 20.0f;
        content.rightMagnitudeDb[i] = -static_cast<float>(i) * 20.0f;
    }
    return content;
}

/// @brief Whether column `x` of `image` is pure black (the padding color
/// renderLayer() uses for columns outside the layer's own content).
bool isBlackColumn(const sound_mind::codec::RgbImage& image, std::uint32_t x) {
    for (std::uint32_t y = 0; y < image.height; ++y) {
        const std::size_t index = (std::size_t{y} * image.width + x) * 3;
        if (image.pixels[index] != 0 || image.pixels[index + 1] != 0 || image.pixels[index + 2] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("renderLayer pads a layer narrower than canvasWidth with black on the right",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(2));

    const auto rendered = renderLayer(layer, /*canvasWidth=*/5);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 5);
    const auto expected = toRgbImage(makeGradientContent(2));
    CHECK(rendered->pixels[0] == expected.pixels[0]);
    CHECK(rendered->pixels[3] == expected.pixels[3]);  // column 1
    CHECK(isBlackColumn(*rendered, 2));
    CHECK(isBlackColumn(*rendered, 3));
    CHECK(isBlackColumn(*rendered, 4));
}

TEST_CASE("renderLayer crops a layer wider than canvasWidth", "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(5));

    const auto rendered = renderLayer(layer, /*canvasWidth=*/2);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 2);
    const auto expected = toRgbImage(makeGradientContent(5));
    CHECK(rendered->pixels[0] == expected.pixels[0]);  // column 0, unchanged
    CHECK(rendered->pixels[3] == expected.pixels[3]);  // column 1, unchanged
}

TEST_CASE("renderLayer's positive translationColumns shifts content later (right), padding the "
          "start with black",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(3));
    layer.setTranslationColumns(2);

    const auto rendered = renderLayer(layer, /*canvasWidth=*/5);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(makeGradientContent(3));
    CHECK(isBlackColumn(*rendered, 0));
    CHECK(isBlackColumn(*rendered, 1));
    CHECK(rendered->pixels[2 * 3] == expected.pixels[0]);  // source column 0 now at x=2
    CHECK(rendered->pixels[3 * 3] == expected.pixels[3]);  // source column 1 now at x=3
    CHECK(rendered->pixels[4 * 3] == expected.pixels[6]);  // source column 2 now at x=4
}

TEST_CASE("renderLayer's negative translationColumns shifts content earlier (left), dropping "
          "leading columns and padding the end with black",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(3));
    layer.setTranslationColumns(-1);

    const auto rendered = renderLayer(layer, /*canvasWidth=*/3);

    REQUIRE(rendered.has_value());
    const auto expected = toRgbImage(makeGradientContent(3));
    CHECK(rendered->pixels[0] == expected.pixels[3]);  // source column 1 now at x=0
    CHECK(rendered->pixels[3] == expected.pixels[6]);  // source column 2 now at x=1
    CHECK(isBlackColumn(*rendered, 2));                // source column 3 doesn't exist
}

TEST_CASE("renderLayer's rescaleFactor stretches the layer's own timeline before placement",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(2));
    layer.setRescaleFactor(2.0);  // 2 columns -> 4, each source column doubled up.

    const auto rendered = renderLayer(layer, /*canvasWidth=*/4);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 4);
    const auto expected = toRgbImage(makeGradientContent(2));
    CHECK(rendered->pixels[0 * 3] == expected.pixels[0]);  // still source column 0
    CHECK(rendered->pixels[1 * 3] == expected.pixels[0]);  // still source column 0 (stretched)
    CHECK(rendered->pixels[2 * 3] == expected.pixels[3]);  // now source column 1
    CHECK(rendered->pixels[3 * 3] == expected.pixels[3]);  // still source column 1 (stretched)
}

TEST_CASE("renderLayer's rescaleFactor compresses the layer's own timeline before placement",
          "[core][compositor]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    layer.setContent(makeGradientContent(4));
    layer.setRescaleFactor(0.5);  // 4 columns -> 2.

    const auto rendered = renderLayer(layer, /*canvasWidth=*/4);

    REQUIRE(rendered.has_value());
    CHECK(rendered->width == 4);
    CHECK(isBlackColumn(*rendered, 2));  // only 2 real columns now - the rest is padding.
    CHECK(isBlackColumn(*rendered, 3));
}
