#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

/**
 * @brief Paints a straight, diagonal 3-second stroke from 3 kHz to
 *        5 kHz using `config`, into a freshly-built, silence-floor
 *        `StreamImage` - the actual sound an audio preview of `config`
 *        would make, ready for `sound_mind::codec::decode()`. Not
 *        cropped or rasterized for display - callers that also want a
 *        visual raster do that themselves from the result (see
 *        `sound-mind-studio`'s own Resource Browser inspector).
 *
 * Shared by every "synthesize a representative stroke to preview this"
 * call site: a real Tool Preset's/the Tool Configuration panel's own
 * live configuration, played as-is; or a synthetic one built just to
 * preview a MindWave (a default `InstrumentConfiguration` with it bound
 * as vibrato) or a Resonance Profile (a default `ResonanceConfiguration`
 * carrying its spectrum) standalone, before either has ever been
 * attached to a real Tool Preset. Direct user feedback: "practically
 * wherever there is a visual preview of something, give the user the
 * ability to play an audio preview of whatever it is."
 *
 * A `NamedToolPreset` carries no `Path`/gradient of its own to reuse
 * (see that struct's own docs on why - a tool configuration is reusable
 * across strokes precisely because it isn't tied to one), so this picks
 * a fixed, fully-opaque default gradient deliberately, rather than
 * trying to guess one.
 *
 * @param config The tool configuration to preview.
 * @param project Supplies the codec config/frequency-to-time scale to
 *        paint with, and resolves any `MindWaveId` `config` itself
 *        binds to (vibrato/tremolo) against its own MindWave library.
 * @return A `StreamImage` holding the painted preview stroke, ready to
 *         decode or rasterize.
 */
[[nodiscard]] sound_mind::codec::StreamImage toolConfigurationPreviewStreamImage(const ToolConfiguration& config,
                                                                                   const Project& project);

}  // namespace sound_mind::core
