#pragma once

#include <filesystem>
#include <vector>

#include <QWidget>

class QVBoxLayout;

namespace sound_mind::studio {

/**
 * @brief The Studio's persistent start screen, shown as the central widget
 *        until a project is created or opened - see
 *        `docs/sound-mind-roadmap.md`'s Landing Page milestone (`v0.Y.9.1`).
 *
 * Informed by the legacy Studio's Welcome panel (a quick-actions column
 * plus a Recent Projects list) without carrying over everything it had:
 * no standalone-file actions (there's no standalone-TIFF concept in this
 * rewrite - see the Project Lifecycle milestone), and no startup-profile
 * selector or favorite-directories list - both deferred until there's a
 * real profile/preferences concept, or the added surface is worth it.
 *
 * Purely presentational: every action is a signal the owning `MainWindow`
 * connects to its own existing handlers (`newProject()`, `openProject()`,
 * a path-taking open) - no project or file-system logic lives here, the
 * same division of responsibility the legacy `WelcomePanel` used.
 */
class LandingPage : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the landing page: title, New/Open Project actions, and
    ///        an initially-empty Recent Projects section.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit LandingPage(QWidget* parent = nullptr);

    /**
     * @brief Replaces the displayed Recent Projects entries.
     *
     * An empty list leaves the section with no clickable entries at all
     * (no placeholder text) - the same minimal treatment used elsewhere in
     * this first pass.
     *
     * @param paths Project file paths to show, most recent first - see
     *        `RecentProjects::list()`.
     */
    void setRecentProjects(const std::vector<std::filesystem::path>& paths);

signals:
    /// @brief The user clicked "New Project".
    void newProjectRequested();

    /// @brief The user clicked "Open Project...".
    void openProjectRequested();

    /// @brief The user clicked one of the Recent Projects entries.
    /// @param path The project file path that was clicked.
    void recentProjectRequested(const QString& path);

private:
    QVBoxLayout* recentProjectsLayout_ = nullptr;
};

}  // namespace sound_mind::studio
