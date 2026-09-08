#pragma once

#include <filesystem>
#include <vector>

class QSettings;

namespace sound_mind::studio {

/**
 * @brief Persists and retrieves a most-recently-used list of Sound Mind
 *        Project file paths, for the Landing Page's "Recent Projects"
 *        section - see `docs/sound-mind-roadmap.md`'s Landing Page
 *        milestone (`v0.Y.9.1`).
 *
 * Backed by a caller-supplied `QSettings`, rather than opening its own, so
 * production code (see `MainWindow`) and tests can point it at different
 * storage: `MainWindow` uses a real, ini-format settings file; tests use
 * one scoped to a throwaway temp directory (via `QTemporaryDir`, or the
 * whole test binary's `QStandardPaths::setTestModeEnabled(true)` - see
 * `tests/main.cpp`), so running the test suite never touches, or pollutes
 * with throwaway test paths, the real Studio's own persisted list.
 *
 * @note Not thread-safe - meant to be constructed and used from the UI
 *       thread only, same as the `MainWindow` it serves.
 */
class RecentProjects {
public:
    /// @brief Maximum number of entries retained - add() silently drops
    /// the least-recently-used entry once a new one would exceed this.
    static constexpr int kMaxEntries = 10;

    /**
     * @brief Wraps `settings` for reading/writing the recent-projects list.
     * @param settings The settings store to use. Not owned - must outlive
     *        this object.
     */
    explicit RecentProjects(QSettings& settings);

    /**
     * @brief Records `path` as the most recently used project.
     *
     * If `path` is already present, it moves to the front rather than
     * being duplicated. The list is trimmed to `kMaxEntries` afterward,
     * dropping the least-recently-used entries beyond that.
     *
     * @param path The project file path to record.
     */
    void add(const std::filesystem::path& path);

    /**
     * @brief The persisted recent-project paths, most recent first.
     *
     * Any path that no longer exists on disk is silently filtered out,
     * matching the legacy Studio's own recent-items behavior - a project
     * that was moved or deleted since it was last opened simply doesn't
     * show up, rather than showing as a broken entry.
     *
     * @return Up to `kMaxEntries` existing paths.
     */
    [[nodiscard]] std::vector<std::filesystem::path> list() const;

private:
    QSettings& settings_;
};

}  // namespace sound_mind::studio
