#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sound_mind::core {

/**
 * @brief Every numeric, single-value parameter of a `FilterConfiguration`
 *        (`v0.1.48.10`) - the rows of filterParameterSpecs().
 *
 * Structured parameters (the tone curve, frequency gradient, convolution
 * kernel) and choices (filter type, Downsample mode, Convolve normalize)
 * aren't listed: they have their own accessors and editors.
 */
enum class FilterParameter : std::uint8_t {
    BlurSigma,
    MedianSize,
    DirectionalBlurLength,
    DirectionalBlurAngle,
    SharpenAmount,
    SpeckleDensity,
    SpeckleIntensity,
    SpeckleThreshold,
    NoiseFloor,
    Reduction,
    CrushAmount,
    GrainSize,
    GrainAmount,
    FeedbackAmount,
    FoldGain,
    ChannelBalance,
    ConvolveKernelSize,
    ConvolveAmount,
    DisplaceDistance,
    DisplaceAngle,
    ChannelCycleAngle,
    ReverbPreDelay,
    ReverbDecay,
    ReverbRoomSize,
    ReverbDiffusion,
    ReverbAbsorption,
    ReverbMix,
    DownsampleBlockSize,
};

/// @brief How many FilterParameter values there are.
inline constexpr std::size_t kFilterParameterCount = static_cast<std::size_t>(FilterParameter::DownsampleBlockSize) + 1;

/**
 * @brief What one FilterParameter is: where it's stored in JSON, whether a
 *        MindWave can drive it, and its valid range.
 *
 * The single place a parameter is described (`v0.1.48.10`, step 5 of the
 * 2026-10-22 code scan): `FilterConfiguration`'s JSON, its generic
 * accessors (FilterConfiguration::parameter()), MindWave resolution
 * (resolveFilterParameterMindWaves()) and the Studio's Filter
 * Configuration panel all read this table instead of listing every
 * parameter by hand. Adding a parameter means one row here (plus its
 * storage and its DSP).
 */
struct FilterParameterSpec {
    FilterParameter parameter;  ///< Which parameter.
    /// @brief Its value's key in a FilterConfiguration's JSON.
    const char* key;
    /// @brief Its MindWave binding's key in that JSON, or `nullptr` if no
    ///        MindWave can drive it. Ends in `MindWaveId`, the convention
    ///        `mindWaveDependents()` finds bindings by.
    const char* mindWaveKey;
    double minimum;  ///< The smallest valid value.
    double maximum;  ///< The largest valid value.
    bool integer;    ///< Whether it's a whole number (stored as `int`).
};

/// @brief Every FilterParameter's spec, in enum order.
/// @return `kFilterParameterCount` rows; row `i` describes parameter `i`.
[[nodiscard]] std::span<const FilterParameterSpec, kFilterParameterCount> filterParameterSpecs() noexcept;

/// @brief One FilterParameter's spec.
/// @param parameter The parameter.
/// @return Its row of filterParameterSpecs().
[[nodiscard]] const FilterParameterSpec& filterParameterSpec(FilterParameter parameter) noexcept;

/// @brief Whether a MindWave can drive `parameter`.
/// @param parameter The parameter.
/// @return `true` if its spec has a `mindWaveKey`.
[[nodiscard]] inline bool isMindWaveBindable(FilterParameter parameter) noexcept {
    return filterParameterSpec(parameter).mindWaveKey != nullptr;
}

}  // namespace sound_mind::core
