#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_wave.h"

namespace sound_mind::core {

class Project;

/**
 * @brief Whether `json` refers to MindWave `id` (`v0.1.47.4`): any object
 *        key, at any depth, whose name ends in `MindWaveId` or `MindWave`
 *        and whose value is the number `id`.
 *
 * Every MindWave binding in this codebase is serialized under such a key
 * (`opacityMindWaveId`, `blurSigmaMindWaveId`, `sizeMindWaveId`,
 * `vibratoMindWaveId`, ...), so a binding added later is found without
 * changing this function - the convention is the contract.
 *
 * @param json The JSON to search.
 * @param id The MindWave.
 * @return `true` if some binding in `json` refers to `id`.
 */
[[nodiscard]] bool jsonRefersToMindWave(const nlohmann::json& json, MindWaveId id);

/// @brief What depends on one MindWave - see mindWaveDependents().
struct MindWaveDependents {
    /// @brief Every layer whose output uses the MindWave, bottom to top:
    ///        through its own opacity binding, its filter settings, or the
    ///        bindings of an active operation painted on it.
    std::vector<LayerId> layers;
    /// @brief The subset whose active operations (strokes, filter-to-
    ///        selection, ...) use it - their painted content has to be
    ///        replayed, not just recomposited.
    std::vector<LayerId> paintedLayers;
    /// @brief The lowest stack position among `layers` - where a composite
    ///        cache has to be invalidated from (everything above it may
    ///        filter it). `std::nullopt` when nothing depends on it.
    std::optional<std::size_t> lowestStackIndex;
};

/**
 * @brief Finds what a change to MindWave `id` affects (`v0.1.47.4`) - so a
 *        MindWave edit re-renders only that, instead of every layer.
 *
 * Looks at each layer's JSON (opacity binding, filter configuration - but
 * not its base recipe: a generated layer's content is baked when it's
 * generated) and at the bindings of each layer's *active* operations. A
 * paste's clip data is skipped (pastes have no bindings).
 *
 * @param project The project.
 * @param id The MindWave that changed (or was removed).
 * @return Its dependents.
 * @note Thread-safety: reads `project` only; UI thread in practice.
 */
[[nodiscard]] MindWaveDependents mindWaveDependents(const Project& project, MindWaveId id);

}  // namespace sound_mind::core
