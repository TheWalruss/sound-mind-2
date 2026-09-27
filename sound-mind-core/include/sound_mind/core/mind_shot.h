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
    /**
     * @brief The real-world pitch `clip`'s own captured content was
     *        recorded/painted at, in Hz - `docs/sound-mind-roadmap.md`'s
     *        `v0.Y.55.1` (MIDI track import) own stated prerequisite,
     *        needed so a MIDI note played back through this Mind Shot can
     *        be pitch-shifted to its own correct target pitch (see
     *        `MindShotConfiguration::fundamentalFrequencyHz()`'s own docs
     *        for the actual shift this drives).
     *
     * `0.0` (the default - every Mind Shot captured before this milestone
     * existed) means "not set": no pitch-shifting is ever applied for such
     * an entry, the same "meaningless until configured" convention every
     * other optional binding in this codebase already follows. A real,
     * positive value is never inferred automatically from the capture
     * itself (there is no reliable way to detect "the" pitch of an
     * arbitrary captured region) - the artist sets this deliberately, at
     * capture time or after.
     */
    double fundamentalFrequencyHz = 0.0;
    /**
     * @brief How far into `clip`'s own captured span, in seconds, the
     *        "true" onset actually sits - `v0.Y.55.1`'s own second stated
     *        prerequisite, alongside `fundamentalFrequencyHz`.
     *
     * A captured region often includes a little context before the actual
     * attack (a pre-roll, a breath, a pick scrape) - stamping it centered
     * on `clip`'s own geometric middle would then place that lead-in
     * *after* a MIDI note's own start time instead of before it. This
     * offset shifts the stamp's own placement earlier by this many
     * seconds, so the captured attack lands exactly on the target note's
     * own start regardless of how much lead-in the capture includes. `0.0`
     * (the default) means "no offset - stamp centered exactly as
     * `blitClipCentered()` already does," the only behavior that existed
     * before this milestone.
     */
    double startTimeOffsetSeconds = 0.0;
};

/// @brief Serializes a named Mind Shot to its JSON representation.
void to_json(nlohmann::json& json, const NamedMindShot& namedMindShot);

/// @brief Parses a named Mind Shot from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedMindShot& namedMindShot);

}  // namespace sound_mind::core
