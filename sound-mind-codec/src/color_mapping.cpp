#include "sound_mind/codec/color_mapping.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace sound_mind::codec {

namespace {

// Matches the legacy Pool codec's dB floor convention (see
// docs/legacy/CODEC_DETAILS.md section 2) - a reasonable, already-precedented
// display range, even though Stream itself stores unclamped float dB.
constexpr float kDisplayMinDb = -96.0f;
constexpr float kDisplayMaxDb = 0.0f;

[[nodiscard]] std::uint8_t dbToByte(float db) noexcept {
    const float clamped = std::clamp(db, kDisplayMinDb, kDisplayMaxDb);
    const float normalized = (clamped - kDisplayMinDb) / (kDisplayMaxDb - kDisplayMinDb);
    return static_cast<std::uint8_t>(std::lround(normalized * 255.0f));
}

[[nodiscard]] float byteToDb(std::uint8_t value) noexcept {
    const float normalized = static_cast<float>(value) / 255.0f;
    return kDisplayMinDb + normalized * (kDisplayMaxDb - kDisplayMinDb);
}

[[nodiscard]] std::uint8_t phaseToByte(float radians) noexcept {
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
    float wrapped = std::fmod(radians + std::numbers::pi_v<float>, kTwoPi);
    if (wrapped < 0.0f) {
        wrapped += kTwoPi;
    }
    return static_cast<std::uint8_t>(std::lround((wrapped / kTwoPi) * 255.0f));
}

[[nodiscard]] float byteToPhase(std::uint8_t value) noexcept {
    const float normalized = static_cast<float>(value) / 255.0f;
    return normalized * 2.0f * std::numbers::pi_v<float> - std::numbers::pi_v<float>;
}

/// @brief The FFT size stream_codec.cpp's encode()/decode() would use for
/// this hop length - duplicated here (rather than shared) since it's a
/// one-line calculation not worth exposing across translation units for.
[[nodiscard]] std::uint32_t fftSizeFor(std::uint32_t hopLength) noexcept {
    return hopLength * 4;
}

}  // namespace

RgbImage toRgbImage(const StreamImage& image) {
    RgbImage rgb;
    rgb.width = image.frameCount;
    rgb.height = image.config.binCount;
    rgb.pixels.resize(rgb.pixelCount() * 3);

    const std::uint32_t binCount = image.config.binCount;
    const std::uint32_t frameCount = image.frameCount;

    for (std::uint32_t row = 0; row < binCount; ++row) {
        const std::uint32_t bin = binCount - 1 - row;  // row 0 = highest frequency
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = static_cast<std::size_t>(bin) * frameCount + frame;
            const std::size_t pixel = (static_cast<std::size_t>(row) * frameCount + frame) * 3;
            rgb.pixels[pixel + 0] = dbToByte(image.leftMagnitudeDb[cell]);
            rgb.pixels[pixel + 1] = dbToByte(image.rightMagnitudeDb[cell]);
            rgb.pixels[pixel + 2] = phaseToByte(image.sharedPhaseRadians[cell]);
        }
    }
    return rgb;
}

RgbImage toGrayscaleImage(const StreamImage& image) {
    RgbImage rgb;
    rgb.width = image.frameCount;
    rgb.height = image.config.binCount;
    rgb.pixels.resize(rgb.pixelCount() * 3);

    const std::uint32_t binCount = image.config.binCount;
    const std::uint32_t frameCount = image.frameCount;

    for (std::uint32_t row = 0; row < binCount; ++row) {
        const std::uint32_t bin = binCount - 1 - row;
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = static_cast<std::size_t>(bin) * frameCount + frame;
            const std::size_t pixel = (static_cast<std::size_t>(row) * frameCount + frame) * 3;
            const float averageDb = 0.5f * (image.leftMagnitudeDb[cell] + image.rightMagnitudeDb[cell]);
            const std::uint8_t value = dbToByte(averageDb);
            rgb.pixels[pixel + 0] = value;
            rgb.pixels[pixel + 1] = value;
            rgb.pixels[pixel + 2] = value;
        }
    }
    return rgb;
}

StreamImage fromRgbImage(const RgbImage& rgb, const StreamCodecConfig& configIn) {
    StreamImage image;
    image.config = configIn;
    image.config.binCount = rgb.height;
    image.frameCount = rgb.width;

    const std::uint32_t fftSize = fftSizeFor(image.config.hopLength);
    image.sampleCount = (image.frameCount == 0)
                             ? 0
                             : static_cast<std::uint64_t>(image.frameCount - 1) * image.config.hopLength + fftSize;

    const std::uint32_t binCount = image.config.binCount;
    const std::uint32_t frameCount = image.frameCount;
    image.leftMagnitudeDb.resize(std::size_t{binCount} * frameCount);
    image.rightMagnitudeDb.resize(std::size_t{binCount} * frameCount);
    image.sharedPhaseRadians.resize(std::size_t{binCount} * frameCount);

    for (std::uint32_t row = 0; row < binCount; ++row) {
        const std::uint32_t bin = binCount - 1 - row;
        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = static_cast<std::size_t>(bin) * frameCount + frame;
            const std::size_t pixel = (static_cast<std::size_t>(row) * frameCount + frame) * 3;
            image.leftMagnitudeDb[cell] = byteToDb(rgb.pixels[pixel + 0]);
            image.rightMagnitudeDb[cell] = byteToDb(rgb.pixels[pixel + 1]);
            image.sharedPhaseRadians[cell] = byteToPhase(rgb.pixels[pixel + 2]);
        }
    }
    return image;
}

}  // namespace sound_mind::codec
