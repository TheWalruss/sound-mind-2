#include "sound_mind/codec/rgb_image_resample.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace sound_mind::codec {

RgbImage downsampleAveraged(const RgbImage& source, std::uint32_t newWidth, std::uint32_t newHeight) {
    RgbImage result;
    result.width = newWidth;
    result.height = newHeight;
    result.pixels.assign(std::size_t{newWidth} * newHeight * 3, 0);

    if (source.width == 0 || source.height == 0 || newWidth == 0 || newHeight == 0) {
        return result;
    }

    for (std::uint32_t outY = 0; outY < newHeight; ++outY) {
        // The source row range [srcYStart, srcYEnd) this output row averages
        // over - proportional to newHeight, so every source row contributes
        // to exactly one output row's own average (rounding aside), whether
        // shrinking or growing.
        const auto srcYStart =
            static_cast<std::uint32_t>((std::uint64_t{outY} * source.height) / newHeight);
        const auto srcYEnd = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(source.height, (std::uint64_t{outY + 1} * source.height + newHeight - 1) / newHeight));
        const std::uint32_t yEnd = srcYEnd > srcYStart ? srcYEnd : srcYStart + 1;

        for (std::uint32_t outX = 0; outX < newWidth; ++outX) {
            const auto srcXStart =
                static_cast<std::uint32_t>((std::uint64_t{outX} * source.width) / newWidth);
            const auto srcXEnd = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(source.width, (std::uint64_t{outX + 1} * source.width + newWidth - 1) / newWidth));
            const std::uint32_t xEnd = srcXEnd > srcXStart ? srcXEnd : srcXStart + 1;

            std::array<std::uint64_t, 3> sum{0, 0, 0};
            std::uint64_t count = 0;
            for (std::uint32_t sy = srcYStart; sy < yEnd && sy < source.height; ++sy) {
                for (std::uint32_t sx = srcXStart; sx < xEnd && sx < source.width; ++sx) {
                    const std::size_t srcIndex = (std::size_t{sy} * source.width + sx) * 3;
                    sum[0] += source.pixels[srcIndex];
                    sum[1] += source.pixels[srcIndex + 1];
                    sum[2] += source.pixels[srcIndex + 2];
                    ++count;
                }
            }

            const std::size_t dstIndex = (std::size_t{outY} * newWidth + outX) * 3;
            if (count > 0) {
                result.pixels[dstIndex] = static_cast<std::uint8_t>(sum[0] / count);
                result.pixels[dstIndex + 1] = static_cast<std::uint8_t>(sum[1] / count);
                result.pixels[dstIndex + 2] = static_cast<std::uint8_t>(sum[2] / count);
            }
        }
    }

    return result;
}

}  // namespace sound_mind::codec
