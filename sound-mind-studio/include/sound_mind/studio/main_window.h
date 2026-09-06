#pragma once

#include <filesystem>
#include <optional>

#include <QMainWindow>

#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class CanvasWidget;

/**
 * @brief The Sound Mind Studio main window.
 *
 * Owns the currently open Project and a File menu (New/Open/Save/Save As)
 * over it, plus the CanvasWidget that reflects it. See
 * `docs/sound-mind-roadmap.md`'s "Project & Canvas" milestone - everything
 * else (painting, layers panel, etc.) arrives in later milestones.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief The currently open project.
     * @return A pointer to the open project. Never `nullptr` - a window
     *         always has some project open, starting with a fresh one.
     */
    [[nodiscard]] const sound_mind::core::Project* project() const noexcept;

public slots:
    /// @brief Replaces the current project with a freshly created one.
    void newProject();

    /// @brief Prompts for a file and opens it as the current project.
    void openProject();

    /**
     * @brief Saves the current project.
     *
     * Prompts for a location first (see saveProjectAs()) if it doesn't
     * have one yet - i.e. it was never opened from, or saved to, a file.
     */
    void saveProject();

    /// @brief Prompts for a file and saves the current project there.
    void saveProjectAs();

private:
    void setProject(sound_mind::core::Project project);

    std::optional<sound_mind::core::Project> project_;
    std::optional<std::filesystem::path> currentPath_;
    CanvasWidget* canvas_ = nullptr;
};

}  // namespace sound_mind::studio
