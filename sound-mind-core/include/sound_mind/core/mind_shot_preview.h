#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/paste_operation.h"

namespace sound_mind::core {

/**
 * @brief Builds a standalone, decodable `StreamImage` from a captured
 *        `Clip`, for inspecting a Mind Shot (or any other `Clip`) in
 *        isolation - not blitted onto any destination layer.
 *
 * Every existing `Clip` consumer in this codebase (`blitClipCentered()`,
 * `applyMindShotPaintOperation()`/`applyMindGrainPaintOperation()`) only
 * ever blits a clip's own cells onto an *already-sized* destination
 * `StreamImage` that supplies its own `config`/`frameCount` - there was no
 * existing path to decode or render a `Clip` entirely on its own before
 * this. `Clip` itself carries no `StreamCodecConfig` (see `Clip`'s own
 * docs on why: "it is always captured from, and pasted back into, a layer
 * using the current project's own config") - so a caller must supply one,
 * same as every other `Clip` consumer implicitly does via its destination
 * layer.
 *
 * @param clip The captured content to wrap.
 * @param config The codec config to decode/render it with - typically
 *        `streamCodecConfigFor(project.settings())` for whichever project
 *        `clip` was captured in (its own, or another project's, when
 *        browsing one read-only). Its own `binCount` is overridden with
 *        `clip.binCount` (the two must match for the cell data to mean
 *        anything; `clip` is the authority on its own shape).
 * @return A `StreamImage` ready for `sound_mind::codec::decode()` or
 *         `sound_mind::codec::toRgbImage()` - `sampleCount` is set to
 *         `clip.frameCount * config.hopLength` (the whole-hop length; the
 *         exact original sample count isn't preserved by a `Clip`, so this
 *         is a reasonable approximation for a preview, not an exact
 *         round-trip).
 */
[[nodiscard]] sound_mind::codec::StreamImage streamImageFromClip(const Clip& clip,
                                                                   const sound_mind::codec::StreamCodecConfig& config);

}  // namespace sound_mind::core
