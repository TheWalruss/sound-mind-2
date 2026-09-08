#pragma once

#include <filesystem>
#include <optional>

#include <QMainWindow>

#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"

class QString;

namespace sound_mind::studio {

class CanvasWidget;

/**
 * @brief The Sound Mind Studio main window.
 *
 * Owns the currently open Project and a File menu (New/Open/Save/Save As/
 * Import Audio/Import Image) over it, plus the CanvasWidget that reflects
 * it and a transport toolbar (Play/Pause/Stop) over a PlaybackEngine. See
 * `docs/sound-mind-roadmap.md`'s Playback milestone (`v0.0.4.1`) -
 * everything else (real compositing, live-edit-reactive playback, layers
 * panel, etc.) arrives in later milestones.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// @brief Builds the window: menu bar, transport toolbar, canvas, and a
    ///        fresh project to start with.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
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

    /**
     * @brief Starts (or resumes) playback of the topmost layer with
     *        content.
     *
     * Per the confirmed scope for this milestone: "the composite" is, for
     * now, just whichever layer CanvasWidget would also show (see
     * `sound_mind::core::renderLayer()`'s docs) - real multi-layer mixing
     * doesn't exist yet. Decodes and loads that layer's audio once (not on
     * every call - resuming after pausePlayback() continues from the same
     * position); does nothing if no layer has content, or none is open.
     */
    void startPlayback();

    /// @brief Pauses playback; startPlayback() resumes from the same position.
    void pausePlayback();

    /// @brief Stops playback and rewinds to the beginning.
    void stopPlayback();

    /**
     * @brief Pools the topmost layer with content, then exports both its
     *        Stream and Pool renders as PNG files, for side-by-side
     *        comparison in any image viewer.
     *
     * Per the confirmed scope for this milestone: pooling replaces the
     * layer's content in place (see `sound_mind::core::poolLayer()`'s
     * docs for why this isn't yet the design doc's "hide-not-delete,
     * undoable operation" behavior) rather than prompting for anything -
     * "the topmost layer" is the same one startPlayback() and
     * CanvasWidget use.
     */
    void poolTopmostLayer();

public:
    /// @brief Whether playback is currently active.
    /// @return The underlying PlaybackEngine's isPlaying().
    [[nodiscard]] bool isPlaying() const noexcept;

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

    /**
     * @brief Pools the topmost layer with content and writes its Stream
     *        and Pool renders as PNG files, without showing any dialog -
     *        the actual work behind poolTopmostLayer(), split out for the
     *        same headless-testability reason as importAudioFile()/
     *        importImageFile() (see their docs).
     *
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @param streamPngPath If non-null and this returns `true`, set to the
     *        path the Stream render was written to.
     * @param poolPngPath If non-null and this returns `true`, set to the
     *        path the Pool render was written to.
     * @return `true` on success; `false` if there was no layer to pool, or
     *         writing either PNG failed.
     */
    bool poolTopmostLayerNow(QString* errorMessage = nullptr, QString* streamPngPath = nullptr,
                              QString* poolPngPath = nullptr);

private:
    void setProject(sound_mind::core::Project project);

    /// @brief The topmost layer with content, if any - the same notion of
    /// "the composite" startPlayback(), CanvasWidget, and
    /// poolTopmostLayer() all share for now (see their docs).
    /// @return A mutable pointer to that layer, or `nullptr` if none has
    ///         content, or no project is open.
    [[nodiscard]] sound_mind::core::Layer* topmostLayerWithContent();

    std::optional<sound_mind::core::Project> project_;
    std::optional<std::filesystem::path> currentPath_;
    CanvasWidget* canvas_ = nullptr;
    sound_mind::core::PlaybackEngine playbackEngine_;

    /// @brief Whether playbackEngine_ already has the current topmost
    /// layer's audio loaded - so startPlayback() knows to just resume
    /// rather than re-decode and restart from the beginning. Cleared by
    /// stopPlayback() and whenever the project (or its content) changes.
    bool playbackLoaded_ = false;
};

}  // namespace sound_mind::studio
