#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include <QString>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_sources.h"

namespace sound_mind::studio {

/// @brief Where a source's file is - the project's own, or (Remaster) a
///        replacement the user picked.
using SourcePathLookup = std::function<std::optional<std::filesystem::path>(sound_mind::core::SourceId)>;

/// @brief A layer's rendered content as it stands at the point of replay -
///        what pooling, phase cleanup and duplicating started from.
using RenderedContentLookup = std::function<const sound_mind::codec::StreamImage*(sound_mind::core::LayerId)>;

/**
 * @brief Remakes a layer's base content from its recipe (`v0.1.43.2`) -
 *        the step that lets a project's History be replayed from its
 *        sources (Remaster, Macro rendering, recovering a base no longer in
 *        memory). See `Layer::setBaseContent()` for what recipes are.
 *
 * Recipe kinds:
 * - `"empty"`: silence at the project's size.
 * - `"importAudio"` (`source`, `offsetSeconds`, `snippetIndex`): the
 *   snippet, encoded exactly as importing it does.
 * - `"importImage"` (`source`, `mode`, optional `polar`): encoded exactly
 *   as importing it does.
 * - `"generate"` (`configuration`): the generator, re-run from its seed.
 * - `"record"`, `"loopTake"` (`source`, a FLAC): the audio, encoded.
 * - `"layerImage"` (`source`, a Stream file): the content itself -
 *   decoded and re-encoded if the project's codec settings differ.
 * - `"duplicate"` (`sourceLayer`): that layer's rendered content.
 * - `"pool"`: this layer's rendered content, pooled.
 * - `"phaseCleanup"`: this layer's rendered content, cleaned up.
 *
 * Bit-for-bit only where the original was: FLAC-stored recordings come
 * back through a lossless file, but float samples are quantized on the way;
 * the rest re-runs the same code on the same inputs.
 *
 * Thread-safe as long as the lookups are and `project` isn't modified
 * meanwhile (a replay runs on its own project copy).
 *
 * @param recipe The recipe.
 * @param layer The layer whose base it makes.
 * @param project The project as it stood just before the base was made
 *        (its settings and MindWaves are used).
 * @param sourcePath Finds each source's file.
 * @param renderedContent Finds a layer's rendered content.
 * @param errorMessage Receives the reason on failure; may be `nullptr`.
 * @return The base, or `std::nullopt` if the recipe is missing, unknown, or
 *         its source can't be read.
 */
[[nodiscard]] std::optional<sound_mind::core::LayerBase> runBaseRecipe(const nlohmann::json& recipe,
                                                                      sound_mind::core::LayerId layer,
                                                                      const sound_mind::core::Project& project,
                                                                      const SourcePathLookup& sourcePath,
                                                                      const RenderedContentLookup& renderedContent,
                                                                      QString* errorMessage = nullptr);

}  // namespace sound_mind::studio
