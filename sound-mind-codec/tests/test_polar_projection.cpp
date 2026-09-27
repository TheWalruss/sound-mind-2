#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/codec/polar_projection.h"

using sound_mind::codec::polarToRect;
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

TEST_CASE("polarToRect produces the requested output dimensions", "[polar_projection]") {
    const RgbImage source = makeImage(40, 40, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{128, 128, 128};
    });

    const RgbImage result = polarToRect(source, 20.0, 20.0, 15.0, 0.0, 0.0, 50, 30);

    CHECK(result.width == 50);
    CHECK(result.height == 30);
    CHECK(result.pixels.size() == std::size_t{50} * 30 * 3);
}

TEST_CASE("polarToRect returns an all-black image for a zero output size or an empty source",
          "[polar_projection]") {
    const RgbImage source = makeImage(40, 40, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{200, 200, 200};
    });

    const RgbImage zeroWidth = polarToRect(source, 20.0, 20.0, 15.0, 0.0, 0.0, 0, 30);
    CHECK(zeroWidth.pixels.empty());
    const RgbImage zeroHeight = polarToRect(source, 20.0, 20.0, 15.0, 0.0, 0.0, 50, 0);
    CHECK(zeroHeight.pixels.empty());

    const RgbImage emptySource = polarToRect(RgbImage{}, 20.0, 20.0, 15.0, 0.0, 0.0, 50, 30);
    for (std::uint8_t value : emptySource.pixels) {
        CHECK(value == 0);
    }
}

TEST_CASE("polarToRect's top row (full circle) samples straight up from the origin at column zero",
          "[polar_projection]") {
    // A bright two-row band straight above the origin - two, not one, for
    // the same bilinear-tolerance reason rectToPolar()'s own equivalent
    // tests use.
    const RgbImage source = makeImage(40, 40, [](std::uint32_t, std::uint32_t y) {
        return (y == 4 || y == 5) ? std::array<std::uint8_t, 3>{255, 255, 255} : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    // originY - maxRadius = 20 - 15 = 5, landing in the bright band.
    const RgbImage result = polarToRect(source, 20.0, 20.0, 15.0, 0.0, 0.0, 20, 10);

    CHECK(redAt(result, 0, 0) > 200);
}

TEST_CASE("polarToRect's bottom row samples the origin itself, regardless of column",
          "[polar_projection]") {
    // A small bright patch exactly at the origin (20, 20) - every column
    // of the output's own last row (r=0) should land on it.
    const RgbImage source = makeImage(40, 40, [](std::uint32_t x, std::uint32_t y) {
        return (x >= 19 && x <= 21 && y >= 19 && y <= 21) ? std::array<std::uint8_t, 3>{255, 255, 255}
                                                           : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    const RgbImage result = polarToRect(source, 20.0, 20.0, 15.0, 0.0, 0.0, 20, 10);

    for (std::uint32_t column = 0; column < 20; column += 4) {
        CHECK(redAt(result, column, 9) > 200);
    }
}

TEST_CASE("polarToRect's arc range restricts which angles the output columns sample",
          "[polar_projection]") {
    // A bright two-column band directly to the right of the origin (three
    // o'clock) and a separate bright two-row band directly below it (six
    // o'clock) - a quarter-arc from three o'clock to six o'clock should
    // sample the first at its own first output column and the second at
    // its own last, both at the outer ring (row 0).
    const RgbImage source = makeImage(60, 60, [](std::uint32_t x, std::uint32_t y) {
        const bool rightOfOrigin = (x == 44 || x == 45) && y >= 25 && y <= 35;  // three o'clock band.
        const bool belowOrigin = (y == 44 || y == 45) && x >= 25 && x <= 35;    // six o'clock band.
        return (rightOfOrigin || belowOrigin) ? std::array<std::uint8_t, 3>{255, 255, 255}
                                               : std::array<std::uint8_t, 3>{0, 0, 0};
    });

    constexpr double kHalfPi = 1.5707963267948966;
    constexpr double kPi = 3.141592653589793;
    const RgbImage result = polarToRect(source, 30.0, 30.0, 15.0, kHalfPi, kPi, 10, 5);

    CHECK(redAt(result, 0, 0) > 200);  // Three o'clock - the arc's own start.
    CHECK(redAt(result, 9, 0) > 200);  // Six o'clock - the arc's own end.
}

TEST_CASE("polarToRect leaves a sample point that falls outside the source's own bounds black",
          "[polar_projection]") {
    // An all-white source, but the origin/radius are chosen so the outer
    // ring at twelve o'clock samples a point above row 0 - genuinely
    // outside the image, not just near its edge.
    const RgbImage source = makeImage(40, 40, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 3>{255, 255, 255};
    });

    const RgbImage result = polarToRect(source, 20.0, 5.0, 15.0, 0.0, 0.0, 20, 10);

    CHECK(redAt(result, 0, 0) == 0);
}
