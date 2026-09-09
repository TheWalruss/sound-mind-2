#pragma once

#include <filesystem>
#include <optional>

#include <QMainWindow>
#include <QSettings>

#include "sound_mind/core/live_engine.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/record_engine.h"
#include "sound_mind/studio/recent_projects.h"

class QCloseEvent;
class QStackedWidget;
class QString;
class QTimer;

namespace sound_mind::studio {

class CanvasWidget;
class CreateProjectWizard;
class LandingPage;

/**
 * @brief The Sound Mind Studio main window.
 *
 * Owns the currently open Project (if any - see the Landing Page milestone,
 * `v0.Y.9.1`, below) and a File menu (New/Open/Save/Save As/Import Audio/
 * Import Image) over it, plus the CanvasWidget that reflects it and a
 * transport toolbar (Play/Pause/Stop) over a PlaybackEngine. See
 * `docs/sound-mind-roadmap.md`'s Playback milestone (`v0.0.4.1`) -
 * everything else (real compositing, layers panel, etc.) arrives in later
 * milestones.
 *
 * **As of `v0.Y.9.1`:** the window no longer silently creates a project at
 * startup. Its central widget is a `QStackedWidget` alternating between a
 * `LandingPage` (shown until a project actually exists) and the
 * `CanvasWidget` - see setProject()'s and isShowingLandingPage()'s docs.
 * The Landing Page's Recent Projects list is backed by `recentProjects_`,
 * an ini-format `QSettings` store distinct from the legacy Python Studio's
 * own settings (a fresh product identity for a rewrite with an
 * incompatible project file format) - see recent_projects.h.
 *
 * **As of `v0.Y.10.1` (Project Lifecycle):** every path that can discard
 * unsaved work - New, Open (both the file-dialog and Recent Projects
 * routes), and closing the window - now guards on hasUnsavedChanges(),
 * via confirmDiscardUnsavedChanges(). Separately, New/Open/Close all
 * refuse outright (no dialog, just a status-bar message) while Live Mode
 * or Recording is active, rather than risking either an unsaved-changes
 * prompt *or* a project switch silently discarding an in-progress
 * capture - see toggleLiveMode()'s/toggleRecording()'s own docs for why
 * "refuse rather than surprise-stop" is this codebase's established
 * answer to exactly that tension. setProject() itself unconditionally
 * stops both engines and clears liveLayerId_ regardless, as a defensive
 * invariant - normally unreachable through the guarded entry points
 * above, but keeping the *audit point* itself correct is what the
 * Project Lifecycle milestone actually asked for.
 *
 * **As of `v0.Y.11.1` (Create Project Wizard):** newProject() now shows a
 * real `CreateProjectWizard` dialog instead of silently creating an
 * in-memory default project - see its own docs, and createProjectAt()'s,
 * for the interactive/testable split this introduces (the same shape as
 * openProject()/openProjectAt()). A project's `ProjectSettings` (sample
 * rate, frequency range, bin count, timestep) now actually drives
 * `sound_mind::codec::encode()`/`fromRgbImage()` at every direct call
 * site (`importAudioFile()`, `importImageFile()`, Recording's post-
 * capture encode) via `sound_mind::core::streamCodecConfigFor()` -
 * previously all three silently used a hardcoded default regardless of
 * the open project's settings. `LiveEngine`'s own construction-time
 * config is deliberately not part of this wiring - see
 * `streamCodecConfigFor()`'s own docs for why.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// @brief Builds the window: menu bar, transport toolbar, and the
    ///        Landing Page/Canvas stack - see the class docs for why no
    ///        project exists yet at this point. Sets the window icon to
    ///        `theme::studioWindowIcon()` - see `docs/sound-mind-roadmap.md`'s
    ///        Visual Identity milestone (`v0.Y.14.1`); the app-wide QSS
    ///        (`theme::studioStyleSheet()`) is applied once, at the
    ///        `QApplication` level in `main.cpp`, not per-window here.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief The currently open project, if any.
     * @return A pointer to the open project, or `nullptr` if none is open
     *         yet - see isShowingLandingPage().
     */
    [[nodiscard]] const sound_mind::core::Project* project() const noexcept;

public slots:
    /**
     * @brief Shows the Create Project wizard and, if accepted, replaces
     *        the current project with the new one it describes - see
     *        createProjectAt() for the actual, non-prompting work, and
     *        for why this is split out the same way openProject()/
     *        openProjectAt() are.
     *
     * Per the Project Lifecycle milestone (`v0.Y.10.1`): refuses outright
     * (a status-bar message, no dialog - checked *before* the wizard is
     * even shown) while Live Mode or Recording is active; otherwise, if
     * the current project has unsaved changes, prompts to save/discard/
     * cancel first (see confirmDiscardUnsavedChanges()) - proceeds
     * unguarded if there's nothing to lose. Per the Create Project Wizard
     * milestone (`v0.Y.11.1`): does nothing further if the wizard is
     * cancelled; on failure to save the new project, reports it via a
     * modal (the new project still becomes current regardless - see
     * createProjectAt()'s docs for why).
     */
    void newProject();

