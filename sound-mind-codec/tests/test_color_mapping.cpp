#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "sound_mind/codec/color_mapping.h"

using sound_mind::codec::fromRgbImage;
using sound_mind::codec::RgbImage;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::codec::toGrayscaleImage;
using sound_mind::codec::toRgbImage;
using sound_mind::codec::dbfsToIntensityValue;
using sound_mind::codec::intensityValueToDbfs;
using sound_mind::codec::markClippedCells;

namespace {

/// @brief A tiny 2 bin x 2 frame StreamImage with hand-picked, boundary-
/// exercising values: full-scale (0 dB), silent (below the -96 dB floor),
/// and a couple of phase values.
StreamImage makeTestImage() {
    StreamImage image;
    image.config.binCount = 2;
    image.frameCount = 2;
    image.sampleCount = 100;
    // [bin][frame], row-major: cell = bin * frameCount + frame.
    image.leftMagnitudeDb = {0.0f, -200.0f, -48.0f, -96.0f};
    image.rightMagnitudeDb = {-96.0f, 0.0f, -48.0f, -200.0f};
    image.sharedPhaseRadians = {0.0f, std::numbers::pi_v<float>, -std::numbers::pi_v<float>,
                                 std::numbers::pi_v<float> / 2.0f};
    return image;
}

}  // namespace

TEST_CASE("toRgbImage maps amplitude and phase to bytes at their range boundaries", "[color_mapping]") {
    const StreamImage image = makeTestImage();
    const RgbImage rgb = toRgbImage(image);

    REQUIRE(rgb.width == 2);
    REQUIRE(rgb.height == 2);

    // Row 0 = highest frequency = bin 1 (binCount - 1). Cell (bin=1, frame=0)
    // is leftMagnitudeDb = -48 dB (mid-range), rightMagnitudeDb = -48 dB,
    // phase = -pi.
    const std::size_t topLeftPixel = 0;
    CHECK(rgb.pixels[topLeftPixel + 0] > 120);  // -48 dB is roughly half of the -96..0 range
    CHECK(rgb.pixels[topLeftPixel + 0] < 135);
    CHECK(rgb.pixels[topLeftPixel + 2] == 0);  // -pi maps to the bottom of the phase range

    // Row 1 = bin 0. Cell (bin=0, frame=0): left = 0 dB (full scale) -> 255,
    // right = -96 dB (the floor) -> 0, phase = 0 -> mid-range.
    const std::size_t bottomLeftPixel = (1 * rgb.width + 0) * 3;
    CHECK(rgb.pixels[bottomLeftPixel + 0] == 255);
    CHECK(rgb.pixels[bottomLeftPixel + 1] == 0);

    // Cell (bin=0, frame=1): left = -200 dB, well below the floor, clamps to 0.
    const std::size_t bottomRightPixel = (1 * rgb.width + 1) * 3;
    CHECK(rgb.pixels[bottomRightPixel + 0] == 0);
}

TEST_CASE("toGrayscaleImage produces equal R, G, and B at every pixel", "[color_mapping]") {
    const StreamImage image = makeTestImage();
    const RgbImage gray = toGrayscaleImage(image);

    for (std::size_t pixel = 0; pixel < gray.pixelCount(); ++pixel) {
        CHECK(gray.pixels[pixel * 3 + 0] == gray.pixels[pixel * 3 + 1]);
        CHECK(gray.pixels[pixel * 3 + 1] == gray.pixels[pixel * 3 + 2]);
    }
}

TEST_CASE("toGrayscaleImage(field) maps 0.0/0.5/1.0 to 0/roughly-mid/255 linearly, not via dB",
          "[color_mapping]") {
    // [bin][frame], row-major: cell = bin * frameCount + frame - same
    // layout the StreamImage overload's own test above uses.
    const std::vector<float> field = {0.0f, 1.0f, 0.5f, 0.25f};
    const RgbImage gray = toGrayscaleImage(field, /*frameCount=*/2, /*binCount=*/2);

    REQUIRE(gray.width == 2);
    REQUIRE(gray.height == 2);

    // Row 0 = highest frequency = bin 1: cell (bin=1, frame=0) = 0.5,
    // cell (bin=1, frame=1) = 0.25.
    CHECK(gray.pixels[0] == 128);  // std::lround(0.5 * 255) == 128, not the StreamImage overload's dB curve.
    CHECK(gray.pixels[3] == 64);   // std::lround(0.25 * 255) == 64.

    // Row 1 = bin 0: cell (bin=0, frame=0) = 0.0, cell (bin=0, frame=1) = 1.0.
    CHECK(gray.pixels[6] == 0);
    CHECK(gray.pixels[9] == 255);
}

TEST_CASE("toGrayscaleImage(field) clamps out-of-range input instead of rejecting it", "[color_mapping]") {
    const std::vector<float> field = {-1.0f, 2.0f};
    const RgbImage gray = toGrayscaleImage(field, /*frameCount=*/2, /*binCount=*/1);

    CHECK(gray.pixels[0] == 0);
    CHECK(gray.pixels[3] == 255);
}

