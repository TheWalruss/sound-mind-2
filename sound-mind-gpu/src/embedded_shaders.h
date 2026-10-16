#pragma once

#include <cstddef>
#include <vector>

namespace sound_mind::gpu::detail {

/// @brief One compiled compute shader built into the program (`v0.1.41.6`).
struct EmbeddedShader {
    /// @brief Its file name as dxc wrote it, e.g. "gaussian_blur.cso".
    const char* name;
    /// @brief The DXIL bytecode, root signature included.
    const unsigned char* data;
    /// @brief The bytecode's length in bytes.
    std::size_t size;
};

/// @brief Every compiled shader, from the source file
/// `cmake/embed_shaders.cmake` generates at build time - so an installed
/// copy needs no shader files and no build-machine path.
/// @return The shaders, in build order.
[[nodiscard]] const std::vector<EmbeddedShader>& embeddedShaders();

}  // namespace sound_mind::gpu::detail
