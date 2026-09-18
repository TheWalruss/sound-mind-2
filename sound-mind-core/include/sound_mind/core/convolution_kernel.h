#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedConvolutionKernel` within a
/// Project - see `MindShotId`'s own docs for the same pattern, applied
/// here instead to `docs/sound-mind-design.md`'s "Spectral shaping"
/// ("an arbitrary convolution kernel for custom spectral or temporal
/// responses").
using ConvolutionKernelId = std::uint64_t;

/**
 * @brief A named, permanently-stored convolution kernel in a Project's own
 *        library - `v0.Y.36.1` (Deferred Filters) Installment B, per
 *        `docs/sound-mind-design.md`'s "Spectral shaping".
 *
 * Confirmed with the user against the legacy Python Studio's own
 * `ConvolveDialog`/`custom_convolve_filter()` (`packages/sound_mind_studio/
 * src/sound_mind_studio/filters/{core,dialogs}.py`) - "full legacy parity":
 * a configurable, odd-sized square kernel, built-in presets, and named
 * save/load. Legacy persisted a kernel as a standalone `.npy` file under a
 * project's own `kernels/` folder; this instead reuses the same
 * "peer resource library on `Project`" pattern `NamedMindShot`/
 * `NamedMindGrain`/`NamedMindWave` already establish - a plain Core struct
 * serialized as part of the project JSON, not a second, kernel-specific
 * file format/folder convention.
 *
 * `FilterConfiguration`'s own `Convolve` type holds an independent, live,
 * editable copy of a kernel (`convolveKernel()`/`convolveKernelSize()`) -
 * loading a `NamedConvolutionKernel` from this library is a one-time copy
 * into that live copy, not a persistent binding (there is no "this Filter
 * layer is linked to kernel #3" relationship anywhere), so editing the
 * Filter layer's own kernel afterward never mutates the saved library
 * entry, and deleting a saved entry never affects a Filter layer that
 * once loaded it.
 */
struct NamedConvolutionKernel {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addConvolutionKernel()`, not meant to be picked by
    ///        hand.
    ConvolutionKernelId id = 0;
    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;
    /// @brief The kernel's own side length - always odd (matching legacy's
    ///        own `size | 1` forcing elsewhere in this codebase, e.g.
    ///        `EdgePreservingBlur`'s own `medianSize()`), so it has a
    ///        well-defined center cell.
    int size = 3;
    /// @brief The kernel's own coefficients, row-major, exactly `size *
    ///        size` entries. Bipolar - not clamped to any particular
    ///        range (a Sharpen-style kernel's own center coefficient can
    ///        exceed `1.0`, for instance).
    std::vector<float> coefficients{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    /// @brief Whether applying this kernel divides it by the sum of its
    ///        own positive coefficients first - see `applyFilter()`'s own
    ///        docs for why this matters (a pure-positive kernel would
    ///        otherwise brighten/darken the whole image by that sum).
    bool normalize = false;
};

/// @brief Serializes a named convolution kernel to its JSON representation.
void to_json(nlohmann::json& json, const NamedConvolutionKernel& namedKernel);

/// @brief Parses a named convolution kernel from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedConvolutionKernel& namedKernel);

}  // namespace sound_mind::core
