#include "sound_mind/codec/polar_projection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace sound_mind::codec {

namespace {

constexpr double kTwoPi = 6.283185307179586;

/// @brief Bilinearly samples `source` at fractional pixel `(x, y)`,
/// already clamped by the caller to `[0, width-1]`/`[0, height-1]`.
[[nodiscard]] std::array<double, 3> bilinearSample(const RgbImage& source, double x, double y) {
    const auto x0 = static_cast<std::uint32_t>(x);
    const auto y0 = static_cast<std::uint32_t>(y);
    const std::uint32_t x1 = std::min(x0 + 1, source.width - 1);
    const std::uint32_t y1 = std::min(y0 + 1, source.height - 1);
    const double xf = x - x0;
    const double yf = y - y0;

    const auto pixelAt = [&](std::uint32_t px, std::uint32_t py) {
        const std::size_t index = (std::size_t{py} * source.width + px) * 3;
        return std::array<double, 3>{static_cast<double>(source.pixels[index]),
                                      static_cast<double>(source.pixels[index + 1]),
                                      static_cast<double>(source.pixels[index + 2])};
    };
    const auto a = pixelAt(x0, y0);
    const auto b = pixelAt(x1, y0);
    const auto c = pixelAt(x0, y1);
    const auto d = pixelAt(x1, y1);

    std::array<double, 3> result{};
    for (std::size_t channel = 0; channel < 3; ++channel) {
        result[channel] = a[channel] * (1.0 - xf) * (1.0 - yf) + b[channel] * xf * (1.0 - yf) +
                           c[channel] * (1.0 - xf) * yf + d[channel] * xf * yf;
    }
    return result;
}

}  // namespace

RgbImage rectToPolar(const RgbImage& source, std::uint32_t diameter) {
    RgbImage result;
    result.width = diameter;
    result.height = diameter;
    result.pixels.assign(std::size_t{diameter} * diameter * 3, 0);

    if (source.width == 0 || source.height == 0 || diameter == 0) {
        return result;
    }

    const double half = static_cast<double>(diameter) / 2.0;
    const double sourceWidth = static_cast<double>(source.width);
    const double sourceHeight = static_cast<double>(source.height);

    for (std::uint32_t py = 0; py < diameter; ++py) {
        const double dy = static_cast<double>(py) - half + 0.5;
        for (std::uint32_t px = 0; px < diameter; ++px) {
            const double dx = static_cast<double>(px) - half + 0.5;
            const double r = std::sqrt(dx * dx + dy * dy);
            if (r > half) {
                continue;  // Outside the disk - stays black, see this function's own docs.
            }

            // theta = 0 at twelve o'clock, increasing clockwise - see this
            // function's own docs on the convention.
            double theta = std::atan2(dx, -dy);
            if (theta < 0.0) {
                theta += kTwoPi;
            }

            const double xSrc = std::clamp(sourceWidth * theta / kTwoPi, 0.0, sourceWidth - 1.0);
            const double ySrc = std::clamp(sourceHeight * (1.0 - r / half), 0.0, sourceHeight - 1.0);

            const auto sampled = bilinearSample(source, xSrc, ySrc);
            const std::size_t dstIndex = (std::size_t{py} * diameter + px) * 3;
            result.pixels[dstIndex] = static_cast<std::uint8_t>(std::clamp(sampled[0], 0.0, 255.0));
            result.pixels[dstIndex + 1] = static_cast<std::uint8_t>(std::clamp(sampled[1], 0.0, 255.0));
            result.pixels[dstIndex + 2] = static_cast<std::uint8_t>(std::clamp(sampled[2], 0.0, 255.0));
        }
    }

    return result;
}

RgbImage polarToRect(const RgbImage& source, double originX, double originY, double maxRadius,
                      double arcStartRadians, double arcEndRadians, std::uint32_t outputWidth,
                      std::uint32_t outputHeight) {
    RgbImage result;
    result.width = outputWidth;
    result.height = outputHeight;
    result.pixels.assign(std::size_t{outputWidth} * outputHeight * 3, 0);

    if (source.width == 0 || source.height == 0 || outputWidth == 0 || outputHeight == 0) {
        return result;
    }

    // Equal (within a small epsilon) means a full circle - see this
    // function's own docs. Otherwise, the arc spans clockwise from start
    // to end, wrapping through zero if end < start (matching normal
    // clock-face arithmetic).
    constexpr double kEpsilon = 1e-9;
    double arcSpan = std::fmod(arcEndRadians - arcStartRadians + kTwoPi, kTwoPi);
    if (std::abs(arcSpan) < kEpsilon) {
        arcSpan = kTwoPi;
    }

    const double sourceWidth = static_cast<double>(source.width);
    const double sourceHeight = static_cast<double>(source.height);
    const double widthDivisor = outputWidth > 1 ? static_cast<double>(outputWidth - 1) : 1.0;
    const double heightDivisor = outputHeight > 1 ? static_cast<double>(outputHeight - 1) : 1.0;

    for (std::uint32_t outputRow = 0; outputRow < outputHeight; ++outputRow) {
        // outputRow=0 (top) samples r=maxRadius (the outer ring, the
        // highest encoded frequency); outputRow=outputHeight-1 (bottom)
        // samples r=0 (the centre, the lowest) - see this function's own
        // docs.
        const double r = maxRadius * (1.0 - static_cast<double>(outputRow) / heightDivisor);
        for (std::uint32_t outputColumn = 0; outputColumn < outputWidth; ++outputColumn) {
            const double theta = arcStartRadians + arcSpan * static_cast<double>(outputColumn) / widthDivisor;

            // sin/cos, not cos/sin - theta=0 (twelve o'clock) points
            // straight up (negative y), matching rectToPolar()'s own
            // inverse convention exactly.
            const double xSrc = originX + r * std::sin(theta);
            const double ySrc = originY - r * std::cos(theta);

            const bool outOfBounds =
                xSrc < 0.0 || xSrc > sourceWidth - 1.0 || ySrc < 0.0 || ySrc > sourceHeight - 1.0;
            if (outOfBounds) {
                continue;  // stays black - see this function's own docs.
            }

            const auto sampled = bilinearSample(source, xSrc, ySrc);
            const std::size_t dstIndex = (std::size_t{outputRow} * outputWidth + outputColumn) * 3;
            result.pixels[dstIndex] = static_cast<std::uint8_t>(std::clamp(sampled[0], 0.0, 255.0));
            result.pixels[dstIndex + 1] = static_cast<std::uint8_t>(std::clamp(sampled[1], 0.0, 255.0));
            result.pixels[dstIndex + 2] = static_cast<std::uint8_t>(std::clamp(sampled[2], 0.0, 255.0));
        }
    }

    return result;
}

}  // namespace sound_mind::codec