    /**
     * @brief Prompts for a file and opens it as the current project - see
     *        openProjectAt() for the actual, non-prompting work, and for
     *        why this is split out the same way importAudio()/
     *        importAudioFile() are.
     *
     * Per the Project Lifecycle milestone (`v0.Y.10.1`): guarded exactly
     * like newProject() (refuses outright while Live Mode/Recording is
     * active; prompts first if there are unsaved changes) *before* even
     * showing the file picker - see newProject()'s docs.
     */
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
    /// Progress and completion are reported via the status bar (non-modal) -
    /// see poolTopmostLayer()'s docs for why only failure shows a modal.
    void importAudio();

    /// @brief Prompts for an image file and imports it as a new layer.
    /// Progress and completion are reported via the status bar (non-modal) -
    /// see poolTopmostLayer()'s docs for why only failure shows a modal.
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
     * position); does nothing if no layer has content, or none is open, or
     * Live Mode or Recording is currently running (see toggleLiveMode()'s/
     * toggleRecording()'s docs for why all three are mutually exclusive in
     * this first pass).
     */
    void startPlayback();

    /// @brief Pauses playback; startPlayback() resumes from the same position.
    void pausePlayback();

    /// @brief Stops playback and rewinds to the beginning.
    void stopPlayback();

    /**
     * @brief Starts or stops Live Mode: continuously captures the default
     *        input device into a newly created layer, encoding and
     *        streaming it back out in real time (see
     *        `sound_mind::core::LiveEngine`'s docs for the full pipeline).
     *
     * Per the confirmed scope for this milestone: the output is the live
     * input alone, round-tripped through the Stream codec - not composited
     * with any other layer or project content yet (real multi-layer audio
     * mixing doesn't exist anywhere in the codebase yet - see LiveEngine's
     * own docs). Stops Playback first if it's running - both engines would
     * otherwise try to open the system's default output device
     * simultaneously through two independent JUCE device managers, which
     * isn't guaranteed to work depending on the platform/driver. Creates a
     * new Normal layer ("Live Input") to capture into; while running, that
     * layer's content refreshes from LiveEngine::currentImage() on a timer
     * and the canvas repaints, so the spectrogram visibly grows in real
     * time (per the confirmed scope for this milestone) - stopping leaves
     * the layer's content as whatever was last captured, exactly like any
     * other layer. Does nothing (refuses to start) if Recording is
     * currently running - both would otherwise want the same input device
     * at once, through two independent JUCE device managers.
     *
     * Marks hasUnsavedChanges() the moment the "Live Input" layer is
     * added (starting) - per the Project Lifecycle milestone (`v0.Y.10.1`),
     * New/Open/Close all refuse outright while this is running, so the
     * capture itself is never at risk of being silently discarded by a
     * project switch.
     */
    void toggleLiveMode();

    /**
     * @brief Starts or stops Recording: one-shot capture from the default
     *        input device into a newly created layer, encoded exactly as
     *        an imported file would be once capture stops (see
     *        `sound_mind::core::RecordEngine`'s docs for why this differs
     *        from Live Mode's continuous, incrementally-encoded pipeline).
     *
     * Per the confirmed scope for this milestone (mirroring Playback's and
     * Live Mode's own precedent): defers a real input-device picker and
     * input gain control - both named in the design doc's Record section -
     * as UI affordances layered on top of a working capture pipeline.
     * Stops Playback first if it's running (same device-contention
     * reasoning as toggleLiveMode()); does nothing (refuses to start) if
     * Live Mode is currently running, or if no project is open. Creates a
     * new Normal layer ("Recording") once capture stops and something was
     * actually captured; stopping with nothing captured (e.g. no input
     * device was available) leaves the project unchanged.
     *
     * Marks hasUnsavedChanges() when a layer is actually added. Per the
     * Project Lifecycle milestone (`v0.Y.10.1`), New/Open/Close all refuse
     * outright while Recording is running - see toggleLiveMode()'s docs.
     */
    void toggleRecording();

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
     * CanvasWidget use. Progress and success are reported via the status
     * bar (non-modal, since pooling can take a real amount of time); only
     * a failure shows a modal, per this milestone's confirmed "status bar
     * for progress/success, keep errors modal" scope.
     */
    void poolTopmostLayer();

    /**
     * @brief Prompts for a destination file and exports the topmost layer
     *        with content's audio to it (see `sound_mind::core::
     *        exportLayerAudio()` - Pool content when available, else a
     *        Stream-mode bounce).
     *
     * The chosen format (Flac/Ogg/MP3) is inferred from the destination
     * file's extension. Does nothing if no layer has content, or none is
     * open. Progress and completion are reported via the status bar
     * (non-modal) - see poolTopmostLayer()'s docs for why only failure
     * shows a modal.
     */
    void exportAudio();

    /**
     * @brief Prompts for a destination file and exports the topmost layer
     *        with content as an MP4 video (see `sound_mind::core::
     *        exportLayerVideo()`): its rendered canvas animated with a
     *        playhead synced to its audio.
     *
     * Does nothing if no layer has content, or none is open. Progress and
     * completion are reported via the status bar (non-modal) - see
     * poolTopmostLayer()'s docs for why only failure shows a modal.
     */
    void exportVideo();

