#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include <QString>

#include "sound_mind/core/history.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/project_sources.h"

namespace sound_mind::studio {

/**
 * @brief Everything a Remaster needs from the original project, copied out
 *        on the UI thread so the remaster itself can run on a background
 *        thread while the original stays open (`v0.1.43.4`).
 */
struct RemasterInput {
    /// The original's settings.
    sound_mind::core::ProjectSettings originalSettings;
    /// Its applied History entries, in order.
    std::vector<sound_mind::core::HistoryEntry> entries;
    /// Its source registry.
    std::vector<sound_mind::core::ProjectSource> sources;
    /// Its `sources/` folder.
    std::filesystem::path sourcesFolder;
};

/**
 * @brief What the user asked a Remaster to change.
 */
struct RemasterRequest {
    /// The codec settings to rebuild at (sample rate, time step, frequency
    /// bins and range); the canvas width is derived so the duration stays
    /// the same (`sound_mind::core::remasterSettings()`).
    sound_mind::core::ProjectSettings settings;
    /// Sources to swap for other files - every operation that used one
    /// works on its replacement instead.
    std::map<sound_mind::core::SourceId, std::filesystem::path> replacements;
    /// Where to save the new project (`.smproj`); must differ from the
    /// original's.
    std::filesystem::path outputPath;
};

/**
 * @brief Copies what remasterProject() needs out of `project`.
 * @param project The original; its History must start at its creation.
 * @return The input, or `std::nullopt` if the History doesn't start at the
 *         project's creation or the project has no folder.
 */
[[nodiscard]] std::optional<RemasterInput> remasterInputFrom(const sound_mind::core::Project& project);

/**
 * @brief Builds a new project by replaying the original's whole History with
 *        the requested changes (`v0.1.43.4`, `docs/sound-mind-design.md`'s
 *        *History and Remastering*), and saves it at `request.outputPath`.
 *
 * Steps:
 * 1. Copy the sources into the new project's folder, adding each
 *    replacement as a source of its own.
 * 2. Adapt every entry: grid-unit data to the new settings
 *    (`sound_mind::core::adaptEntryForRemaster()`); replaced sources'
 *    ids in recipes; text pastes re-rendered from their text at the new
 *    resolution.
 * 3. Replay them from `Project::createNew()`, remaking every base from its
 *    recipe (nothing kept in memory - those were at the old settings).
 * 4. Give the result the adapted entries as its own History, so it can be
 *    replayed and remastered in turn, and save it.
 *
 * Safe to call on a background thread: touches no live project.
 *
 * @param input From remasterInputFrom().
 * @param request What to change.
 * @param shouldCancel Polled between entries; may be empty.
 * @param progress Told how many entries are done of how many; may be empty.
 * @param errorMessage Receives the reason on failure; may be `nullptr`.
 * @return The new project (already saved), or `std::nullopt`.
 */
[[nodiscard]] std::optional<sound_mind::core::Project> remasterProject(
    const RemasterInput& input, const RemasterRequest& request, const std::function<bool()>& shouldCancel,
    const std::function<void(std::size_t, std::size_t)>& progress, QString* errorMessage);

}  // namespace sound_mind::studio
