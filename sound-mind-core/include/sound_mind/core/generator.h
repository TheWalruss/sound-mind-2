#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/generator_configuration.h"
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
 * @return The generated content, deterministic from `config.seed` (see
 *         `GeneratorConfiguration::seed`'s own docs on the exact
 *         reproducibility guarantee).
 */
[[nodiscard]] sound_mind::codec::StreamImage generateContent(const GeneratorConfiguration& config,
                                                               const ProjectSettings& settings);

}  // namespace sound_mind::core
