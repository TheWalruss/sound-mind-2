#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedToolPreset` within a Project - see
/// `MindShotId`'s own docs for the same pattern, applied here instead to
/// `docs/sound-mind-design.md`'s "Tool Configuration" ("a Tool Preset
/// drop-down listing every tool configuration saved in the current
/// project").
using ToolPresetId = std::uint64_t;

/**
 * @brief A named, permanently-stored `ToolConfiguration` in a Project's own
 *        library - `docs/sound-mind-design.md`'s "Tool Configuration": "a
 *        Tool Preset drop-down listing every tool configuration saved in
 *        the current project. Selecting a preset loads all of its
 *        parameters into the Panel at once."
 *
 * A real, embedded `ToolConfiguration` clone, not a reference into some
 * other owner - the same "permanent snapshot, independent of whatever
 * created it" shape `NamedMindShot::clip`'s own `Clip` already is, just for
 * a polymorphic type instead of a plain value one. This is the "later
 * installment of this same milestone" `ToolConfiguration`'s own class docs
 * anticipated back when Tool Configuration itself was first built - a
 * project has nowhere to save one until now.
 *
 * Deliberately copyable (unlike most `unique_ptr<ToolConfiguration>`
 * owners in this codebase, e.g. `PaintOperation`, which only ever needs a
 * fresh clone via `translatedCopy()`, never a real copy constructor) -
 * `Project` stores its whole library as a plain `std::vector<NamedToolPreset>`,
 * the same "peer resource library" value-type shape `NamedMindShot`/
 * `NamedMindWave`/`NamedMindGrain` already establish, so this type needs
 * genuine copy semantics to fit that shape rather than a bespoke
 * move-only container.
 */
struct NamedToolPreset {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addToolPreset()`, not meant to be picked by hand.
    ToolPresetId id = 0;
    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;
    /// @brief The saved configuration itself - never `nullptr` for an
    ///        entry actually stored in a Project's own library (only a
    ///        default-constructed, not-yet-added `NamedToolPreset` has
    ///        one); never mutated in place by anything in this codebase
    ///        (re-saving a preset under the same name replaces this
    ///        pointer wholesale, via `Project::addToolPreset()` again -
    ///        see its own docs - rather than editing this instance).
    std::unique_ptr<ToolConfiguration> config;

    /// @brief Default-constructs an empty (as-yet-unsaved) entry - `config`
    ///        is `nullptr` until explicitly set.
    NamedToolPreset() = default;

    /// @brief Deep-copies `other`'s own `config` via `ToolConfiguration::clone()`
    ///        - see this struct's own docs on why real copy semantics are
    ///        needed here, unlike most other `unique_ptr<ToolConfiguration>`
    ///        owners in this codebase.
    /// @param other The entry to copy.
    NamedToolPreset(const NamedToolPreset& other)
        : id(other.id), name(other.name), config(other.config ? other.config->clone() : nullptr) {}

    /// @brief Deep-copies `other`'s own `config` via `ToolConfiguration::clone()`
    ///        - see this struct's own docs on why real copy semantics are
    ///        needed here, unlike most other `unique_ptr<ToolConfiguration>`
    ///        owners in this codebase.
    /// @param other The entry to copy.
    /// @return `*this`, now an independent deep copy of `other`.
    NamedToolPreset& operator=(const NamedToolPreset& other) {
        if (this != &other) {
            id = other.id;
            name = other.name;
            config = other.config ? other.config->clone() : nullptr;
        }
        return *this;
    }

    /// @brief Move-constructs, transferring ownership of `config` directly
    ///        rather than cloning it.
    NamedToolPreset(NamedToolPreset&&) noexcept = default;

    /// @brief Move-assigns, transferring ownership of `config` directly
    ///        rather than cloning it.
    /// @return `*this`.
    NamedToolPreset& operator=(NamedToolPreset&&) noexcept = default;
};

/// @brief Serializes a named tool preset to its JSON representation.
/// @throws std::invalid_argument if `namedToolPreset.config` is `nullptr`
///         (see `to_json(nlohmann::json&, const ToolConfiguration&)`'s own
///         docs - this just delegates to it).
void to_json(nlohmann::json& json, const NamedToolPreset& namedToolPreset);

/// @brief Parses a named tool preset from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
/// @throws std::invalid_argument if `config`'s own `"type"` field names an
///         unrecognized `ToolType` - see `toolConfigurationFromJson()`'s
///         own docs.
void from_json(const nlohmann::json& json, NamedToolPreset& namedToolPreset);

}  // namespace sound_mind::core
