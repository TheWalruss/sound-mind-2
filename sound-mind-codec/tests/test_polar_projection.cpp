#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/polar_projection.h"

using sound_mind::codec::rectToPolar;
using sound_mind::codec::RgbImage;

namespace {

/// @brief A `width` x `height` RgbImage where pixel (x, y) is
/// `(r(x, y), g(x, y), b(x, y))` - the same per-pixel-callback helper
/// `test_rgb_image_resample.cpp` already uses.
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

std::uint8_t redAt(const RgbImage& image, std::uint32_t x, std::uint32_t y) {
    return image.pixels[(std::size_t{y} * image.width + x) * 3];
}

}  // namespace

TEST_CASE("rectToPolar produces a diameter x diameter square", "[polar_projection]") {
    const RgbImage source = makeImage(20, 10, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{128, 128, 128};
    });

    const RgbImage result = rectToPolar(source, 40);

    CHECK(result.width == 40);
    CHECK(result.height == 40);
    CHECK(result.pixels.size() == std::size_t{40} * 40 * 3);
}

TEST_CASE("rectToPolar returns an all-black image for a zero diameter or an empty source",
          "[polar_projection]") {
    const RgbImage source = makeImage(20, 10, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{200, 200, 200};
    });

    const RgbImage zeroDiameter = rectToPolar(source, 0);
    CHECK(zeroDiameter.pixels.empty());

    const RgbImage emptySource = rectToPolar(RgbImage{}, 10);
    for (std::uint8_t value : emptySource.pixels) {
        CHECK(value == 0);
    }
}

TEST_CASE("rectToPolar leaves every pixel outside the disk black", "[polar_projection]") {
    const RgbImage source = makeImage(16, 8, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{255, 255, 255};
    });

    const RgbImage result = rectToPolar(source, 32);

    // The four corners of the square output are always outside the
    // inscribed disk, regardless of source content.
    CHECK(redAt(result, 0, 0) == 0);
    CHECK(redAt(result, 31, 0) == 0);
    CHECK(redAt(result, 0, 31) == 0);
    CHECK(redAt(result, 31, 31) == 0);
}

TEST_CASE("rectToPolar's centre samples the source's own bottom row (lowest frequency)",
          "[polar_projection]") {
    // Bottom row (y = source.height - 1) is bright; every other row is
    // dark - so the flower's own centre (r = 0, per this function's own
    // docs: "r = 0 at the centre sampling source's own bottom row") should
    // read as bright, distinguishing it from a implementation that instead
    // centred on the top row.
    constexpr std::uint32_t kSourceHeight = 10;
    const RgbImage source = makeImage(20, kSourceHeight, [](std::uint32_t, std::uint32_t y) {
        return y == kSourceHeight - 1 ? std::array<std::uint8_t, 3>{255, 255, 255}
                                       : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    constexpr std::uint32_t kDiameter = 40;
    const RgbImage result = rectToPolar(source, kDiameter);

    const std::uint32_t centre = kDiameter / 2;
    CHECK(redAt(result, centre, centre) > 200);
}

TEST_CASE("rectToPolar's outer ring at twelve o'clock samples the source's own top row at time zero",
          "[polar_projection]") {
    // The top two source rows (y=0,1 - time zero, highest frequency) are
    // bright; every other row is dark - two rows, not one, so the near-edge
    // sample point below (whose own y_src lands a little inside the top
    // edge, not exactly on it) still bilinearly interpolates between two
    // bright neighbors rather than partially diluting against a dark one.
    const RgbImage source = makeImage(20, 10, [](std::uint32_t, std::uint32_t y) {
        return y <= 1 ? std::array<std::uint8_t, 3>{255, 255, 255} : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    constexpr std::uint32_t kDiameter = 40;
    const RgbImage result = rectToPolar(source, kDiameter);

    const std::uint32_t centre = kDiameter / 2;
    // Just inside the outer edge, straight up from centre.
    CHECK(redAt(result, centre, 1) > 200);
}

TEST_CASE("rectToPolar's ring a quarter-turn clockwise from twelve o'clock samples a quarter into the "
          "source's own width",
          "[polar_projection]") {
    // Two adjacent bright source columns straddling a quarter of the way
    // across the source (time = W/4) - two, not one, for the same
    // bilinear-tolerance reason the previous test's own two-row band uses.
    constexpr std::uint32_t kSourceWidth = 40;
    const RgbImage source = makeImage(kSourceWidth, 10, [](std::uint32_t x, std::uint32_t) {
        return (x == kSourceWidth / 4 || x == kSourceWidth / 4 + 1) ? std::array<std::uint8_t, 3>{255, 255, 255}
                                                                     : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    constexpr std::uint32_t kDiameter = 80;
    const RgbImage result = rectToPolar(source, kDiameter);

    const std::uint32_t centre = kDiameter / 2;
    CHECK(redAt(result, kDiameter - 2, centre) > 200);
}
