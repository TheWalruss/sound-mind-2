#pragma once

#include <cstdint>

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
};

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
};

}  // namespace sound_mind::core
