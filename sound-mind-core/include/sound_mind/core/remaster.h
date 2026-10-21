#pragma once

#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/history.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief The settings a Remaster replays at (`v0.1.43.4`): `wanted`'s
 *        codec settings with `original`'s duration - the canvas width is
 *        rescaled so the project stays as long as it was when the time step
 *        changes.
 * @param original The project's settings.
 * @param wanted The settings asked for; its `canvasWidth` is ignored.
 * @return The settings to replay at.
 */
[[nodiscard]] ProjectSettings remasterSettings(const ProjectSettings& original, const ProjectSettings& wanted);

/**
 * @brief Resamples a pasted clip from one codec grid to another (nearest
 *        cell), keeping it at the same seconds and Hz - paste writes a clip
 *        cell for cell, so a clip recorded at one resolution must be
 *        brought to the new one (`v0.1.43.4`).
 * @param clip The clip, at `from`'s resolution.
 * @param placement Where it was placed.
 * @param from The grid it was made at.
 * @param to The grid to bring it to.
 * @return The clip at `to`'s resolution; unchanged if the grids match.
 */
[[nodiscard]] Clip resampleClip(const Clip& clip, const TimeFrequencyRect& placement,
                                const sound_mind::codec::StreamCodecConfig& from,
                                const sound_mind::codec::StreamCodecConfig& to);

/**
 * @brief Resamples a clip with no absolute placement (a Mind Shot's) -
 *        time by the ratio of frame durations, frequency by the ratio of
 *        bin counts. Exact for a time-step or sample-rate change; only
 *        approximate when the frequency range or scale changes, since a
 *        Mind Shot doesn't record which frequencies it spans.
 * @param clip The clip.
 * @param from The grid it was made at.
 * @param to The grid to bring it to.
 * @return The resampled clip; unchanged if the grids match.
 */
[[nodiscard]] Clip resampleClipProportionally(const Clip& clip, const sound_mind::codec::StreamCodecConfig& from,
                                              const sound_mind::codec::StreamCodecConfig& to);

/**
 * @brief Resamples a raster (mask) selection boundary's JSON to another
 *        grid; a path boundary (seconds and Hz) is left as it is.
 * @param boundary `to_json(SelectionRegion)`'s shape.
 * @param from The grid it was made at.
 * @param to The grid to bring it to.
 * @return The adapted JSON.
 */
[[nodiscard]] nlohmann::json resampleSelectionJson(const nlohmann::json& boundary,
                                                   const sound_mind::codec::StreamCodecConfig& from,
                                                   const sound_mind::codec::StreamCodecConfig& to);

/**
 * @brief Adapts one History entry recorded at `from` to replay at `to`
 *        (`v0.1.43.4`) - Remaster's `ReplayOptions::transformEntry`.
 *
 * Everything else in an entry is already in seconds and Hz. This converts
 * what isn't:
 * - each layer's `translationColumns` (by the ratio of frame durations);
 * - pasted clips (resampleClip(); a text paste's clip too - the Studio
 *   re-renders text from its `TextSource` afterwards where it can);
 * - mask boundaries of fills, filters and pastes (resampleSelectionJson());
 * - Mind Shot clips in the resource library (resampleClipProportionally()).
 *
 * @param entry The entry, changed in place.
 * @param from The settings it was recorded at.
 * @param to The settings it's replayed at.
 */
void adaptEntryForRemaster(HistoryEntry& entry, const ProjectSettings& from, const ProjectSettings& to);

}  // namespace sound_mind::core