public:
    /**
     * @brief Opens the project at `path` as the current project, without
     *        prompting or showing an error dialog on failure.
     *
     * The actual work behind openProject(), split out so it's callable
     * directly - by a test, or by the Landing Page's Recent Projects
     * entries - without needing a real file dialog. On success, records
     * `path` via `recentProjects_` and refreshes the Landing Page's list -
     * see importAudioFile()'s docs for why this never shows a message box
     * itself.
     *
     * Per the Project Lifecycle milestone (`v0.Y.10.1`): refuses (no
     * dialog - `errorMessage` is set, same as any other failure here)
     * while Live Mode or Recording is active - the one guard this method
     * *does* enforce itself, since it's a plain status check rather than
     * a blocking prompt. The unsaved-changes confirmation is deliberately
     * `openProject()`'s/the Recent Projects handler's job, not this
     * method's - callers that intentionally bypass the interactive layer
     * (tests, in particular) still get a non-prompting call this way.
     *
     * @param path Path to the `.smproj` file to open.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if the file couldn't be loaded,
     *         or if Live Mode/Recording is currently active.
     */
    bool openProjectAt(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /**
     * @brief Creates a new project with `settings`, saves it to `path`
     *        immediately, and makes it the current project - without
     *        showing any dialog. The actual work behind newProject(),
     *        split out the same way openProjectAt() is.
     *
     * Per the Create Project Wizard milestone (`v0.Y.11.1`): "the
     * wizard's completion *is* the first save" - a project created this
     * way always has a real file on disk from the moment it exists,
     * unlike the pre-wizard `newProject()`'s in-memory-only result. If
     * the save itself fails, the new project still becomes current (its
     * `currentPath_` is still set to `path`, so a later `saveProject()`
     * retries the same location) - reported via `errorMessage`/the
     * return value rather than rolled back, the same "report, don't
     * silently revert" precedent `importAudioFile()` and friends use.
     *
     * @param settings The settings for the new project.
     * @param path Destination path to save it to immediately.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` if the save succeeded; `false` if it failed (the
     *         project is still current, just not yet actually on disk).
     */
    bool createProjectAt(sound_mind::core::ProjectSettings settings, const std::filesystem::path& path,
                          QString* errorMessage = nullptr);

    /// @brief Whether the Landing Page is currently the visible central
    ///        widget (no project open yet) rather than the canvas.
    /// @return `true` until setProject() has been called at least once.
    [[nodiscard]] bool isShowingLandingPage() const noexcept;

    /**
     * @brief Whether the current project has changes not yet reflected in
     *        its last save (or, for a project never saved at all, changes
     *        beyond its just-created state).
     *
     * Tracked as a plain flag, set whenever content actually changes
     * (import, Pool, a Live/Recording capture adding or updating a layer)
     * and cleared by a successful save or by setProject() (a fresh/loaded
     * project matches what's on disk, or - for `newProject()` - has
     * nothing on disk to differ from yet). Deliberately simpler than
     * diffing against the operation log's replay: no operation type logs
     * these particular mutations yet (that starts with real `Operation`
     * subtypes in Phase 3's Basic Painting milestone) - revisit once it
     * does, rather than building a fuller mechanism speculatively now.
     *
     * @return `true` if closing or switching away from the current
     *         project right now would lose something.
     */
    [[nodiscard]] bool hasUnsavedChanges() const noexcept;

    /// @brief Whether playback is currently active.
    /// @return The underlying PlaybackEngine's isPlaying().
    [[nodiscard]] bool isPlaying() const noexcept;

    /// @brief Whether Live Mode is currently capturing.
    /// @return The underlying LiveEngine's isRunning().
    [[nodiscard]] bool isLiveModeRunning() const noexcept;

    /// @brief Whether Recording is currently capturing.
    /// @return The underlying RecordEngine's isRecording().
    [[nodiscard]] bool isRecording() const noexcept;

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
     * Marks hasUnsavedChanges() on success, per the Project Lifecycle
     * milestone (`v0.Y.10.1`).
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
     * Marks hasUnsavedChanges() on success, same as importAudioFile().
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
     *
     * Marks hasUnsavedChanges() on success (the layer's content genuinely
     * changed in place - see `sound_mind::core::poolLayer()`'s docs).
     */
    bool poolTopmostLayerNow(QString* errorMessage = nullptr, QString* streamPngPath = nullptr,
                              QString* poolPngPath = nullptr);

    /**
     * @brief Exports the topmost layer with content's audio to `path`,
     *        without prompting or showing an error dialog on failure - the
     *        actual work behind exportAudio(), split out for the same
     *        headless-testability reason as importAudioFile() (see its docs).
     *
     * @param path Destination path; its extension (`.flac`/`.ogg`/`.mp3`)
     *        selects the compressed format.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if there was no layer to export,
     *         the extension didn't match a supported format, or the
     *         underlying codec export failed.
     */
    bool exportTopmostLayerAudioNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /**
     * @brief Exports the topmost layer with content as an MP4 video at
     *        `path`, without prompting or showing an error dialog on
     *        failure - the actual work behind exportVideo(), split out for
     *        the same headless-testability reason as importAudioFile()
     *        (see its docs).
     *
     * @param path Destination path.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if there was no layer to export,
     *         or the underlying codec export failed.
     */
    bool exportTopmostLayerVideoNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

protected:
    /**
     * @brief Guards window close the same way newProject()/openProject()
     *        do: refuses (ignores the event, a status-bar message) while
     *        Live Mode or Recording is active; otherwise prompts via
     *        confirmDiscardUnsavedChanges() if there are unsaved changes,
     *        ignoring the event if the user cancels.
     * @param event The close event; accepted or ignored per the above.
     */
    void closeEvent(QCloseEvent* event) override;

private:
    void setProject(sound_mind::core::Project project);

    /**
     * @brief If hasUnsavedChanges() is `false`, returns `true` immediately
     *        (nothing to guard). Otherwise, prompts (Save/Discard/Cancel)
     *        and returns whether the caller should proceed with whatever
     *        would discard the current project - `true` for Discard, or
     *        for Save once it actually completes (hasUnsavedChanges()
     *        false afterward - a cancelled/failed save leaves it `true`,
     *        so this correctly still returns `false`); `false` for
     *        Cancel.
     *
     * Shows a real, blocking `QMessageBox` when there's something to
     * guard - callers that must stay headless-safe (tests, in particular)
     * should never invoke this while hasUnsavedChanges() is `true`, the
     * same constraint every other `QMessageBox`-showing method in this
     * class already carries (see importAudioFile()'s docs).
     *
     * @return Whether the caller should proceed.
     */
    [[nodiscard]] bool confirmDiscardUnsavedChanges();

    /// @brief The topmost layer with content, if any - the same notion of
    /// "the composite" startPlayback(), CanvasWidget, and
    /// poolTopmostLayer() all share for now (see their docs).
    /// @return A mutable pointer to that layer, or `nullptr` if none has
    ///         content, or no project is open.
    [[nodiscard]] sound_mind::core::Layer* topmostLayerWithContent();

    /// @brief The layer with the given id, if the current project has one.
    /// @return A mutable pointer to that layer, or `nullptr` if no project
    ///         is open or no layer in it has this id.
    [[nodiscard]] sound_mind::core::Layer* layerById(sound_mind::core::LayerId id);

    /// @brief liveUpdateTimer_'s slot: refreshes the Live layer's content
    /// from liveEngine_.currentImage() and repaints the canvas, while Live
    /// Mode is running - see toggleLiveMode()'s docs.
    void updateLiveLayer();

    /// @brief recordDrainTimer_'s slot: moves whatever's newly captured
    /// out of recordEngine_'s ring buffer, while Recording is running -
    /// see RecordEngine::drainAvailable()'s docs for why this needs to
    /// happen periodically rather than only once recording stops.
    void drainRecording();

    std::optional<sound_mind::core::Project> project_;
    std::optional<std::filesystem::path> currentPath_;

    /// @brief Backing flag for hasUnsavedChanges() - see its own docs for
    /// what sets and clears it.
    bool hasUnsavedChanges_ = false;

    /// @brief Alternates between landingPage_ (index 0, shown until a
    /// project exists) and canvas_ (index 1) - see setProject()'s docs.
    QStackedWidget* stack_ = nullptr;
    LandingPage* landingPage_ = nullptr;
    CanvasWidget* canvas_ = nullptr;

    /// @brief Backing store for recentProjects_ - an ini-format file (not
    /// the platform registry/native format) specifically so
    /// QStandardPaths::setTestModeEnabled(true) (see tests/main.cpp) can
    /// sandbox it during automated tests, without touching whatever a real
    /// installed Studio has persisted on the same machine.
    QSettings settings_{QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SoundMind"),
                         QStringLiteral("SoundMindStudio")};
    RecentProjects recentProjects_{settings_};

    sound_mind::core::PlaybackEngine playbackEngine_;

    /// @brief Whether playbackEngine_ already has the current topmost
    /// layer's audio loaded - so startPlayback() knows to just resume
    /// rather than re-decode and restart from the beginning. Cleared by
    /// stopPlayback() and whenever the project (or its content) changes.
    bool playbackLoaded_ = false;

    sound_mind::core::LiveEngine liveEngine_{sound_mind::codec::StreamCodecConfig{}};
    QTimer* liveUpdateTimer_ = nullptr;

    /// @brief The layer currently being captured into, while Live Mode is
    /// running - std::nullopt otherwise. An id, not a Layer*, since
    /// Project::layers() is a std::vector<Layer> that addLayer() (or
    /// future layer-list operations) can reallocate, invalidating a raw
    /// pointer held across such a call - see layerById().
    std::optional<sound_mind::core::LayerId> liveLayerId_;

    sound_mind::core::RecordEngine recordEngine_{sound_mind::codec::StreamCodecConfig{}.sampleRateHz};
    QTimer* recordDrainTimer_ = nullptr;
};

}  // namespace sound_mind::studio
