#pragma once

#include <filesystem>
#include <optional>

#include <QMainWindow>

#include "sound_mind/core/project.h"

class QString;

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

    /// @brief Prompts for a WAV file and imports it as a new layer.
    void importAudio();

    /// @brief Prompts for an image file and imports it as a new layer.
    void importImage();

public:
    /**
     * @brief Imports a WAV file as a new layer, without prompting or
     *        showing an error dialog on failure.
     *
     * The actual work behind importAudio(), split out so it's callable
     * directly - by a test, or eventually a drag-and-drop handler - without
     * needing a real file dialog, and deliberately without ever showing a
     * message box: `QMessageBox::critical()` blocks on a modal event loop
     * that nothing can dismiss under the `offscreen` QPA platform tests run
     * under, so this stays a plain, headless-safe function and only
     * importAudio() (the interactive slot) shows a dialog, based on the
     * error text this returns.
     *
     * @param path Path to the WAV file to import.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if reading or encoding it failed.
     */
    bool importAudioFile(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /**
     * @brief Imports an image file as a new layer, without prompting or
     *        showing an error dialog on failure.
     *
     * The image's RGB pixels are converted into amplitude/phase data via
     * `sound_mind::codec::fromRgbImage()` - per
     * `docs/sound-mind-design.md`'s "sound and image are one continuous
     * surface" principle, an imported image is genuinely unified with
     * audio-imported content, not a picture with no underlying sound
     * representation. See importAudioFile()'s docs for why this never shows
     * a message box itself.
     *
     * @param path Path to the image file to import.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if loading or converting it failed.
     */
    bool importImageFile(const std::filesystem::path& path, QString* errorMessage = nullptr);

private:
    void setProject(sound_mind::core::Project project);

    std::optional<sound_mind::core::Project> project_;
    std::optional<std::filesystem::path> currentPath_;
    CanvasWidget* canvas_ = nullptr;
};

}  // namespace sound_mind::studio
