#pragma once

#include <filesystem>
#include <vector>

#include <QWidget>

class QVBoxLayout;

namespace sound_mind::studio {

class DeviceConfigurationWidget;

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
 *
 * **As of `v0.Y.14.1` (Visual Identity):** the header shows the legacy
 * Studio's own logo (`ChooseAgainLarge.png`, embedded via `assets/app.qrc`
 * - see `theme.h`) alongside the title text, matching the legacy
 * `WelcomePanel`'s header layout now that the asset has been carried over.
 *
 * **As of `v0.0.42.4` (Workflow & Device Polish, Installment D):** a
 * "Documentation" section below Recent Projects - Quick Start/Readme/User
 * Guide/Changelog/About, each a flat button emitting its own signal - the
 * same "Read" column the legacy `WelcomePanel` had (informed by, per this
 * class's own header docs, without carrying over its two-column Open/Read
 * layout - a single centered column matches this class's own existing
 * style instead).
 *
 * **As of `v0.Y.62.1` Installment H:** a "Device Configuration" section
 * below Recent Projects, embedding one `DeviceConfigurationWidget`
 * instance - the same input/output device selection, gain, and "test it"
 * controls `ConfigureDevicesPanel`'s own dock already offers - so a user
 * can pick and test audio devices immediately, before even creating or
 * opening a project (`MainWindow` already constructs
 * `recordEngine_`/`playbackController_`/`deviceTestTonePlayer_`
 * regardless of whether a project exists, so there's nothing project-
 * specific blocking this). deviceConfiguration()'s own docs cover how
 * `MainWindow` keeps this instance and the dock's own instance in sync -
 * this class itself just embeds and displays it, the same "purely
 * presentational" treatment every other control here already gets.
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

    /**
     * @brief The "Device Configuration" section's own embedded controls -
     *        `v0.Y.62.1` Installment H. `MainWindow` connects its own
     *        signals and calls its own setters exactly as it already
     *        does for `ConfigureDevicesPanel`'s own instance (both need
     *        the same device lists, gain levels, and testing state kept
     *        in sync, since they're two views onto the same underlying
     *        preferences) - this class owns the instance (constructed in
     *        the constructor, always non-null) but has no behavior of
     *        its own to add on top of it.
     * @return The embedded widget. Never `nullptr`.
     */
    [[nodiscard]] DeviceConfigurationWidget* deviceConfiguration() const noexcept { return deviceConfiguration_; }

signals:
    /// @brief The user clicked "New Project".
    void newProjectRequested();

    /// @brief The user clicked "Open Project...".
    void openProjectRequested();

    /// @brief The user clicked one of the Recent Projects entries.
    /// @param path The project file path that was clicked.
    void recentProjectRequested(const QString& path);

    /// @brief The user clicked "Quick Start" in the Documentation section.
    void quickStartRequested();

    /// @brief The user clicked "Readme" in the Documentation section.
    void readmeRequested();

    /// @brief The user clicked "User Guide" in the Documentation section.
    void userGuideRequested();

    /// @brief The user clicked "Changelog" in the Documentation section.
    void changelogRequested();

    /// @brief The user clicked "About" in the Documentation section.
    void aboutRequested();

private:
    QVBoxLayout* recentProjectsLayout_ = nullptr;
    DeviceConfigurationWidget* deviceConfiguration_ = nullptr;
};

}  // namespace sound_mind::studio
