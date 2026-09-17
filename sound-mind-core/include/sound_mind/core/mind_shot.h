#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "sound_mind/core/paste_operation.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedMindShot` within a Project - see
/// `MindWaveId`'s own docs for the same pattern, applied here instead to
/// `docs/sound-mind-design.md`'s "Mind Shots".
using MindShotId = std::uint64_t;

/**
 * @brief A named, permanently-stored Mind Shot in a Project's own library -
 *        see `docs/sound-mind-design.md`'s "Mind Shots": "captures a
 *        selection from any layer at a point in time and stores it
 *        permanently... always paints back exactly as it was when
 *        captured, independent of later changes to its source."
 *
 * Deliberately reuses `Clip` (the same captured-content representation
 * Copy/Cut/Paste already use - see `Clip`'s own docs on why it's a plain
 * Core struct, not a `sound_mind::codec::StreamImage`) rather than a
 * second, Mind-Shot-specific image-patch format: capturing a Mind Shot
 * *is* architecturally the same operation Copy already performs
 * (`captureClip()` from a committed Selection), just stored permanently
 * and named in this project-scoped library instead of held anonymously,
 * transiently, on the clipboard.
 *
 * The "permanently, independent of later changes to its source" half of
 * the design doc's own description is what a `Clip` already guarantees on
 * its own - it's a captured snapshot, never a live reference back into
 * whatever layer it was captured from. Mind Grains (`docs/sound-mind-
 * design.md`'s own "Mind Grains", a later installment - see
 * `docs/sound-mind-roadmap.md`'s `v0.Y.33.1`) are the deliberate opposite:
 * a *live* reference re-drawn fresh from its source on every stamp,
 * which is exactly why they need their own, separate representation
 * rather than reusing this one.
 */
struct NamedMindShot {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addMindShot()`, not meant to be picked by hand.
    MindShotId id = 0;
    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;
    /// @brief The captured content itself - permanent once stored; never
    ///        mutated in place by anything in this codebase (a "re-
    ///        capture" would be a fresh `addMindShot()` call, not an edit
    ///        of an existing entry's own `clip`).
    Clip clip;
};

/// @brief Serializes a named Mind Shot to its JSON representation.
void to_json(nlohmann::json& json, const NamedMindShot& namedMindShot);

/// @brief Parses a named Mind Shot from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedMindShot& namedMindShot);

}  // namespace sound_mind::core
