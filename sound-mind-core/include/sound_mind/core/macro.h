#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/history.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

class Project;

/// @brief Identifies a Macro within its project; never reused.
using MacroId = std::uint64_t;

/// @brief What a MacroEvent records.
enum class MacroEventKind {
    Entry,  ///< A change was made: its History entry was applied.
    Undo,   ///< A change was undone: its entry was reverted.
    Redo,   ///< An undone change was redone: its entry was applied again.
    Play,   ///< Playback (or Loop) started, from `position`.
    Pause,  ///< Playback stopped, at `position`.
    Seek,   ///< The playhead jumped to `position` while playing (a seek, a
            ///< Repeat wrap, a new Loop pass).
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(MacroEventKind, {
    {MacroEventKind::Entry, "entry"},
    {MacroEventKind::Undo, "undo"},
    {MacroEventKind::Redo, "redo"},
    {MacroEventKind::Play, "play"},
    {MacroEventKind::Pause, "pause"},
    {MacroEventKind::Seek, "seek"},
})
// clang-format on

/**
 * @brief One moment of a recorded performance (`v0.1.44.1`).
 */
struct MacroEvent {
    /// When, on the Macro's own clock (seconds) - which only runs while
    /// Playback or Loop does.
    double time = 0.0;
    /// What happened.
    MacroEventKind kind = MacroEventKind::Entry;
    /// For Entry/Undo/Redo: the History entry - a copy, so the Macro doesn't
    /// depend on the project's history keeping it. Without its session-only
    /// base images.
    std::optional<HistoryEntry> entry;
    /// For Play/Pause/Seek: the playhead position (seconds) right after.
    double position = 0.0;
};

/// @brief Serializes an event.
void to_json(nlohmann::json& json, const MacroEvent& event);

/// @brief Parses an event.
/// @throws nlohmann::json::exception on missing or malformed fields.
void from_json(const nlohmann::json& json, MacroEvent& event);

/**
 * @brief A recorded performance (`v0.1.44.1`, `docs/sound-mind-design.md`'s
 *        *Macros*): what the project was when recording began, and what was
 *        done to it, and what the playhead did, on the Macro's own clock.
 *
 * The starting state is "the project after its History entry
 * `startAfterEntry`" (`0` = as created). If later editing drops History
 * entries the Macro starts after, the project's History copies them into
 * `retainedPrefix` first (see `History::commit()`), so a Macro stays
 * playable however the project is edited afterwards.
 */
struct Macro {
    /// Its id in the project's library.
    MacroId id = 0;
    /// Its name.
    std::string name;
    /// The settings the project had when it was recorded.
    ProjectSettings settings;
    /// The last History entry applied when recording began; `0` = none.
    HistoryEntryId startAfterEntry = 0;
    /// Whether the project's History started at its creation when the
    /// Macro was recorded - a Macro can only be rebuilt if so.
    bool historyStartsAtCreation = false;
    /// Whether `retainedPrefix` holds the starting entries (the project's
    /// History no longer does).
    bool prefixRetained = false;
    /// The History entries up to and including `startAfterEntry`, once
    /// `prefixRetained`.
    std::vector<HistoryEntry> retainedPrefix;
    /// Everything recorded, in time order.
    std::vector<MacroEvent> events;
    /// The clock's reading when recording stopped (seconds).
    double durationSeconds = 0.0;
    /// Whether it's still being recorded.
    bool recording = false;
};

/// @brief Serializes a Macro.
void to_json(nlohmann::json& json, const Macro& macro);

/// @brief Parses a Macro.
/// @throws nlohmann::json::exception on missing or malformed fields.
void from_json(const nlohmann::json& json, Macro& macro);

/**
 * @brief The History entries that rebuild `project` as it was when `macro`
 *        began - its retained prefix, or the project's own entries up to
 *        `startAfterEntry`.
 * @param project The project the Macro belongs to.
 * @param macro The Macro.
 * @return The entries, or `std::nullopt` if they can't be had (the History
 *         didn't start at creation when it was recorded, or the start entry
 *         is gone without having been retained).
 */
[[nodiscard]] std::optional<std::vector<HistoryEntry>> macroStartEntries(const Project& project, const Macro& macro);

/**
 * @brief Where the playhead was, and whether it was moving, at a moment of
 *        a Macro - from its Play/Pause/Seek events: it moves at normal
 *        speed from each Play or Seek (while playing) and stands still from
 *        each Pause.
 */
struct MacroPlayhead {
    /// Position, seconds into the project.
    double position = 0.0;
    /// Whether playback was running.
    bool playing = false;
};

/**
 * @brief The playhead at Macro time `time`.
 * @param macro The Macro.
 * @param time Seconds on the Macro's clock.
 * @return Where it was.
 */
[[nodiscard]] MacroPlayhead macroPlayheadAt(const Macro& macro, double time);

}  // namespace sound_mind::core
