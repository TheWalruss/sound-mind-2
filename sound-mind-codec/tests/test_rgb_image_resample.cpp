#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/rgb_image_resample.h"

using sound_mind::codec::downsampleAveraged;
using sound_mind::codec::RgbImage;

namespace {

/// @brief A `width` x `height` RgbImage where pixel (x, y) is
/// `(r(x, y), g(x, y), b(x, y))` - built via a per-pixel callback so each
/// test can hand-craft exactly the pattern it needs.
template <typename PixelFn>
RgbImage makeImage(std::uint32_t width, std::uint32_t height, PixelFn pixelAt) {
    RgbImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(std::size_t{width} * height * 3);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto [r, g, b] = pixelAt(x, y);
            const std::size_t index = (std::size_t{y} * width + x) * 3;
            image.pixels[index] = r;
            image.pixels[index + 1] = g;
            image.pixels[index + 2] = b;
        }
    }
    return image;
}

}  // namespace

TEST_CASE("downsampleAveraged produces the requested dimensions", "[rgb_image_resample]") {
    const RgbImage source = makeImage(8, 6, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{100, 100, 100};
    });

    const RgbImage result = downsampleAveraged(source, 3, 2);

    REQUIRE(result.width == 3);
    REQUIRE(result.height == 2);
    REQUIRE(result.pixels.size() == std::size_t{3} * 2 * 3);
}

TEST_CASE("downsampleAveraged of a solid color stays that color", "[rgb_image_resample]") {
    const RgbImage source = makeImage(10, 10, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{40, 120, 200};
    });

    const RgbImage result = downsampleAveraged(source, 4, 3);

    for (std::size_t i = 0; i < result.pixels.size(); i += 3) {
        CHECK(result.pixels[i] == 40);
        CHECK(result.pixels[i + 1] == 120);
        CHECK(result.pixels[i + 2] == 200);
    }
}

TEST_CASE("downsampleAveraged actually averages, not just samples one source pixel", "[rgb_image_resample]") {
    // A 2x1 checkerboard (black, white) shrunk to 1x1 must land near mid-
    // gray - nearest-neighbor sampling would instead land on one extreme
    // (0 or 255), missing the other source pixel entirely.
    const RgbImage source = makeImage(2, 1, [](std::uint32_t x, std::uint32_t) {
        return x == 0 ? std::array<std::uint8_t, 3>{0, 0, 0} : std::array<std::uint8_t, 3>{255, 255, 255};
    });

    const RgbImage result = downsampleAveraged(source, 1, 1);

    REQUIRE(result.pixels.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(result.pixels[i] >= 120);
        CHECK(result.pixels[i] <= 135);
    }
}

TEST_CASE("downsampleAveraged upscaling still fills every output pixel from the nearest source region",
          "[rgb_image_resample]") {
    const RgbImage source = makeImage(2, 2, [](std::uint32_t x, std::uint32_t y) {
        return (x == 0 && y == 0) ? std::array<std::uint8_t, 3>{255, 0, 0} : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    const RgbImage result = downsampleAveraged(source, 4, 4);

    REQUIRE(result.width == 4);
    REQUIRE(result.height == 4);
    // Top-left quadrant of the upscaled result should stay red-dominant.
    CHECK(result.pixels[0] > 0);
}

TEST_CASE("downsampleAveraged of a zero-sized source produces an all-black result, not a crash",
          "[rgb_image_resample]") {
    RgbImage source;  // width = height = 0, empty pixels.

    const RgbImage result = downsampleAveraged(source, 3, 3);

    REQUIRE(result.width == 3);
    REQUIRE(result.height == 3);
    for (const auto byte : result.pixels) {
        CHECK(byte == 0);
    }
}
