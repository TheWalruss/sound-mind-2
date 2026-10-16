#pragma once

#include <cstdint>
#include <optional>

#include <nlohmann/json.hpp>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/mind_wave.h"

namespace sound_mind::core {

/**
 * @brief Which family of algorithm a generator uses to seed spectral
 *        material - `docs/sound-mind-design.md`'s "Generators",
 *        `v0.Y.51.1`.
 *
 * **As of this milestone's own first installment, only `Lattice` is
 * actually functional** - `Fractal`/`Streaming` exist here so
 * `GeneratorConfiguration`/the Studio's own Generate dialog already have
 * a complete, forward-looking shape, but `generateContent()` returns
 * silent content for either, the same "groundwork, not yet functional"
 * state `ToolConfiguration`'s own `Clone` tool type already establishes
 * as this codebase's precedent for exactly this situation.
 */
enum class GeneratorFamily {
    /// @brief A grid of coupled cells evolving under simple local rules -
    ///        an organic, self-similar texture across the whole canvas.
    Lattice,
    /// @brief A branching grammar (the same kind behind fractal MindWave
    ///        functions) grown directly into amplitude and phase - not
    ///        yet implemented.
    Fractal,
    /// @brief A genuinely time-varying signal, evolving continuously
    ///        across the full duration of the layer - not yet
    ///        implemented.
    Streaming,
    /// @brief Colored noise - see `NoiseConfiguration` (Pre-Beta Plan
    ///        `v0.Y.79.1`). Unlike the three families above it isn't placed
    ///        on the order/chaos axis: noise is all chaos by definition.
    Noise,
};

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

/**
 * @brief How `GeneratorFamily::Noise` fills a layer (Pre-Beta Plan
 *        `v0.Y.79.1`).
 *
 * Every noise cell draws a fresh random amplitude (Rayleigh-distributed,
 * as an STFT of real noise is) and phase, deterministic from the seed, on
 * top of the color's spectral shape. A noise cell can span several canvas
 * cells - `horizontalResolution` frames by `verticalResolution` bins - for
 * coarser, blockier noise.
 *
 * **MindWave bindings** follow the convention every other binding in the
 * Studio uses: the bound MindWave's value (`[0, 1]`) at each canvas cell
 * blends from the parameter's "no effect" baseline at `0` to the set value
 * at `1`. For the resolutions the baseline is one cell (finest); for the
 * custom spectrum it is silence (`kNoiseSilenceDb`).
 */
struct NoiseConfiguration {
    /// @brief The noise color.
    NoiseColor color = NoiseColor::Pink;
    /// @brief The level a named color reads at 1 kHz on the canvas, dBFS
    ///        (the whole flat level for `Gray`). Unused for `Custom`.
    float levelDb = -30.0f;
    /// @brief How many frames (time columns) one noise cell spans; `>= 1`.
    double horizontalResolution = 1.0;
    /// @brief How many bins (frequency rows) one noise cell spans; `>= 1`.
    double verticalResolution = 1.0;
    /// @brief Varies `horizontalResolution` across the canvas, if set.
    std::optional<MindWaveId> horizontalResolutionMindWave;
    /// @brief Varies `verticalResolution` across the canvas, if set.
    std::optional<MindWaveId> verticalResolutionMindWave;
    /// @brief `Custom`'s spectrum: its left/right intensities, along the
    ///        frequency axis (`t = 0` lowest bin, `t = 1` highest), are the
    ///        levels each channel reads at. Opacity is ignored.
    Gradient customSpectrum;
    /// @brief Varies `Custom`'s level per cell, if set - see this struct's
    ///        docs - so the spectrum can change over time too.
    std::optional<MindWaveId> customSpectrumMindWave;
};

/// @brief The floor a noise cell's level never goes below, dBFS - the same
///        silence floor the rest of the Studio uses.
inline constexpr float kNoiseSilenceDb = -96.0f;

/**
 * @brief The full, self-contained description of one generator run -
 *        `docs/sound-mind-design.md`'s "Generators".
 *
 * Deliberately not persisted/serialized anywhere (unlike
 * `FilterConfiguration`'s own "pending" seed value) - generating a layer
 * is a one-shot action, not an ongoing configuration something else keeps
 * editing afterward; once `generateContent()` has produced a layer's own
 * content, this struct's own job is done.
 */
struct GeneratorConfiguration {
    /// @brief Which algorithm family to run.
    GeneratorFamily family = GeneratorFamily::Lattice;

    /// @brief Where this run sits on the shared order/chaos continuum -
    ///        the same `[-1, 1]` convention (negative for chaos, positive
    ///        for order, `0` for the "rich, complex-but-coherent" middle
    ///        the design doc calls out) `OrderChaosConfiguration::
    ///        amount()` already establishes for the Order/Chaos brush,
    ///        reused verbatim rather than inventing a second convention
    ///        for what the design doc itself describes as "the same
    ///        order/chaos control."
    double orderChaos = 0.0;

    /// @brief The seed this run is deterministic from - replaying the
    ///        exact same `GeneratorConfiguration` against the exact same
    ///        `ProjectSettings` reproduces the exact same content,
    ///        bit-for-bit, on the same Sound Mind Studio version and
    ///        target architecture (a perceptual match only, not a
    ///        bit-identical one, across Arm64 versus x64) - see the
    ///        design doc's own "Generators" section.
    std::uint64_t seed = 0;

    /// @brief `GeneratorFamily::Noise`'s own settings; ignored otherwise.
    NoiseConfiguration noise;
};

}  // namespace sound_mind::core
