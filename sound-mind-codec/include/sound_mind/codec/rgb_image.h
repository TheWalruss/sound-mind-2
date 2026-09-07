#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sound_mind::codec {

/**
 * @brief A simple in-memory RGB image: `width` x `height` pixels, 3 bytes
 *        each (interleaved R, G, B), row-major, top row first.
 *
 * Deliberately framework-agnostic (no Qt types) - per
 * `docs/sound-mind-architecture.md`'s Build & Module Layout, `codec` has no
 * GUI dependency. `sound-mind-studio` is what converts one of these to/from
 * a `QImage` for actual on-screen display or file loading.
 */
struct RgbImage {
    /// @brief Image width, in pixels.
    std::uint32_t width = 0;

    /// @brief Image height, in pixels.
    std::uint32_t height = 0;

    /// @brief Pixel data, size `width * height * 3`.
    std::vector<std::uint8_t> pixels;

    /// @brief Number of pixels (not bytes).
    /// @return `width * height`.
    [[nodiscard]] std::size_t pixelCount() const noexcept { return std::size_t{width} * height; }
};

}  // namespace sound_mind::codec
