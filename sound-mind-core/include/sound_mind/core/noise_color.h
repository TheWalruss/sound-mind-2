#pragma once

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/**
 * @brief The color of generated noise - how its energy is spread across
 *        frequency (Pre-Beta Plan `v0.Y.79.1`).
 *
 * Each named color is a power law, power spectral density proportional to
 * `f^alpha` - see `noiseColorExponent()`. `Gray` is equal-loudness noise
 * (shaped by inverse A-weighting), so it reads flat on the A-weighted
 * canvas. `Custom` takes its shape from a gradient or a MindWave instead
 * (see `NoiseConfiguration::customSpectrum`).
 */
enum class NoiseColor {
    White,   ///< Equal power per Hz (alpha 0).
    Pink,    ///< Equal power per octave (alpha -1) - "natural" sounding.
    Red,     ///< Red / Brownian: steeper toward the bass (alpha -2).
    Purple,  ///< Purple / violet: rising toward the treble (alpha +2).
    Gray,    ///< Equal perceived loudness at every frequency.
    Custom,  ///< A hand-shaped spectrum - see `NoiseConfiguration`.
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(NoiseColor, {
    {NoiseColor::White, "white"},
    {NoiseColor::Pink, "pink"},
    {NoiseColor::Red, "red"},
    {NoiseColor::Purple, "purple"},
    {NoiseColor::Gray, "gray"},
    {NoiseColor::Custom, "custom"},
})
// clang-format on

/// @brief A named noise color's power-law exponent: power spectral density
///        is proportional to `f^alpha`.
/// @param color The color.
/// @return `alpha`; `0` for `Gray`/`Custom`, which aren't power laws.
[[nodiscard]] constexpr double noiseColorExponent(NoiseColor color) noexcept {
    switch (color) {
        case NoiseColor::Pink:
            return -1.0;
        case NoiseColor::Red:
            return -2.0;
        case NoiseColor::Purple:
            return 2.0;
        default:
            return 0.0;
    }
}

}  // namespace sound_mind::core
