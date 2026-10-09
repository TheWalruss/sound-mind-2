#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/core/filter_configuration.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedFilterPreset` within a Project -
/// see `ToolPresetId`'s own docs for the same pattern, applied here to a
/// whole `FilterConfiguration` instead of a `ToolConfiguration`.
using FilterPresetId = std::uint64_t;

/**
 * @brief A named, permanently-stored `FilterConfiguration` in a Project's
 *        own library - the Filter-layer counterpart to `NamedToolPreset`,
 *        confirmed with the user ("make filter configuration save-able to
 *        a filter preset, just like tool configurations are save-able to
 *        tool preset").
 *
 * Simpler than `NamedToolPreset`: `FilterConfiguration` is a single
 * concrete, plain-value class (a `FilterType` tag plus every type's own
 * parameters, not a polymorphic hierarchy), already copyable with no
 * `clone()` of its own - so this struct needs no hand-written copy/move
 * constructors the way `NamedToolPreset` does for its own
 * `unique_ptr<ToolConfiguration>`.
 */
struct NamedFilterPreset {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addFilterPreset()`, not meant to be picked by hand.
    FilterPresetId id = 0;

    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;

    /// @brief The saved configuration itself - a real copy, independent of
    ///        whichever Filter layer it was saved from; editing that
    ///        layer afterward never mutates this entry, and loading this
    ///        entry into a layer never creates a persistent link back to
    ///        it (the same "one-time copy, not a binding" contract
    ///        `NamedConvolutionKernel`'s own docs already establish).
    FilterConfiguration config;
};

/// @brief Serializes a named filter preset to its JSON representation.
void to_json(nlohmann::json& json, const NamedFilterPreset& namedFilterPreset);

/// @brief Parses a named filter preset from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedFilterPreset& namedFilterPreset);

}  // namespace sound_mind::core