TEST_CASE("toGrayscaleImage(field) produces equal R, G, and B at every pixel", "[color_mapping]") {
    const std::vector<float> field = {0.1f, 0.4f, 0.7f, 0.9f};
    const RgbImage gray = toGrayscaleImage(field, /*frameCount=*/2, /*binCount=*/2);

    for (std::size_t pixel = 0; pixel < gray.pixelCount(); ++pixel) {
        CHECK(gray.pixels[pixel * 3 + 0] == gray.pixels[pixel * 3 + 1]);
        CHECK(gray.pixels[pixel * 3 + 1] == gray.pixels[pixel * 3 + 2]);
    }
}

TEST_CASE("fromRgbImage inverts toRgbImage within 8-bit quantization tolerance", "[color_mapping]") {
    StreamImage original = makeTestImage();
    // Clamp the source to the representable range first, since values below
    // the -96 dB floor (like -200 dB above) can't round-trip through an
    // 8-bit image - they clamp to the same byte as -96 dB.
    for (float& db : original.leftMagnitudeDb) {
        db = std::max(db, -96.0f);
    }
    for (float& db : original.rightMagnitudeDb) {
        db = std::max(db, -96.0f);
    }

    const RgbImage rgb = toRgbImage(original);
    const StreamImage roundTripped = fromRgbImage(rgb, StreamCodecConfig{});

    REQUIRE(roundTripped.config.binCount == original.config.binCount);
    REQUIRE(roundTripped.frameCount == original.frameCount);

    constexpr float kDbTolerance = 96.0f / 255.0f;
    constexpr float kPhaseTolerance = 2.0f * std::numbers::pi_v<float> / 255.0f;
    for (std::size_t cell = 0; cell < original.leftMagnitudeDb.size(); ++cell) {
        CHECK(roundTripped.leftMagnitudeDb[cell] == Catch::Approx(original.leftMagnitudeDb[cell]).margin(kDbTolerance));
        CHECK(roundTripped.rightMagnitudeDb[cell] ==
              Catch::Approx(original.rightMagnitudeDb[cell]).margin(kDbTolerance));
    }
    // Phase near the wraparound boundary (+-pi) is excluded: -pi and +pi are
    // the same angle but map to opposite ends of the byte range, so a
    // straightforward margin check isn't meaningful right at that seam.
    CHECK(roundTripped.sharedPhaseRadians[3] == Catch::Approx(original.sharedPhaseRadians[3]).margin(kPhaseTolerance));
}

TEST_CASE("The intensity value scale maps the noise floor to 0 and full scale to 1, linearly in dB",
          "[color_mapping][intensity]") {
    CHECK(dbfsToIntensityValue(-96.0f) == Catch::Approx(0.0f));
    CHECK(dbfsToIntensityValue(0.0f) == Catch::Approx(1.0f));
    CHECK(dbfsToIntensityValue(-48.0f) == Catch::Approx(0.5f));
    // Not clamped - above full scale reads above 1, below the floor below 0.
    CHECK(dbfsToIntensityValue(6.0f) > 1.0f);
    CHECK(dbfsToIntensityValue(-120.0f) < 0.0f);
    for (const float db : {-96.0f, -40.0f, -6.0f, 0.0f, 3.0f}) {
        CHECK(intensityValueToDbfs(dbfsToIntensityValue(db)) == Catch::Approx(db).margin(1e-4));
    }
}

TEST_CASE("markClippedCells marks exactly the cells above full scale on either channel",
          "[color_mapping][intensity]") {
    StreamImage image;
    image.config.binCount = 2;
    image.frameCount = 2;
    image.sampleCount = 100;
    // [bin][frame]: cell 0 left over, cell 1 right over, cell 2 exactly 0 dB, cell 3 quiet.
    image.leftMagnitudeDb = {3.0f, -10.0f, 0.0f, -50.0f};
    image.rightMagnitudeDb = {-10.0f, 1.0f, 0.0f, -50.0f};
    image.sharedPhaseRadians = {0.0f, 0.0f, 0.0f, 0.0f};
    RgbImage rgb = toRgbImage(image);
    const RgbImage before = rgb;

    const std::size_t marked = markClippedCells(image, rgb);

    CHECK(marked == 2);
    // Display rows are flipped (row 0 = highest bin), so bin 0 is row 1.
    auto pixelChanged = [&](std::uint32_t row, std::uint32_t frame) {
        const std::size_t i = (static_cast<std::size_t>(row) * rgb.width + frame) * 3;
        return rgb.pixels[i] != before.pixels[i] || rgb.pixels[i + 1] != before.pixels[i + 1] ||
               rgb.pixels[i + 2] != before.pixels[i + 2];
    };
    CHECK(pixelChanged(1, 0));        // bin 0, frame 0 - left over.
    CHECK(pixelChanged(1, 1));        // bin 0, frame 1 - right over.
    CHECK_FALSE(pixelChanged(0, 0));  // bin 1, frame 0 - exactly full scale.
    CHECK_FALSE(pixelChanged(0, 1));  // bin 1, frame 1 - quiet.
}
