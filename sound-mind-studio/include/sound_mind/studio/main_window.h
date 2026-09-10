#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <QMainWindow>
#include <QSettings>

#include "sound_mind/core/loop_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/record_engine.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/recent_projects.h"

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QStackedWidget;
class QString;
class QTimer;

namespace sound_mind::studio {

class CanvasWidget;
class CreateProjectWizard;
class LandingPage;
class LayersPanel;
class LoopPanel;
class PlaybackPanel;
class RecordPanel;

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
 * refuse outright (no dialog, just a status-bar message) while Loop Mode
 * or Recording is active, rather than risking either an unsaved-changes
 * prompt *or* a project switch silently discarding an in-progress
 * capture - see toggleLoopMode()'s/toggleRecording()'s own docs for why
 * "refuse rather than surprise-stop" is this codebase's established
 * answer to exactly that tension. setProject() itself unconditionally
 * stops both engines and clears loopLayerId_ regardless, as a defensive
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
 * the open project's settings. `LoopEngine`'s own construction-time
 * config was deliberately left out of this wiring at the time - resolved
 * by the Loop Mode milestone below, which reworks that engine's whole
 * construction/lifecycle anyway.
 *
 * **As of `v0.Y.13.1` (Layers Panel):** a `LayersPanel` dock
 * (`layersPanel_`) shows the current project's layer stack, hidden until
 * setProject() is first called (matching the Landing Page's own
 * "nothing to show yet" treatment) and refreshed via refreshLayersPanel()
 * after any action that adds, removes, reorders, renames, or changes a
 * layer's visibility/opacity - including the panel's own signals, wired
 * to toggleLayerVisibility()/setLayerOpacity()/renameLayer()/
 * deleteLayer()/reorderLayers(). topmostLayerWithContent() now also
 * skips hidden layers - see its own docs.
 *
 * **As of `v0.Y.12.1` (Loop Mode, renamed from Live Mode):** the original
 * `LiveEngine` is renamed/reimplemented as `sound_mind::core::LoopEngine` -
 * see its own docs for the fixed-length loop-pedal redesign. `loopEngine_`
 * is no longer a fixed member constructed once with a default config: it's
 * a `std::unique_ptr`, `nullptr` until the first setProject() call, then
 * (re)constructed there from the *new* project's own
 * `streamCodecConfigFor()` config and its duration in samples
 * (`canvasWidth * hopLength`) - resolving the construction-time-config gap
 * `v0.Y.11.1`'s docs above flagged as this milestone's job. toggleLoopMode()
 * (renamed from toggleLiveMode()) creates a "Loop Input" layer (renamed
 * from "Live Input") the same way Live Mode's did; setKeepLooping()
 * forwards to `LoopEngine::setKeepLooping()` - the new "Freeze Loop"
 * (labeled "Keep Looping" until a later UI-wording pass renamed it, without
 * touching this method or the rest of its API) toolbar checkbox's actual
 * work. updateLoopLayer() (renamed from
 * updateLiveLayer()) additionally reports `LoopEngine::loopsBehind()` in
 * the status bar - the confirmed scope's "visible loop-delay indicator".
 *
 * **As of `v0.Y.16.1` (Transport Panels):** Play/Pause/Stop/Loop/Record
 * are no longer direct toolbar actions - each moved into its own new dock
 * panel (`playbackPanel_`/`recordPanel_`/`loopPanel_`), adapted from the
 * legacy Studio's own separate docks. The transport toolbar's three
 * remaining actions for these are pure show/hide toggles
 * (`QDockWidget::toggleViewAction()`), not transport controls themselves.
 * The "Freeze Loop" checkbox mentioned above moved from the toolbar into
 * `loopPanel_`. Real input/output device selection
 * (`setLoopInputDevice()`/`setLoopOutputDevice()`/`setRecordInputDevice()`/
 * `setPlaybackOutputDevice()`) and an above-unity Playback volume control
 * (`setPlaybackVolume()`) - all previously-deferred scope across Playback/
 * Live Mode/Record's own original milestones - land on these panels too.
 *
 * **As of `v0.Y.17.1` (Drag & Drop Import):** dropping local files onto
 * the window imports/opens them via handleDroppedFiles() - see its own
 * docs for the per-extension routing and why failures report through the
 * status bar rather than a blocking dialog.
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
     * even shown) while Loop Mode or Recording is active; otherwise, if
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
     * like newProject() (refuses outright while Loop Mode/Recording is
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

    /**
     * @brief Prompts for a WAV file and imports it as one or more new
     *        layers - see `docs/sound-mind-roadmap.md`'s Audio Import
     *        Snippets milestone (`v0.Y.19.1`).
     *
     * Audio longer than the current project's own duration is split into
     * project-length snippets first (see audioSnippetsForFile()'s docs).
     * With exactly one snippet (the common case - audio no longer than the
     * project), imports it directly, matching this method's previous,
     * simpler behavior exactly - no dialog beyond the file picker itself.
     * With more than one, shows an `AudioSnippetPickerDialog` (every
     * snippet checked by default) and imports only what's still checked
     * when it's accepted - cancelling it, or accepting with nothing
     * checked, imports nothing. Progress and completion are reported via
     * the status bar (non-modal) - see poolTopmostLayer()'s docs for why
     * only failure shows a modal.
     */
    void importAudio();

    /**
     * @brief Prompts for one or more image files, then how to scale them,
     *        and imports each as a new layer - see
     *        `docs/sound-mind-roadmap.md`'s Image Import Scaling
     *        (`v0.Y.20.1`) and Image Sequence Import (`v0.Y.22.1`)
     *        milestones.
     *
     * Always shows an `ImageScalePickerDialog` after the file(s) are
     * chosen - unlike Audio Import Snippets' picker, there's no "trivial,
     * skip the dialog" case here (every image import has a real scaling
     * choice to make). The dialog's "Import as sequence" checkbox is only
     * offered when more than one file was selected (see
     * `ImageScalePickerDialog`'s own constructor docs); cancelling the
     * dialog cancels the whole import. Progress and completion are
     * reported via the status bar (non-modal) - see poolTopmostLayer()'s
     * docs for why only failure shows a modal.
     */
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
     * Loop Mode or Recording is currently running (see toggleLoopMode()'s/
     * toggleRecording()'s docs for why all three are mutually exclusive in
     * this first pass).
     */
    void startPlayback();

    /// @brief Pauses playback; startPlayback() resumes from the same
    /// position. The position bar/playhead stay at their current position
    /// (not cleared) - stopPlayback() is what resets them.
    void pausePlayback();

    /// @brief Stops playback and rewinds to the beginning; resets the
    /// Playback panel's position bar/time label and clears the canvas
    /// playhead (`v0.0.21.1`, Playback position bar).
    void stopPlayback();

    /**
     * @brief Jumps playback to the given position - the actual work behind
     *        the Playback panel's position bar being dragged
     *        (`v0.0.21.1`).
     *
     * Does nothing if nothing is currently loaded for playback (no
     * startPlayback() has run yet, or stopPlayback() reset it) - there's
     * nothing to seek within. Updates the Playback panel's position bar/
     * label and the canvas playhead immediately, not just on the next
     * timer tick, so dragging while paused still shows the new position
     * right away.
     *
     * @param positionSeconds The position to seek to, in seconds; clamped
     *        to the loaded audio's own duration.
     */
    void seekPlayback(double positionSeconds);

    /**
     * @brief Switches `playbackController_` to the named output device,
     *        right now - the actual work behind the Playback panel's
     *        device picker (`v0.Y.16.1`).
     *
     * Forwards to `PlaybackEngine::setPreferredOutputDevice()` (via
     * `playbackController_`) - see its own docs for why this takes effect
     * immediately rather than on a later start(), unlike Loop/Record's
     * device pickers. Reports a failed switch via the status bar (the
     * previously open device, if any, stays open); does nothing silently
     * on success beyond the switch itself.
     *
     * @param deviceName The device to switch to, or empty for the system
     *        default.
     */
    void setPlaybackOutputDevice(const QString& deviceName);

    /**
     * @brief Sets `playbackController_`'s output gain - the actual work
     *        behind the Playback panel's volume slider (`v0.Y.16.1`).
     * @param percent `[0, PlaybackPanel::kMaxVolumePercent]` (200) - `100`
     *        is unity gain; above `100` is a real boost past it, per the
     *        confirmed scope for this milestone.
     */
    void setPlaybackVolume(int percent);

    /**
     * @brief Starts or stops Loop Mode (renamed from Live Mode): a fixed-
     *        length loop pedal that continuously captures the default input
     *        device one loop at a time into a newly created layer, encoding/
     *        decoding each completed loop and playing the previous loop's
     *        result back during the next (see
     *        `sound_mind::core::LoopEngine`'s docs for the full pipeline,
     *        including its confirmed real, structural playback latency).
     *
     * Per the confirmed scope for this milestone: the output is the loop
     * input alone, round-tripped through the Stream codec - not composited
     * with any other layer or project content yet (real multi-layer audio
     * mixing doesn't exist anywhere in the codebase yet - see LoopEngine's
     * own docs). Stops Playback first if it's running - both engines would
     * otherwise try to open the system's default output device
     * simultaneously through two independent JUCE device managers, which
     * isn't guaranteed to work depending on the platform/driver. Captures
     * into a Normal layer named "Loop Input" - **reusing one that already
     * exists in the project** (from an earlier session this run, or
     * reloaded from disk), rather than creating a new one every start, so
     * its previous content stays visible immediately on a restart instead
     * of the canvas going blank again while the new session's first loop
     * is still being captured (see LoopEngine's own docs for how long that
     * first loop alone can take - the whole project's duration). A
     * genuinely **new** layer instead gets LoopEngine::emptyImage() - a
     * silent placeholder at the right dimensions - as its starting content,
     * so the canvas shows an empty spectrogram immediately rather than
     * nothing at all, which otherwise reads as "Loop Mode isn't capturing
     * anything" (a real, confirmed point of confusion) rather than "Loop
     * Mode is running and has genuinely captured nothing *yet*". While
     * running, that layer's content refreshes from
     * LoopEngine::currentImage() on a timer and the canvas repaints, so
     * each completed loop's spectrogram visibly updates - stopping leaves
     * the layer's content as whatever was last captured, exactly like any
     * other layer. Does nothing (refuses to start) if Recording is
     * currently running - both would otherwise want the same input device
     * at once, through two independent JUCE device managers. Does nothing
     * at all (no-op, not even the refusal above) if no project is open,
     * since the loop length itself is derived from the project's own
     * duration - see LoopEngine's own docs.
     *
     * Marks hasUnsavedChanges() the moment the "Loop Input" layer is
     * added (starting) - per the Project Lifecycle milestone (`v0.Y.10.1`),
     * New/Open/Close all refuse outright while this is running, so the
     * capture itself is never at risk of being silently discarded by a
     * project switch.
     */
    void toggleLoopMode();

    /**
     * @brief Sets whether Loop Mode should keep replaying the last
     *        successfully captured take unchanged instead of recording
     *        over it - the actual work behind the Loop panel's "Keep
     *        Looping" checkbox (`v0.Y.16.1`; moved here from a transport
     *        toolbar checkbox `v0.Y.12.1` added as a confirmed stopgap).
     *
     * Also syncs `loopPanel_`'s own checkbox display (harmless/idempotent
     * when called *from* that checkbox's own signal) - so calling this
     * directly (a test, in particular) never leaves the panel showing a
     * stale state. Forwards to `LoopEngine::setKeepLooping()` - see its
     * own docs. Does nothing further if no project is open yet
     * (loopEngine_ doesn't exist until setProject() has been called at
     * least once) - harmless, since there's no running Loop Mode session
     * for the checkbox to affect in that case either.
     *
     * @param keepLooping The new state.
     */
    void setKeepLooping(bool keepLooping);

    /**
     * @brief Sets which input device Loop Mode's *next* start() should
     *        open - the actual work behind the Loop panel's input device
     *        picker (`v0.Y.16.1`).
     *
     * Forwards to `LoopEngine::setPreferredInputDevice()` - see its own
     * docs for why this only takes effect on the next start(), not
     * retroactively. Does nothing if no project is open yet.
     *
     * @param deviceName The device to prefer, or empty for the system
     *        default.
     */
    void setLoopInputDevice(const QString& deviceName);

    /// @brief Sets which output device Loop Mode's *next* start() should
    /// open - the actual work behind the Loop panel's output device
    /// picker (`v0.Y.16.1`). See setLoopInputDevice()'s docs for the same
    /// "next start()", "does nothing with no project open" behavior.
    /// @param deviceName The device to prefer, or empty for the system
    ///        default.
    void setLoopOutputDevice(const QString& deviceName);

    /**
     * @brief Starts or stops Recording: one-shot capture from the default
     *        input device into a newly created layer, encoded exactly as
     *        an imported file would be once capture stops (see
     *        `sound_mind::core::RecordEngine`'s docs for why this differs
     *        from Loop Mode's per-loop, whole-buffer-encoded pipeline).
     *
     * Per the confirmed scope for this milestone (mirroring Playback's and
     * Loop Mode's own precedent): defers an input gain control - named in
     * the design doc's Record section - as a UI affordance layered on top
     * of a working capture pipeline. A real input-device picker landed in
     * `v0.Y.16.1` - see setRecordInputDevice(). Stops Playback first if
     * it's running (same device-contention
     * reasoning as toggleLoopMode()); does nothing (refuses to start) if
     * Loop Mode is currently running, or if no project is open. Creates a
     * new Normal layer ("Recording") once capture stops and something was
     * actually captured; stopping with nothing captured (e.g. no input
     * device was available) leaves the project unchanged.
     *
     * Marks hasUnsavedChanges() when a layer is actually added. Per the
     * Project Lifecycle milestone (`v0.Y.10.1`), New/Open/Close all refuse
     * outright while Recording is running - see toggleLoopMode()'s docs.
     */
    void toggleRecording();

    /**
     * @brief Sets which input device Recording's *next* start() should
     *        open - the actual work behind the Record panel's input
     *        device picker (`v0.Y.16.1`).
     *
     * Forwards to `RecordEngine::setPreferredInputDevice()`.
     *
     * @param deviceName The device to prefer, or empty for the system
     *        default.
     */
    void setRecordInputDevice(const QString& deviceName);

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

    /**
     * @brief Sets whether the layer with the given id contributes to the
     *        project - the actual work behind `LayersPanel`'s visibility
     *        toggle.
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel. Does nothing if no layer with this id exists.
     *
     * @param id The layer to change.
     * @param visible The new visibility.
     */
    void toggleLayerVisibility(sound_mind::core::LayerId id, bool visible);

    /**
     * @brief Sets the opacity of the layer with the given id - the actual
     *        work behind `LayersPanel`'s opacity slider.
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel. Does nothing if no layer with this id exists.
     *
     * @param id The layer to change.
     * @param opacity The new opacity, intended to be in [0, 1].
     */
    void setLayerOpacity(sound_mind::core::LayerId id, float opacity);

    /**
     * @brief Sets the horizontal translation of the layer with the given
     *        id - the actual work behind `LayersPanel`'s translation spin
     *        box. See `docs/sound-mind-roadmap.md`'s Layer Time Alignment
     *        milestone (`v0.Y.21.1`).
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel - unlike setLayerOpacity(), this one's canvas refresh is
     * not a no-op: translation directly changes renderLayer()'s pixel
     * output (see its own docs), where opacity currently has no visible
     * effect at all (real multi-layer blending doesn't exist yet). Does
     * nothing if no layer with this id exists.
     *
     * @param id The layer to change.
     * @param translationColumns The new shift, in spectrogram columns - see
     *        `sound_mind::core::Layer::translationColumns()`'s docs.
     */
    void setLayerTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns);

    /**
     * @brief Sets the horizontal rescale of the layer with the given id -
     *        the actual work behind `LayersPanel`'s rescale spin box. See
     *        setLayerTranslation()'s docs for the milestone this belongs to
     *        and why this refreshes the canvas, unlike setLayerOpacity().
     *
     * @param id The layer to change.
     * @param rescaleFactor The new ratio - see
     *        `sound_mind::core::Layer::rescaleFactor()`'s docs.
     */
    void setLayerRescale(sound_mind::core::LayerId id, double rescaleFactor);

    /**
     * @brief Prompts for a new name and applies it - the actual work
     *        behind `LayersPanel`'s double-click-to-rename, split into an
     *        interactive slot (this one) and renameLayerTo() (the
     *        non-prompting testable core), the same shape as
     *        openProject()/openProjectAt().
     * @param id The layer to rename.
     */
    void renameLayer(sound_mind::core::LayerId id);

    /**
     * @brief Deletes the layer with the given id - the actual work behind
     *        `LayersPanel`'s delete button.
     *
     * No confirmation prompt (matching the legacy Studio's own row-level
     * delete button); refuses (no-op) for a `Background`/`Equalizer`
     * layer, or if no layer with this id exists - `LayersPanel` doesn't
     * even show a delete button for the two locked types, but this is
     * still enforced here too, the same defense-in-depth `setProject()`'s
     * own docs describe for its engine-stopping invariant. Marks
     * hasUnsavedChanges() and refreshes the canvas and Layers Panel on
     * success.
     *
     * @param id The layer to delete.
     */
    void deleteLayer(sound_mind::core::LayerId id);

    /**
     * @brief Reorders the current project's layer stack - the actual work
     *        behind `LayersPanel`'s drag-to-reorder.
     *
     * Delegates the actual validation to `Project::reorderLayers()` (a
     * no-op, `false` return, for anything other than a valid permutation
     * of the current layers' ids) - refreshes the Layers Panel either way,
     * since even a rejected reorder needs the panel snapped back to the
     * authoritative order (see `LayersPanel::reorderRequested()`'s docs).
     * Marks hasUnsavedChanges() only if the reorder actually applied.
     *
     * @param newOrderBottomToTop Every current layer's id, exactly once
     *        each, in the desired new bottom-to-top order.
     */
    void reorderLayers(const std::vector<sound_mind::core::LayerId>& newOrderBottomToTop);

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
     * while Loop Mode or Recording is active - the one guard this method
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
     *         or if Loop Mode/Recording is currently active.
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

    /**
     * @brief Renames the layer with the given id, without prompting - the
     *        actual work behind renameLayer(), split out for the same
     *        headless-testability reason as importAudioFile() (see its
     *        docs).
     *
     * Marks hasUnsavedChanges() and refreshes the Layers Panel on success.
     *
     * @param id The layer to rename.
     * @param newName The new name - an empty name is rejected.
     * @return `true` on success; `false` if no layer with this id exists,
     *         or `newName` is empty.
     */
    bool renameLayerTo(sound_mind::core::LayerId id, const QString& newName);

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
     * (import, Pool, a Loop/Recording capture adding or updating a layer)
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

    /// @brief Whether Loop Mode is currently capturing.
    /// @return The underlying LoopEngine's isRunning().
    [[nodiscard]] bool isLoopModeRunning() const noexcept;

    /// @brief The current "Freeze Loop" state - see setKeepLooping()'s
    /// docs.
    /// @return The underlying LoopEngine's keepLooping(), or `false` if no
    ///         project has ever been opened yet (loopEngine_ doesn't exist).
    [[nodiscard]] bool keepLooping() const noexcept;

    /// @brief Whether Recording is currently capturing.
    /// @return The underlying RecordEngine's isRecording().
    [[nodiscard]] bool isRecording() const noexcept;

    /// @brief The input device Loop Mode's next start() will prefer - see
    /// setLoopInputDevice(). Empty for the system default, or if no
    /// project has ever been opened yet.
    /// @return The preferred input device name.
    [[nodiscard]] QString loopInputDevice() const;

    /// @brief The output device Loop Mode's next start() will prefer -
    /// see setLoopOutputDevice(). Empty for the system default, or if no
    /// project has ever been opened yet.
    /// @return The preferred output device name.
    [[nodiscard]] QString loopOutputDevice() const;

    /// @brief The input device Recording's next start() will prefer - see
    /// setRecordInputDevice(). Empty means the system default.
    /// @return The preferred input device name.
    [[nodiscard]] QString recordInputDevice() const;

    /// @brief The current Playback output gain - see setPlaybackVolume().
    /// @return `1.0` is unity; the underlying PlaybackEngine's volume().
    [[nodiscard]] float playbackVolume() const noexcept;

    /**
     * @brief Imports every project-length snippet of a WAV file as new
     *        layers, without prompting or showing an error dialog on
     *        failure.
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
     * Equivalent to calling importAudioSnippets() with every index
     * audioSnippetsForFile() reports - i.e. the legacy Studio's own
     * "import everything, no picker" behavior. Audio no longer than the
     * project's own duration always has exactly one snippet, so this stays
     * a plain single-layer import in the common case, named from the
     * file's own name exactly as before this milestone.
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
     * @brief Computes how the audio file at `path` would split into the
     *        current project's own duration worth of snippets, without
     *        importing anything or showing any dialog.
     *
     * A snippet's length is the same `canvasWidth * hopLength` samples
     * `sound_mind::core::LoopEngine`'s own loop length is derived from
     * (see its docs) - snippet `0` is the source's first such stretch,
     * snippet `1` the next, and so on; the final snippet is shorter than
     * the rest if the source's length isn't an exact multiple. Audio no
     * longer than one snippet's worth always returns exactly one entry.
     *
     * @param path Path to the WAV file to analyze.
     * @param errorMessage If non-null and this returns empty, set to a
     *        human-readable description of what went wrong.
     * @return One entry per snippet, in order; empty if no project is
     *         open or the file couldn't be read.
     */
    [[nodiscard]] std::vector<AudioSnippetPickerDialog::RowData> audioSnippetsForFile(
        const std::filesystem::path& path, QString* errorMessage = nullptr) const;

    /**
     * @brief Imports specific snippets (see audioSnippetsForFile()) of an
     *        audio file as new layers, without prompting or showing an
     *        error dialog on failure - the actual work behind both
     *        importAudioFile() (which requests every snippet) and
     *        importAudio()'s snippet picker (which requests only the
     *        checked subset).
     *
     * Each imported snippet becomes its own new Normal layer. With more
     * than one snippet in the source overall, a layer's name is
     * `"<stem>_NNNN"` (the source file's stem, an underscore, and its
     * snippet index zero-padded to four digits) - matching the legacy
     * Studio's own `name_0000`/`name_0001`/... convention - so a layer's
     * name still reflects its real position in the source even if some
     * snippets were skipped. With only one snippet in the source overall,
     * the layer is named from the file's own name directly, matching
     * importAudioFile()'s pre-existing single-layer behavior exactly.
     *
     * @param path Path to the WAV file to import from.
     * @param snippetIndices Which of the source's snippets to import, in
     *        any order and with any duplicates ignored; an index at or
     *        beyond the source's actual snippet count is silently
     *        skipped, not an error.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` if the file was read and at least one requested
     *         snippet was imported; `false` if the file couldn't be read,
     *         no project is open, or nothing was actually imported (an
     *         empty `snippetIndices`, or every given index out of range).
     */
    bool importAudioSnippets(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                              QString* errorMessage = nullptr);

    /**
     * @brief Imports an image file as a new layer, resized per `mode`,
     *        without prompting or showing an error dialog on failure.
     *
     * The image is first resized to the project's canvas dimensions
     * according to `mode` (see `ImageScalePickerDialog::Mode`'s own docs
     * for exactly what each value does), then its RGB pixels are converted
     * into amplitude/phase data via `sound_mind::codec::fromRgbImage()` -
     * per `docs/sound-mind-design.md`'s "sound and image are one
     * continuous surface" principle, an imported image is genuinely
     * unified with audio-imported content, not a picture with no
     * underlying sound representation. See importAudioFile()'s docs for
     * why this never shows a message box itself.
     *
     * Marks hasUnsavedChanges() on success, same as importAudioFile().
     *
     * @param path Path to the image file to import.
     * @param mode How to resize the image before importing it.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if loading or converting it failed.
     */
    bool importImageFile(const std::filesystem::path& path, ImageScalePickerDialog::Mode mode,
                          QString* errorMessage = nullptr);

    /**
     * @brief Imports several image files at once - the multi-file testable
     *        core behind `importImage()`'s file dialog - see
     *        `docs/sound-mind-roadmap.md`'s Image Sequence Import milestone
     *        (`v0.Y.22.1`).
     *
     * When `importAsSequence` is `false`, every path is imported
     * independently via importImageFile(), each with `mode` and no
     * translation (`translationColumns()` stays `0`) - equivalent to
     * calling importImageFile() once per path. When `true`, `mode` is
     * ignored entirely: every file is imported with
     * `ImageScalePickerDialog::Mode::ScaleVerticalProportional`, sorted by
     * path first (deterministic - the natural choice for numbered frame
     * sequences), and given a cumulative `translationColumns()` so each
     * layer starts immediately after the previous one's own (proportional)
     * width ends - matching the legacy Studio's own cumulative-offset
     * placement, including its wrap-back-to-`0` behavior once the running
     * total reaches the project's own `canvasWidth` (confirmed with the
     * user before implementing, over the alternative of just letting later
     * layers keep extending past `canvasWidth` - renderLayer() would have
     * cropped those anyway, but the wrap was chosen to match the legacy
     * behavior exactly rather than deviate from it here).
     *
     * A failure importing one file doesn't stop the rest - matching
     * importAudioSnippets()'s and handleDroppedFiles()'s own "don't let one
     * bad file block everything" precedent; this only returns `false` if
     * *nothing* was imported.
     *
     * @param paths The image files to import.
     * @param mode How to resize each image - ignored when
     *        `importAsSequence` is `true`.
     * @param importAsSequence Whether to lay the files out end-to-end in
     *        time instead of importing each independently.
     * @param errorMessage If non-null and this returns `false`, set to the
     *        first failure's own message.
     * @return `true` if at least one file was imported; `false` if no
     *         project is open, `paths` is empty, or every file failed.
     */
    bool importImageFiles(const std::vector<std::filesystem::path>& paths, ImageScalePickerDialog::Mode mode,
                           bool importAsSequence, QString* errorMessage = nullptr);

    /**
     * @brief Routes a list of dropped local file paths to the matching
     *        import/open method by extension - the actual work behind
     *        dropEvent(), split out so it's callable directly by a test
     *        without needing a real OS-level drag gesture (which nothing
     *        can simulate headlessly) - see
     *        `docs/sound-mind-roadmap.md`'s Drag & Drop Import milestone
     *        (`v0.Y.17.1`).
     *
     * `.wav` goes to importAudioSnippets() with whatever indices
     * `audioSnippetSelections` gives that path, or - for a path with no
     * entry there - the same "every computed snippet, no picker" behavior
     * importAudioFile() always had; every image extension
     * `importImageFiles()` accepts is collected and imported as one batch
     * with `imageMode`/`importAsSequence`. dropEvent() is the one that
     * actually decides all of this (via real `AudioSnippetPickerDialog`/
     * `ImageScalePickerDialog` prompts, the same ones File → Import Audio/
     * Image themselves show - confirmed with the user: a drop should offer
     * the same choices those menu actions do), so this method itself stays
     * non-prompting; `.smproj` goes to openProjectAt(), guarded by
     * confirmDiscardUnsavedChanges() first - openProjectAt() itself already
     * refuses (no dialog) while Loop Mode or Recording is active. Every
     * other extension is silently ignored, not an error - a stray file
     * dropped by accident shouldn't force anything onto the screen.
     *
     * A recognized file that fails to import or open reports it via the
     * status bar (non-modal), not a blocking dialog - deliberately gentler
     * than the equivalent File menu actions, both because a multi-file drop
     * shouldn't stop and demand attention partway through, and because it
     * keeps this method itself unconditionally headless-testable (the one
     * exception is `.smproj` with genuine unsaved changes to confirm - the
     * same real, interactive-gesture-only exception every
     * confirmDiscardUnsavedChanges()-guarded call site already has).
     *
     * @param paths The local file paths to route, in order.
     * @param imageMode How to resize any image files among `paths` - see
     *        `importImageFiles()`'s own docs; ignored if `paths` has none.
     *        Defaults to the same `RescaleToFitProject` the interactive
     *        picker itself pre-selects, so a caller that doesn't care about
     *        this (every test predating this parameter, in particular)
     *        gets the same result as before.
     * @param importImagesAsSequence Whether to lay out any image files
     *        among `paths` end-to-end in time instead of importing each
     *        independently - see `importImageFiles()`'s own docs. Defaults
     *        to `false`.
     * @param audioSnippetSelections Which snippet indices to import for a
     *        given `.wav` path among `paths` - see
     *        `audioSnippetsForFile()`/`importAudioSnippets()`'s own docs. A
     *        `.wav` path with no entry here imports every snippet it has,
     *        the pre-existing default every test predating this parameter
     *        still gets.
     */
    void handleDroppedFiles(
        const std::vector<std::filesystem::path>& paths,
        ImageScalePickerDialog::Mode imageMode = ImageScalePickerDialog::Mode::RescaleToFitProject,
        bool importImagesAsSequence = false,
        const std::map<std::filesystem::path, std::vector<std::size_t>>& audioSnippetSelections = {});

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
     *        Loop Mode or Recording is active; otherwise prompts via
     *        confirmDiscardUnsavedChanges() if there are unsaved changes,
     *        ignoring the event if the user cancels.
     * @param event The close event; accepted or ignored per the above.
     */
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief Accepts a drag carrying at least one local file - see
     *        `docs/sound-mind-roadmap.md`'s Drag & Drop Import milestone
     *        (`v0.Y.17.1`).
     * @param event The drag-enter event; accepted or ignored per the above.
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief Extracts every local file `event` carries, prompts for
     *        whatever choices images/multi-snippet audio among them would
     *        need from the matching File menu action, and routes all of
     *        them via handleDroppedFiles() - see its own docs for the
     *        actual per-extension behavior.
     *
     * A drop is meant to offer exactly the same choices File → Import
     * Audio/Image would, confirmed with the user:
     *
     * - If `event` carries at least one image file, a single
     *   `ImageScalePickerDialog` is shown once for the whole batch (with
     *   its "Import as sequence" checkbox offered exactly when more than
     *   one image was dropped, same as `importImage()`'s own file dialog).
     * - Each `.wav` file among `event`'s files gets its own
     *   `audioSnippetsForFile()` check; one with more than one snippet
     *   shows its own `AudioSnippetPickerDialog`, exactly as
     *   `importAudio()` would for that file alone - per-file, not batched,
     *   since (unlike images) audio snippets aren't a cross-file concept.
     *
     * **Cancelling any one of these dialogs cancels the whole drop**,
     * including every other file carried alongside it (images, other
     * audio, `.smproj`) - confirmed with the user over the alternative of
     * only skipping whatever that one dialog was for and still processing
     * the rest.
     *
     * Untestable directly, like dragEnterEvent() - nothing can simulate a
     * real OS-level drag gesture headlessly, and a real modal dialog would
     * hang a headless test the same way every other undismissable dialog
     * in this codebase's own test suite already does (see
     * `docs/sound-mind-architecture.md`'s Decisions Made). The routing
     * logic this delegates to, and the choices themselves, are
     * independently tested via handleDroppedFiles()'s own optional
     * parameters and `ImageScalePickerDialog`'s/`AudioSnippetPickerDialog`'s
     * own test suites.
     *
     * @param event The drop event; accepted if it carried at least one
     *        local file, ignored otherwise.
     */
    void dropEvent(QDropEvent* event) override;

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

    /// @brief The topmost *visible* layer with content, if any - the same
    /// notion of "the composite" startPlayback(), CanvasWidget, and
    /// poolTopmostLayer() all share for now (see their docs). As of the
    /// Layers Panel milestone (`v0.Y.13.1`), a layer hidden via
    /// toggleLayerVisibility() is skipped here, same as one with no
    /// content at all.
    /// @return A mutable pointer to that layer, or `nullptr` if none
    ///         qualifies, or no project is open.
    [[nodiscard]] sound_mind::core::Layer* topmostLayerWithContent();

    /// @brief The layer with the given id, if the current project has one.
    /// @return A mutable pointer to that layer, or `nullptr` if no project
    ///         is open or no layer in it has this id.
    [[nodiscard]] sound_mind::core::Layer* layerById(sound_mind::core::LayerId id);

    /// @brief Sets the window title to "Sound Mind Studio v<version>",
    /// plus " - <project name>" (currentPath_'s own file stem) once a
    /// project has been saved/opened at a real path - called after every
    /// currentPath_ assignment (createProjectAt()/openProjectAt()/
    /// saveProjectAs()).
    void updateWindowTitle();

    /// @brief Pushes the current project's layer stack into layersPanel_ -
    /// called after setProject() and after any action that adds, removes,
    /// reorders, renames, or changes a layer's visibility/opacity. An
    /// empty list (not a no-op) when no project is open.
    void refreshLayersPanel();

    /// @brief loopUpdateTimer_'s slot: refreshes the Loop layer's content
    /// from loopEngine_->currentImage() and repaints the canvas, and shows
    /// loopEngine_->loopsBehind() in the status bar, while Loop Mode is
    /// running - see toggleLoopMode()'s docs.
    void updateLoopLayer();

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

    /// @brief The Layers Panel dock - hidden until setProject() is first
    /// called (see refreshLayersPanel()'s docs), then shown automatically
    /// exactly once (see layersPanelShownOnce_) and left to the toolbar's
    /// own toggleViewAction() (set up in the constructor) after that.
    LayersPanel* layersPanel_ = nullptr;

    /// @brief Whether layersPanel_ has already been auto-shown once this
    /// session - setProject() only forces it visible the *first* time any
    /// project exists, so a later manual hide (via the toolbar toggle)
    /// isn't overridden by switching to a different project. See
    /// setProject()'s own comment.
    bool layersPanelShownOnce_ = false;

    /// @brief The Playback/Record/Loop transport panels (`v0.Y.16.1`) -
    /// each hidden until setProject() is first called, and - unlike
    /// layersPanel_ - never force-shown afterward either: they start OFF
    /// by default and stay however the user last left them via the
    /// transport toolbar's own toggleViewAction() (set up in the
    /// constructor), confirmed with the user.
    PlaybackPanel* playbackPanel_ = nullptr;
    RecordPanel* recordPanel_ = nullptr;
    LoopPanel* loopPanel_ = nullptr;

    /// @brief Backing store for recentProjects_ - an ini-format file (not
    /// the platform registry/native format) specifically so
    /// QStandardPaths::setTestModeEnabled(true) (see tests/main.cpp) can
    /// sandbox it during automated tests, without touching whatever a real
    /// installed Studio has persisted on the same machine.
    QSettings settings_{QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SoundMind"),
                         QStringLiteral("SoundMindStudio")};
    RecentProjects recentProjects_{settings_};

    /// @brief Owns the PlaybackEngine and its position-polling timer -
    /// extracted from a plain member + free-standing timer/flag as part of
    /// the Phase 2.5 Refactor & Clean Up milestone (`v0.Y.23.1`). "Which
    /// layer to play" (topmostLayerWithContent()) and wiring its signals to
    /// playbackPanel_/canvas_ both stay MainWindow's own job - see
    /// PlaybackController's own docs for why it doesn't know about either.
    PlaybackController* playbackController_ = nullptr;

    /// @brief `nullptr` until the first setProject() call - LoopEngine
    /// needs a real loop length (the project's own duration in samples)
    /// and codec config at construction time, so it's (re)constructed
    /// fresh in setProject() for whatever project is current, rather than
    /// being a fixed member built once before any project exists - see the
    /// class docs' `v0.Y.12.1` note for why this replaced the original
    /// `LiveEngine` member's simpler, always-default-constructed shape.
    std::unique_ptr<sound_mind::core::LoopEngine> loopEngine_;
    QTimer* loopUpdateTimer_ = nullptr;

    /// @brief The layer currently being captured into, while Loop Mode is
    /// running - std::nullopt otherwise. An id, not a Layer*, since
    /// Project::layers() is a std::vector<Layer> that addLayer() (or
    /// future layer-list operations) can reallocate, invalidating a raw
    /// pointer held across such a call - see layerById().
    std::optional<sound_mind::core::LayerId> loopLayerId_;

    sound_mind::core::RecordEngine recordEngine_{sound_mind::codec::StreamCodecConfig{}.sampleRateHz};
    QTimer* recordDrainTimer_ = nullptr;
};

}  // namespace sound_mind::studio
