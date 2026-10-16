#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/generator_configuration.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief Synthesizes a project-canvas-sized `StreamImage` from scratch,
 *        per `config`'s own family/order-chaos/seed - `docs/sound-mind-
 *        design.md`'s "Generators", `v0.Y.51.1`.
 *
 * The result is an ordinary `StreamImage`, exactly like any imported or
 * painted layer's own content - fully paintable and filterable
 * afterward, and composited/blended the same as any other layer's own
 * source (see the design doc's own framing). Studio-side, `LayerController::
 * addGeneratedLayer()` is what actually wraps this into a new `Layer`.
 *
 * @param config What to generate - see `GeneratorConfiguration`'s own
 *        docs. `GeneratorFamily::Fractal`/`GeneratorFamily::Streaming`
 *        aren't implemented yet - both return silent content, sized
 *        correctly, the same "groundwork, not yet functional" state
 *        `GeneratorFamily`'s own docs describe.
 * @param settings The project's own settings - determines the result's
 *        own canvas dimensions (`canvasWidth`/`binCount`) and frequency
 *        range, the same as any other freshly-encoded layer content.
 * @param resolveMindWave Resolves the MindWave ids a `Noise` run is bound
 *        to (see `NoiseConfiguration`); an empty resolver, or an id that
 *        doesn't resolve, leaves that parameter unbound.
 * @return The generated content, deterministic from `config.seed` (see
 *         `GeneratorConfiguration::seed`'s own docs on the exact
 *         reproducibility guarantee).
 */
[[nodiscard]] sound_mind::codec::StreamImage generateContent(const GeneratorConfiguration& config,
                                                               const ProjectSettings& settings,
                                                               const MindWaveResolver& resolveMindWave = {});

/**
 * @brief One sample, in `[0, 1]`, of seeded noise whose spectrum follows
 *        `f^alpha` along a sequence of integer steps `n` - the noise core of
 *        a noise MindWave (Pre-Beta Plan `v0.Y.79.1`).
 *
 * `alpha = 0` is white (independent steps). Negative `alpha` (pink -1, red
 * -2) sums octave generators held over `2^k` steps, weighted
 * `2^(k*(-alpha-1)/2)` - the Voss-McCartney construction, generalized -
 * so neighboring steps are increasingly correlated. Positive `alpha` (up to
 * purple, +2) blends toward the first difference of white noise, so
 * neighbors are anti-correlated. Mean `0.5` throughout; deterministic and
 * order-independent.
 *
 * @param seed The run's seed.
 * @param row Separates independent sequences (e.g. one per row of cells).
 * @param n The step.
 * @param alpha The spectral exponent, typically in `[-2, 2]`.
 * @return The sample.
 */
[[nodiscard]] double coloredNoiseSample(std::uint64_t seed, std::int64_t row, std::int64_t n, double alpha) noexcept;

/**
 * @brief A seeded, order-independent random value in `[0, 1)` for one
 *        integer coordinate - the shared core of every noise generator
 *        (layer noise here, noise MindWaves), so each cell can be computed
 *        on its own and still replay bit-for-bit.
 * @param seed The run's seed.
 * @param stream Separates independent draws at the same coordinate (e.g.
 *        amplitude vs phase, left vs right).
 * @param x The first coordinate.
 * @param y The second coordinate.
 * @return The value.
 */
[[nodiscard]] double hashedUnitNoise(std::uint64_t seed, std::uint64_t stream, std::int64_t x,
                                     std::int64_t y) noexcept;

}  // namespace sound_mind::core
