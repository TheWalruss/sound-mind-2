#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <QColor>
#include <QMainWindow>
#include <QSettings>
#include <QUrl>

#include "sound_mind/codec/audio_export.h"
#include "sound_mind/core/background_task.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/device_test_tone_player.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/loop_engine.h"
#include "sound_mind/core/pooling.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/record_engine.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/chord_generator_panel.h"
#include "sound_mind/studio/configure_devices_panel.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/grid_panel.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/layer_controller.h"
#include "sound_mind/studio/mind_wave_controller.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/recent_projects.h"
#include "sound_mind/studio/selection_configuration_panel.h"
#include "sound_mind/studio/tool_configuration_panel.h"
#include "sound_mind/studio/tool_palette_controller.h"
#include "sound_mind/studio/undo_stack.h"

class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QString;
class QTimer;

namespace sound_mind::studio {

class ComposerPanel;
class CreateProjectWizard;
class HistoryPanel;
class LandingPage;
class LayersPanel;
class LoopPanel;
class MindWavesPanel;
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
 * to cycleLayerVisibilityState()/setLayerOpacity()/renameLayer()/
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
 * `loopPanel_`. Real input/output device selection and an above-unity
 * Playback volume control (`setPlaybackOutputDevice()`/
 * `setPlaybackVolume()`) - all previously-deferred scope across Playback/
 * Live Mode/Record's own original milestones - landed on these panels
 * too, each with its own local device picker at first; those per-panel
 * pickers were later removed once Configure Devices (`v0.0.42.1`)
 * consolidated them into one shared picker (real-world testing pass,
 * 2026-09-20, finding #7).
 *
 * **As of `v0.Y.17.1` (Drag & Drop Import):** dropping local files onto
 * the window imports/opens them via handleDroppedFiles() - see its own
 * docs for the per-extension routing and why failures report through the
 * status bar rather than a blocking dialog.
 *
 * **Phase 3 (Basic Painting through Filter Layers, `v0.Y.24.1`-`v0.Y.28.1`):**
 * the largest single expansion of this class's own responsibility so far -
 * a `toolConfigurationPanel_`/`gridPanel_` pair configuring whichever tool
 * is active, and `filterConfigurationPanel_` (paired with
 * `handleLayerSelectionChanged()`/`applyFilterConfiguration()`) for the
 * Filter/Equalizer layer types multi-layer compositing and Filter Layers
 * introduced. Per-milestone paragraphs weren't added here for each of
 * these individually (unlike every phase above) - `docs/sound-mind-
 * roadmap.md`'s own `v0.Y.29.1` (Refactor & Clean Up) instead identified
 * most of this cluster (the Paint/Pick/Select/Path tool controllers
 * themselves) as an extraction candidate, the same "own presentation"
 * treatment `LayersPanel`/`LoopPanel`/`RecordPanel`/`PlaybackPanel`
 * already received.
 *
 * **As of `v0.Y.29.1` (Refactor & Clean Up, Installment C):** that
 * extraction landed - `toolPaletteController_` now owns all four tool
 * controllers (`PaintController`/`PickController`/`SelectionController`/
 * `PathController`) and every signal wiring between them and
 * `canvas_`/`toolConfigurationPanel_` - see its own class docs. This
 * class's own remaining job for the tool palette is narrower than it
 * looks from the sheer number of still-present delegating methods
 * (`undo()`, `deletePickedObject()`, `fillSelectionWith()`, `paste()`,
 * ...): resolving "which layer" a freehand gesture starting right now
 * targets and managing the four toolbar `QAction`s' own mutual
 * exclusivity (`setExclusiveToolMode()`) - a toolbar/menu concern, not a
 * tool-palette one. Every one of those delegating methods kept its exact
 * pre-extraction signature, now a thin forwarding body - matching the
 * same "unchanged public surface" precedent `v0.Y.23.1`'s own
 * `PlaybackController`/import_export extractions already set.
 *
 * **As of `v0.Y.29.1` (Refactor & Clean Up, Installment D):**
 * `layerController_` now owns layer-stack lookup (`layerById()`,
 * `topmostLayerWithContent()`, `paintTargetLayerId()`), mutation
 * (`cycleLayerVisibilityState()`, `setLayerOpacity()`, ... `reorderLayers()`),
 * and Layers Panel/Filter Configuration Panel refresh - see its own class
 * docs. `paintTargetLayerId()` - the "which layer" resolution the
 * previous paragraph names - now lives there too, called via
 * `layerController_->paintTargetLayerId()` at each of the same call
 * sites; `MainWindow` itself no longer implements it. Every public
 * method delegating to it kept its exact signature, the same "unchanged
 * public surface" precedent as Installment C.
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
    /// @param audioDeviceMode Threaded through to every real-device-capable
    ///        member constructed here or later in setProject()
    ///        (`playbackController_`, `recordEngine_`, `loopEngine_`) -
    ///        `Real` (the default) attaches each to the system's actual
    ///        audio hardware, same as leaving this argument off entirely.
    ///        `None` exists specifically for tests: previously every
    ///        `MainWindow` - regardless of what it actually tested - opened
    ///        and closed two real system audio devices (one in, one out) on
    ///        construction alone, since `recordEngine_`/`playbackController_`
    ///        both defaulted to `AudioDeviceMode::Real` with no way for
    ///        `MainWindow` to say otherwise. `sound-mind-studio-tests`'s own
    ///        `TestMainWindow` helper (`test_main_window.cpp`) passes `None`
    ///        here for exactly this reason - see its own docs.
    explicit MainWindow(QWidget* parent = nullptr,
                         sound_mind::core::AudioDeviceMode audioDeviceMode = sound_mind::core::AudioDeviceMode::Real);

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
     * @brief Prompts for an audio file (see `kAudioFileFilter` - WAV, MP3,
     *        FLAC, Ogg, AIFF, M4A, or Opus, since `v0.0.42.3`) and imports
     *        it as one or more new layers - see
     *        `docs/sound-mind-roadmap.md`'s Audio Import Snippets
     *        milestone (`v0.Y.19.1`).
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
     * @brief Starts (or resumes) playback of the project's own real
     *        composite.
     *
     * As of `v0.Y.27.1` (Multi-layer Compositing): every visible layer
     * mixed together via `sound_mind::core::compositeProject()`, not just
     * whichever layer happens to be topmost - see that function's own
     * docs. Decodes and loads the composite's audio once (not on every
     * call - resuming after pausePlayback() continues from the same
     * position); does nothing if the composite is empty (no visible layer
     * has content), or no project is open, or Loop Mode or Recording is
     * currently running (see toggleLoopMode()'s/toggleRecording()'s docs
     * for why all three are mutually exclusive).
     *
     * As of `v0.0.45.20` (finding #12, Installment H): a fresh (not yet
     * loaded) start runs `compositeProject()` on a `BackgroundTask`
     * instead of blocking the UI thread - the one call site among
     * `compositeProject()`'s three (this one, the live canvas redraw, and
     * Repeat/Delta/Review's per-edit recomposite) that's a genuine
     * one-shot "click and wait" action rather than part of the
     * interactive edit/render loop; the other two stay synchronous (see
     * `docs/sound-mind-architecture.md`'s "Threading & Real-Time Model"
     * for why). A copy of `*project_` is captured by value before
     * backgrounding, so the background thread never touches the live
     * `project_` - the same "compute independently, commit on the UI
     * thread only once fully successful" shape every other Finding #12
     * installment already uses. Cancelling rolls back cleanly:
     * `playbackController_` was never touched, so there's nothing to
     * undo, just a discarded, uncommitted result - the same trivial
     * rollback importAudioSnippetsAsync() already established. A no-op
     * (status bar message only) if isCompositingForPlayback() is already
     * `true`. Resuming after pausePlayback() (already loaded) stays fully
     * synchronous, unchanged - only a *fresh* start composites at all.
     */
    void startPlayback();

    /// @brief Whether startPlayback()'s own background composite is still
    /// running.
    /// @return `true` from startPlayback() (once it actually started a
    ///         background task for a fresh load) until the background
    ///         compute finishes, one way or another.
    [[nodiscard]] bool isCompositingForPlayback() const noexcept;

    /// @brief Requests cancellation of the currently running playback
    /// composite - the actual work behind the status bar's own cancel
    /// button. A no-op if isCompositingForPlayback() is `false`.
    void cancelPlaybackComposite();

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
     *        behind Configure Devices' own output gain slider (originally
     *        the Playback panel's own volume slider, `v0.Y.16.1`, before
     *        that picker was removed - real-world testing pass,
     *        2026-09-20, finding #7).
     * @param percent `[0, ConfigureDevicesPanel::kMaxGainPercent]` (200) -
     *        `100` is unity gain; above `100` is a real boost past it, per
     *        the confirmed scope for this milestone.
     */
    void setPlaybackVolume(int percent);

    /**
     * @brief Sets whether GPU-accelerated compute is enabled - the actual
     *        work behind the View menu's own Hardware Acceleration
     *        checkable action (`v0.0.42.5`, Workflow & Device Polish,
     *        Installment E).
     *
     * Persists the choice via `settings_` (the same ini-format store
     * `recentProjects_` already uses, restored on every later `MainWindow`
     * construction - not a session-only diagnostic switch, confirmed with
     * the user), forwards to
     * `sound_mind::core::setHardwareAccelerationEnabled()` (the real flag
     * every GPU-accelerated call site in `sound-mind-core` already checks
     * - `compositeProject()`'s per-layer mixing, `applyFilter()`'s
     * `UniformBlur` case), and repaints the canvas immediately so flipping
     * it has an instantly visible (or, for a correctness check, instantly
     * *invisible* - the whole point) effect to compare, per
     * `docs/sound-mind-design.md`'s own "does it go faster, are the
     * results the same" framing.
     *
     * @param enabled `true` to allow the GPU path (still falling back to
     *        CPU if no real device is available); `false` to force every
     *        GPU-accelerated call site onto its own CPU implementation.
     */
    void setHardwareAccelerationEnabled(bool enabled);

    /**
     * @brief Sets whether Repeat Playback is active - the actual work
     *        behind the Playback panel's own Repeat checkbox (`v0.0.42.2`,
     *        Workflow & Device Polish, Installment B).
     *
     * Does *not* gate whether an edit re-renders and jumps under Delta/
     * Review scope - see `handleContentChangedForPlayback()`'s own docs on
     * why that's Scope's job alone, not this flag's. What this *does* gate
     * is only what happens once the active range's own end is reached:
     * loops back to its own start (`checkRepeatPlaybackRange()`) while
     * `true`, halts there while `false` - and, for `Track` scope
     * specifically, whether an edit re-renders in place at all (`Track` has
     * no narrower region to preview on its own, so with Repeat off it does
     * nothing). Turning Repeat off immediately widens the active range
     * back to the whole track (`[0, totalSeconds()]`), so nothing about a
     * stale Delta/Review range lingers once it's unchecked.
     *
     * @param enabled The new state.
     */
    void setPlaybackRepeat(bool enabled);

    /// @brief Sets which portion of the track Repeat Playback targets -
    /// the actual work behind the Playback panel's own Scope combo
    /// (`v0.0.42.2`). Only takes effect on the *next* canvas edit or
    /// natural loop-around - see `PlaybackScope`'s own docs.
    /// @param scope The new scope.
    void setPlaybackScope(sound_mind::studio::PlaybackScope scope);

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
     * `v0.Y.16.1`, since consolidated into Configure Devices'
     * `setConfiguredInputDevice()` (real-world testing pass, 2026-09-20,
     * finding #7). Stops Playback first if
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
     * @brief Re-queries available input/output devices and refreshes
     *        `configureDevicesPanel_`'s own device pickers - the only ones
     *        left in the app (real-world testing pass, 2026-09-20, finding
     *        #7) - the actual work behind its own "Refresh Devices" button
     *        (`v0.0.42.1`). Never opens or closes a device itself - see
     *        `sound_mind::core::availableAudioDeviceNames()`'s own docs
     *        for how a fresh scan already works without one.
     */
    void refreshConfiguredDevices();

    /**
     * @brief Sets the input device `recordEngine_`'s and `loopEngine_`'s
     *        (if a project is open) *next* start() should each open - the
     *        actual work behind the Configure Devices panel's own input
     *        device picker (`v0.0.42.1`) - now the only input device
     *        picker in the app (real-world testing pass, 2026-09-20,
     *        finding #7; see `ConfigureDevicesPanel`'s own class docs on
     *        why this applies to both engines at once).
     * @param deviceName The device to prefer, or empty for the system
     *        default.
     */
    void setConfiguredInputDevice(const QString& deviceName);

    /// @brief Sets the output device `playbackController_`'s and
    /// `loopEngine_`'s (if a project is open) should each switch to - the
    /// actual work behind the Configure Devices panel's own output device
    /// picker (`v0.0.42.1`) - now the only output device picker in the app
    /// (real-world testing pass, 2026-09-20, finding #7). See
    /// setConfiguredInputDevice()'s own docs for the same "applies to both
    /// engines at once" reasoning.
    /// @param deviceName The device to prefer, or empty for the system
    ///        default.
    void setConfiguredOutputDevice(const QString& deviceName);

    /// @brief Sets `recordEngine_`'s and `loopEngine_`'s (if a project is
    /// open) own input gain - the actual work behind the Configure Devices
    /// panel's own input gain slider (`v0.0.42.1`).
    /// @param percent `[0, ConfigureDevicesPanel::kMaxGainPercent]` (200) -
    ///        `100` is unity gain.
    void setConfiguredInputGain(int percent);

    /// @brief Sets `playbackController_`'s own output gain - the actual
    /// work behind the Configure Devices panel's own output gain slider
    /// (`v0.0.42.1`). Forwards straight to setPlaybackVolume() (the same
    /// underlying `PlaybackController::setVolume()`) - `PlaybackPanel`'s
    /// own, now-removed volume slider used to need syncing here too (real-
    /// world testing pass, 2026-09-20, finding #7: it was a pure duplicate
    /// of this same gain).
    /// @param percent `[0, ConfigureDevicesPanel::kMaxGainPercent]` (200).
    void setConfiguredOutputGain(int percent);

    /**
     * @brief Starts or stops testing the currently configured input
     *        device - the actual work behind the Configure Devices
     *        panel's own input "Test" toggle (`v0.0.42.1`).
     *
     * Starting opens `deviceTestRecordEngine_` (never `recordEngine_`
     * itself - see its own docs) against whatever device
     * `configureDevicesPanel_`'s own input picker currently names, and
     * starts `testInputLevelTimer_` polling its `currentInputLevel()` into
     * the panel's own level meter; stopping does the reverse.
     *
     * @param testing `true` to start testing, `false` to stop.
     */
    void toggleTestInputDevice(bool testing);

    /**
     * @brief Starts or stops testing the currently configured output
     *        device - the actual work behind the Configure Devices
     *        panel's own output "Test" toggle (`v0.0.42.1`).
     *
     * Forwards to `deviceTestTonePlayer_`'s own start()/stop(), against
     * whatever device `configureDevicesPanel_`'s own output picker
     * currently names.
     *
     * @param testing `true` to start playing the test tone, `false` to
     *        stop.
     */
    void toggleTestOutputDevice(bool testing);

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
     * @brief Cycles the layer with the given id through its own 3-way
     *        visibility state (Visible -> Muted -> Invisible) - the actual
     *        work behind `LayersPanel`'s visibility button, `v0.Y.46.1`
     *        Installment B ("Layers Panel & Editing Enhancements v2"). See
     *        `LayerController::cycleLayerVisibilityState()`'s own docs for
     *        the exact states.
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel. Does nothing if no layer with this id exists.
     *
     * @param id The layer to cycle.
     */
    void cycleLayerVisibilityState(sound_mind::core::LayerId id);

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
     * @brief Sets the stereo balance of the layer with the given id - the
     *        actual work behind `LayersPanel`'s balance slider, `v0.Y.46.1`
     *        Installment C ("Per-layer balance").
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel, the same as setLayerOpacity(). Does nothing if no
     * layer with this id exists.
     *
     * @param id The layer to change.
     * @param balance The new balance, intended to be in [0, 1].
     */
    void setLayerBalance(sound_mind::core::LayerId id, float balance);

    /**
     * @brief Sets (or clears) which MindWave the layer with the given id's
     *        own opacity is bound to - the actual work behind
     *        `LayersPanel`'s per-row opacity-MindWave combo
     *        (`v0.Y.31.1` Installment C2).
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel, the same as setLayerOpacity() - delegates entirely to
     * `LayerController::setLayerOpacityMindWave()`.
     *
     * @param id The layer to change.
     * @param mindWaveId The new binding, or `std::nullopt` to unbind.
     */
    void setLayerOpacityMindWave(sound_mind::core::LayerId id, std::optional<sound_mind::core::MindWaveId> mindWaveId);

    /**
     * @brief Sets the horizontal translation of the layer with the given
     *        id - the actual work behind `LayersPanel`'s translation spin
     *        box. See `docs/sound-mind-roadmap.md`'s Layer Time Alignment
     *        milestone (`v0.Y.21.1`).
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel, the same as setLayerOpacity() - translation directly
     * changes both renderLayer()'s pixel output (see its own docs) and
     * compositeProject()'s own placement of this layer. Does nothing if no
     * layer with this id exists.
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
     *        and for why this refreshes the canvas, the same as
     *        setLayerOpacity().
     *
     * @param id The layer to change.
     * @param rescaleFactor The new ratio - see
     *        `sound_mind::core::Layer::rescaleFactor()`'s docs.
     */
    void setLayerRescale(sound_mind::core::LayerId id, double rescaleFactor);

    /**
     * @brief Sets the blend mode of the layer with the given id - the
     *        actual work behind `LayersPanel`'s per-row Blend Mode combo
     *        (`v0.Y.37.1`, Deferred Blend Modes).
     *
     * Marks hasUnsavedChanges() and refreshes both the canvas and the
     * Layers Panel, the same as setLayerOpacity() - delegates entirely to
     * `LayerController::setLayerBlendMode()`.
     *
     * @param id The layer to change.
     * @param mode The new blend mode.
     */
    void setLayerBlendMode(sound_mind::core::LayerId id, sound_mind::core::BlendMode mode);

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
     * @brief Duplicates the layer with the given id - the actual work
     *        behind `LayersPanel`'s duplicate button, real-world testing
     *        pass finding #22.
     *
     * A full copy - content, opacity (and its own bound MindWave, if
     * any), blend mode, translation/rescale, and filter configuration all
     * carry over unchanged - see `LayerController::duplicateLayer()`'s own
     * docs for the exact mechanism. Refuses (no-op) for a `Background`/
     * `Equalizer` layer, or if no layer with this id exists, the same
     * restriction deleteLayer() gives. Marks hasUnsavedChanges() and
     * refreshes the canvas and Layers Panel on success, selecting the new
     * copy immediately.
     *
     * @param id The layer to duplicate.
     */
    void duplicateLayer(sound_mind::core::LayerId id);

    /**
     * @brief Cleans up junk phase data in the layer with the given id's
     *        own already-silent cells - the actual work behind
     *        `LayersPanel`'s phase-cleanup button, real-world testing pass
     *        finding #24. See `LayerController::cleanUpLayerPhase()`'s own
     *        docs for the exact mechanism. Refuses (no-op) for a layer
     *        with no content at all, or if no layer with this id exists.
     *        Marks hasUnsavedChanges() and refreshes the canvas and Layers
     *        Panel on success.
     *
     * @param id The layer to clean up.
     */
    void cleanUpLayerPhase(sound_mind::core::LayerId id);

    /**
     * @brief Selects the layer above the currently selected one - the
     *        actual work behind the "Select Layer Above" Edit menu action
     *        (Page Up), `v0.Y.46.1` Installment A ("Layers Panel & Editing
     *        Enhancements v2"). See `LayerController::selectLayerAbove()`'s
     *        own docs for the exact no-op conditions.
     */
    void selectLayerAbove();

    /// @brief Selects the layer below the currently selected one - the
    ///        actual work behind the "Select Layer Below" Edit menu action
    ///        (Page Down). See `LayerController::selectLayerBelow()`'s own
    ///        docs.
    void selectLayerBelow();

    /// @brief Moves the currently selected layer up one position in the
    ///        stack - the actual work behind the "Move Layer Up" Edit menu
    ///        action (Shift+Page Up). See `LayerController::
    ///        moveSelectedLayerUp()`'s own docs for the exact no-op
    ///        conditions. Marks hasUnsavedChanges() and refreshes the
    ///        canvas and Layers Panel on success.
    void moveSelectedLayerUp();

    /// @brief Moves the currently selected layer down one position in the
    ///        stack - the actual work behind the "Move Layer Down" Edit
    ///        menu action (Shift+Page Down). See `LayerController::
    ///        moveSelectedLayerDown()`'s own docs.
    void moveSelectedLayerDown();

    /// @brief Increases the currently selected layer's own opacity by a
    ///        fixed step - the actual work behind the "Increase Layer
    ///        Opacity" Edit menu action (Ctrl+Page Up). See
    ///        `LayerController::nudgeSelectedLayerOpacity()`'s own docs.
    void increaseSelectedLayerOpacity();

    /// @brief Decreases the currently selected layer's own opacity by the
    ///        same fixed step increaseSelectedLayerOpacity() applies -
    ///        the actual work behind the "Decrease Layer Opacity" Edit
    ///        menu action (Ctrl+Page Down).
    void decreaseSelectedLayerOpacity();

    /**
     * @brief Adds a new, empty `Normal` layer to the current project - the
     *        actual work behind `LayersPanel`'s "+ Add Layer" button.
     *
     * The only way to get a paintable layer that isn't an import: a
     * silent, project-dimensioned placeholder (`loopEngine_->emptyImage()`
     * - the same one a fresh Loop Input layer gets), added to the top of
     * the stack, selected immediately via `LayersPanel::selectLayer()` so
     * it can be painted into without an extra click. A no-op if no
     * project is open.
     */
    void addEmptyLayer();

    /**
     * @brief Adds a new `Filter`-type layer to the current project - the
     *        actual work behind `LayersPanel`'s "+ Add Filter Layer"
     *        button.
     *
     * Unlike addEmptyLayer(), a Filter layer is never painted onto (see
     * `sound_mind::core::Layer::filterConfiguration()`'s own docs), so no
     * placeholder content is set - it starts with a fresh, fully
     * transparent `FrequencyAxisGradient` configuration (see
     * `FilterConfiguration`'s own docs: "nothing happens by accident"),
     * added to the top of the stack and selected immediately via
     * `LayersPanel::selectLayer()`, ready to configure in
     * `FilterConfigurationPanel` without an extra click. A no-op if no
     * project is open.
     */
    void addFilterLayer();

    /**
     * @brief Reacts to `LayersPanel`'s own selection changing - the
     *        actual work keeping `FilterConfigurationPanel` in sync.
     *
     * Loads the newly selected layer's own `filterConfiguration()` into
     * `filterConfigurationPanel_` (via `setFilterConfiguration()`, which
     * doesn't itself emit a change - see that method's own docs) and
     * enables the panel, if `id` refers to either Filter layer kind (a
     * plain `Filter`-type layer, or the special `Equalizer` layer - see
     * `sound_mind::core::isFilterLayerType()`'s own docs); otherwise
     * disables the panel entirely (`QWidget::setEnabled(false)`) - editing
     * a Filter layer's own parameters only makes sense while one is
     * actually selected. Also calls `FilterConfigurationPanel::
     * setEqualizerMode()` (before `setFilterConfiguration()` - see that
     * method's own docs for why the order matters) so the panel shows its
     * specialized Cut editor for the Equalizer specifically, rather than
     * the general Frequency-Axis Gradient editor every plain `Filter`
     * layer of that type gets.
     *
     * @param id The newly selected layer's id, or `std::nullopt` if the
     *        selection was cleared - see `LayersPanel::selectionChanged()`'s
     *        own docs.
     */
    void handleLayerSelectionChanged(std::optional<sound_mind::core::LayerId> id);

    /**
     * @brief Applies `FilterConfigurationPanel`'s own edited configuration
     *        back onto whichever layer it's currently editing - the
     *        actual work behind `FilterConfigurationPanel::
     *        filterConfigurationChanged()`.
     *
     * A no-op if no project is open, or the panel isn't currently editing
     * a real, still-selected layer of either Filter layer kind (plain
     * `Filter` or `Equalizer` - see `sound_mind::core::isFilterLayerType()`'s
     * own docs; the panel is disabled in that case anyway - see
     * `handleLayerSelectionChanged()`'s own docs - so this shouldn't
     * normally be reachable, only guarded defensively).
     *
     * @param config The panel's own new, complete configuration.
     */
    void applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config);

    /**
     * @brief Appends a new, auto-named entry to the current project's own
     *        convolution kernel library and refreshes `filterConfigurationPanel_`'s
     *        own Load Kernel combo - the actual work behind
     *        `FilterConfigurationPanel::saveConvolutionKernelRequested()`.
     *
     * Named "Kernel 1", "Kernel 2", and so on, the same auto-naming
     * `captureMindShot()`/`captureMindGrain()` already establish (no
     * naming prompt). A no-op if no project is open.
     *
     * @param size The kernel's own current side length.
     * @param coefficients The kernel's own current coefficients, row-major.
     * @param normalize The kernel's own current Normalize setting.
     */
    void saveConvolutionKernel(int size, std::vector<float> coefficients, bool normalize);

    /**
     * @brief Feeds the current project's own convolution kernel library
     *        into `filterConfigurationPanel_->setAvailableConvolutionKernels()`
     *        - called whenever a project is opened/created and after
     *        `saveConvolutionKernel()` appends a new entry, the same
     *        "refresh after every library change" role
     *        `MindWaveController::refreshMindWavesPanel()` plays for
     *        MindWaves. An empty library (no project open) simply clears
     *        the panel's own Load Kernel combo down to its placeholder.
     */
    void refreshConvolutionKernelCombo();

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

    /**
     * @brief Adds a new, default MindWave to the current project - the
     *        actual work behind `MindWavesPanel`'s "+ Add MindWave"
     *        button (`v0.Y.31.1` Installment C2). Delegates entirely to
     *        `MindWaveController::addMindWave()`; marks
     *        hasUnsavedChanges() on success.
     */
    void addMindWave();

    /**
     * @brief Removes the MindWave with the given id from the current
     *        project - the actual work behind `MindWavesPanel`'s delete
     *        button. Delegates entirely to
     *        `MindWaveController::removeMindWave()`; marks
     *        hasUnsavedChanges() on success. Does **not** clear any
     *        layer's own reference to it - see `sound_mind::core::
     *        Project::removeMindWave()`'s own docs.
     * @param id The MindWave to remove.
     */
    void removeMindWave(sound_mind::core::MindWaveId id);

    /**
     * @brief Prompts for a new name and applies it - the actual work
     *        behind `MindWavesPanel`'s double-click-to-rename, the same
     *        interactive-slot-plus-non-prompting-core split
     *        `renameLayer()`/`MindWaveController::renameMindWaveTo()`
     *        already establish for layers.
     * @param id The MindWave to rename.
     */
    void renameMindWave(sound_mind::core::MindWaveId id);

    /**
     * @brief Applies `MindWavesPanel`'s own edited MindWave back onto the
     *        library entry it belongs to - the actual work behind
     *        `MindWavesPanel::mindWaveChanged()`. Delegates entirely to
     *        `MindWaveController::updateMindWave()`; marks
     *        hasUnsavedChanges() on success.
     * @param id The entry that changed.
     * @param wave Its own new, complete MindWave.
     */
    void updateMindWave(sound_mind::core::MindWaveId id, const sound_mind::core::MindWave& wave);

    /**
     * @brief Turns the canvas's Paint tool on or off - the actual work
     *        behind the toolbar's Paint toggle.
     *
     * Turning it off cancels any in-progress stroke (see
     * `PaintController::cancelStroke()`'s own docs) rather than leaving
     * it dangling; turning it on has no effect if no project is open.
     *
     * Paint/Pick/Select/Path share one canvas tool mode (`CanvasWidget::
     * ToolMode`) and so can never have more than one active - enforced by
     * setExclusiveToolMode() (see its own docs for why that's hand-managed
     * rather than a `QActionGroup`), not repeated here or in
     * setPickModeEnabled()/setSelectModeEnabled()/setPathModeEnabled().
     *
     * @param enabled `true` to accept freehand paint input on the canvas;
     *        `false` to return to plain, non-interactive display.
     */
    void setPaintModeEnabled(bool enabled);

    /**
     * @brief Toggles between Pick and plain (`ToolMode::None`) canvas
     *        interaction - the actual work behind the toolbar's Pick
     *        toggle.
     *
     * Turning it off clears the current selection (see `PickController::
     * clearSelection()`'s own docs) rather than leaving it dangling;
     * turning it on has no effect if no project is open. See
     * setPaintModeEnabled()'s own docs for the shared exclusivity with
     * Paint/Select/Path.
     *
     * @param enabled `true` to accept Pick input on the canvas; `false`
     *        to return to plain, non-interactive display.
     */
    void setPickModeEnabled(bool enabled);

    /**
     * @brief Toggles between Select and plain (`ToolMode::None`) canvas
     *        interaction - the actual work behind the toolbar's Select
     *        toggle.
     *
     * Turning it off cancels any in-progress selection drag (see
     * `SelectionController::cancelSelectionDrag()`'s own docs) - a
     * *committed* selection stays exactly as it is, since it scopes
     * Fill/Cut/Copy/Paste independent of which tool is currently active
     * (`docs/sound-mind-design.md`'s own "Selection" framing); turning it
     * on has no effect if no project is open. See setPaintModeEnabled()'s
     * own docs for the shared exclusivity with Paint/Pick/Path.
     *
     * @param enabled `true` to accept Select input on the canvas; `false`
     *        to return to plain, non-interactive display.
     */
    void setSelectModeEnabled(bool enabled);

    /**
     * @brief Toggles between Path and plain (`ToolMode::None`) canvas
     *        interaction - the actual work behind the toolbar's Path
     *        toggle.
     *
     * Turning it off cancels any in-progress node placement (see
     * `PathController::cancelPath()`'s own docs) - discarded, not
     * committed; turning it on has no effect if no project is open. See
     * setPaintModeEnabled()'s own docs for the shared exclusivity with
     * Paint/Pick/Select.
     *
     * @param enabled `true` to accept Path input on the canvas; `false` to
     *        return to plain, non-interactive display.
     */
    void setPathModeEnabled(bool enabled);

    /**
     * @brief Toggles between Chord Stamp and plain (`ToolMode::None`)
     *        canvas interaction - the actual work behind the toolbar's
     *        Chord toggle.
     *
     * Unlike setPaintModeEnabled()/setPickModeEnabled()/
     * setSelectModeEnabled()/setPathModeEnabled() above, turning it off
     * cancels nothing - `ChordStamp` mode's own single-press gesture has no
     * in-progress state to begin with (see
     * `CanvasWidget::ToolMode::ChordStamp`'s own docs); turning it on has
     * no effect if no project is open. See setPaintModeEnabled()'s own
     * docs for the shared exclusivity with Paint/Pick/Select/Path.
     *
     * @param enabled `true` to accept Chord Stamp input on the canvas;
     *        `false` to return to plain, non-interactive display.
     */
    void setChordModeEnabled(bool enabled);

    /**
     * @brief Undoes the most recent undoable edit, if any - the actual
     *        work behind the Edit menu's Undo action.
     *
     * Delegates to `undoStack_` (a `sound_mind::studio::UndoStack`),
     * covering both content operations (paint strokes, Pick's move/
     * modify/delete, Fill, Paste) and layer property changes (opacity,
     * opacityMindWave binding, visibility, translation, rescale - see
     * `LayerController`'s own docs), in whatever order they actually
     * happened - a no-op if nothing is undoable (no project open, or
     * nothing undoable yet since the project's own history - session-
     * only, see `UndoStack`'s own docs - began).
     */
    void undo();

    /// @brief Redoes the most recently undone edit, if any - the actual
    ///        work behind the Edit menu's Redo action. Delegates to
    ///        `undoStack_`; a no-op if nothing is redoable.
    void redo();

    /**
     * @brief Jumps directly to an arbitrary point in `undoStack_`'s own
     *        history - the actual work behind `HistoryPanel`'s own
     *        double-click gesture, `v0.Y.46.1` Installment D ("History
     *        Panel").
     *
     * Delegates to `UndoStack::jumpTo()` (see its own docs - every
     * intermediate command's own callback still actually runs, in order),
     * then refreshes the History Panel to reflect the new position - every
     * other affected refresh (canvas, Layers Panel, `hasUnsavedChanges()`)
     * already happens as a side effect of those same callbacks running,
     * the same as a plain undo()/redo() already triggers them.
     *
     * @param index The target index - see `UndoStack::jumpTo()`'s own docs.
     */
    void jumpToHistoryIndex(std::size_t index);

    /// @brief Zooms in proportionally - the actual work behind the View >
    ///        Zoom menu's "Zoom In" action (`]`). Delegates to
    ///        `CanvasWidget::zoomIn()`.
    void zoomIn();

    /// @brief The inverse of zoomIn() - "Zoom Out" (`[`).
    void zoomOut();

    /// @brief Zooms in the time axis only (frequency-invariant) -
    ///        "Zoom In (Time Only)" (`Shift+]`).
    void zoomInTimeOnly();

    /// @brief The inverse of zoomInTimeOnly() - "Zoom Out (Time Only)"
    ///        (`Shift+[`).
    void zoomOutTimeOnly();

    /// @brief Zooms in the frequency axis only (time-invariant) -
    ///        "Zoom In (Frequency Only)" (`Alt+]`).
    void zoomInFrequencyOnly();

    /// @brief The inverse of zoomInFrequencyOnly() - "Zoom Out (Frequency
    ///        Only)" (`Alt+[`).
    void zoomOutFrequencyOnly();

    /// @brief Zooms in proportionally, at a coarser step than zoomIn()'s
    ///        own - "Zoom In (Coarse)" (`Ctrl+]`).
    void zoomInCoarse();

    /// @brief The inverse of zoomInCoarse() - "Zoom Out (Coarse)"
    ///        (`Ctrl+[`).
    void zoomOutCoarse();

    /// @brief Switches to `CanvasWidget::ZoomMode::FitToWindow` - "Fit to
    ///        Window" (`Ctrl+0`).
    void zoomToFit();

    /// @brief Switches to `CanvasWidget::ZoomMode::Manual` at exactly
    ///        100% - "Actual Size" (`Ctrl+1`).
    void zoomToActualSize();

    /// @brief Deletes the currently Picked paint object, if any - the
    ///        actual work behind the Edit menu's Delete action. Delegates
    ///        to `PickController::deleteSelection()`; a no-op if nothing
    ///        is selected.
    void deletePickedObject();

    /// @brief Moves the currently Picked object to the top of its own
    ///        layer's stack - the actual work behind the Edit menu's
    ///        "Bring to Front" action. Delegates to
    ///        `PickController::bringToFront()`; a no-op if nothing is
    ///        selected, or it's already topmost.
    void bringPickedObjectToFront();

    /// @brief Moves the currently Picked object to the bottom of its own
    ///        layer's stack - the actual work behind the Edit menu's
    ///        "Send to Back" action. Delegates to
    ///        `PickController::sendToBack()`; a no-op if nothing is
    ///        selected, or it's already at the back.
    void sendPickedObjectToBack();

    /// @brief Swaps the currently Picked object with whichever active
    ///        object on its own layer sits immediately above it - the
    ///        actual work behind the Edit menu's "Bring Forward" action.
    ///        Delegates to `PickController::bringForward()`; a no-op if
    ///        nothing is selected, or it's already topmost.
    void bringPickedObjectForward();

    /// @brief Swaps the currently Picked object with whichever active
    ///        object on its own layer sits immediately below it - the
    ///        actual work behind the Edit menu's "Send Backward" action.
    ///        Delegates to `PickController::sendBackward()`; a no-op if
    ///        nothing is selected, or it's already at the back.
    void sendPickedObjectBackward();

    /// @brief Enters direct node/handle editing of the currently Picked
    ///        stroke's own Path - the actual work behind the Edit menu's
    ///        "Edit Path" action. Delegates to
    ///        `PickController::beginPathEdit()`; a no-op if nothing is
    ///        selected, or the selection isn't a stroke (a `FillOperation`/
    ///        `PasteOperation` has no Path to edit).
    void editPickedPath();

    /// @brief Converts the currently selected node (within an active path
    ///        edit) between `Corner` and `Smooth` - the actual work
    ///        behind the Edit menu's "Toggle Node Type" action. Delegates
    ///        to `PickController::toggleSelectedPathNodeType()`; a no-op
    ///        if a path edit isn't active, or no node is selected.
    void togglePickedPathNodeType();

    /// @brief Commits the active path edit's own accumulated changes -
    ///        the actual work behind the Edit menu's "Apply Path Edit"
    ///        action. Delegates to `PickController::commitPathEdit()`; a
    ///        no-op if a path edit isn't active.
    void applyPickedPathEdit();

    /// @brief Discards the active path edit's own accumulated changes -
    ///        the actual work behind the Edit menu's "Cancel Path Edit"
    ///        action. Delegates to `PickController::cancelPathEdit()`; a
    ///        no-op if a path edit isn't active.
    void cancelPickedPathEdit();

    /// @brief Clears the current rectangular selection - the actual work
    ///        behind the Edit menu's Deselect action. Delegates to
    ///        `SelectionController::clearSelection()`; a no-op if there
    ///        isn't one.
    void deselect();

    /**
     * @brief Fills the current selection with a flat `color` - a thin,
     *        still-supported shortcut onto fillSelectionWithGradient()
     *        (see its own docs), kept for direct/test use where a real
     *        gradient editor would be overkill.
     *
     * `color`'s red/green channels become the fill's own left/right
     * channel intensity (`sound_mind::studio::dbToDisplayByte()`'s own
     * inverse - the identical red=left/green=right convention
     * `ToolConfigurationPanel`'s own brush Color/gradient controls already
     * established), applied uniformly (both gradient stops the same
     * value) at full opacity. A no-op if there's no committed selection.
     *
     * @param color The color to fill with.
     */
    void fillSelectionWith(QColor color);

    /**
     * @brief Fills the current selection with `gradient` directly - the
     *        testable core behind the Edit menu's Fill Selection action's
     *        own `FillGradientDialog` (see fillSelection()'s own docs),
     *        and directly callable without one (e.g. by a test).
     *
     * A real, full multi-stop gradient fill - `applyFillOperation()`
     * already evaluates `gradient` across the selection's own time axis
     * (`t` running left-to-right), so this needed no core-level changes,
     * only this real gradient-editing entry point where the panel used to
     * flatten every fill to a single uniform color/opacity pair (see
     * fillSelectionWith()'s own docs for that still-supported shortcut). A
     * no-op if there's no committed selection.
     *
     * @param gradient The gradient to fill with.
     */
    void fillSelectionWithGradient(const sound_mind::core::Gradient& gradient);

    /**
     * @brief Shows `FillGradientDialog`, seeded with a fully-opaque
     *        default gradient, and fills the current selection with
     *        whatever's accepted - the actual work behind the Edit menu's
     *        Fill Selection action.
     *
     * Cancelling leaves the selection untouched, matching every other
     * dialog-driven action in this codebase treating a cancelled dialog
     * as "nothing happened". A no-op (dialog never shown) if there's no
     * selection to fill in the first place.
     */
    void fillSelection();

    /**
     * @brief Applies whatever `FilterConfiguration` the Filter
     *        Configuration Panel currently shows to the current
     *        selection - the actual work behind the Edit menu's Apply
     *        Filter to Selection action, real-world testing pass finding
     *        #33 ("Layers Panel & Editing Enhancements v2" Installment E,
     *        `v0.Y.46.1`).
     *
     * Reads `filterConfigurationPanel_->filterConfiguration()` directly -
     * whichever configuration the panel is showing right now, whether
     * that's a real Filter/Equalizer layer's own or the "pending" one
     * `addFilterLayer()` seeds a new layer from (see
     * `LayerController::pendingFilterConfiguration()`'s own docs) - the
     * same "whatever's currently configured" a user would expect, without
     * needing to add a Filter layer first just to configure one. A no-op
     * if there's no committed selection (delegates to
     * `SelectionController::applyFilterToSelection()`).
     */
    void applyFilterToSelection();

    /**
     * @brief Switches the current project's own Principal Mode
     *        (`docs/sound-mind-design.md`'s "Principal modes", `v0.Y.47.1`)
     *        between Sound-mode and Image-mode.
     *
     * Persisted on the project itself (`Project::setPrincipalMode()`), not
     * app-wide `QSettings` (unlike Hardware Acceleration's own toggle) -
     * this is a property of how a project's *content* is authored, not a
     * diagnostic/display preference. Only affects new geometry synthesized
     * from here on (currently: Procedural/Heal/Soften/Smudge/OrderChaos
     * brush-stamp radius) - already-painted content is completely
     * unaffected until repainted.
     *
     * @param imageModeEnabled `true` for Image-mode (a stamp's on-screen
     *        pixel footprint stays fixed regardless of frequency
     *        register); `false` for Sound-mode (the default, and the only
     *        behavior before this milestone).
     */
    void setPrincipalMode(bool imageModeEnabled);

    /**
     * @brief Captures the currently Picked object's own `Path` as the
     *        MindWaves panel's own currently-selected library entry's drawn
     *        shape - `docs/sound-mind-design.md`'s "MindWave Functions"
     *        ("Drawn shapes"), `v0.Y.39.1` Installment B.
     *
     * **The workflow, in full**: draw a curve as an ordinary paint
     * stroke, switch to Pick and click it (selecting it the same way
     * Pick already selects anything), then choose Edit → Use Picked Path
     * as MindWave Shape - no dedicated curve-drawing mode exists, or is
     * needed, since Pick's own existing click-to-select mechanic already
     * supplies exactly this. A no-op unless *both* something is Picked
     * (`ToolPaletteController::selectedPath()`) and a library entry is
     * currently selected in `mindWavesPanel_`
     * (`MindWavesPanel::selectedMindWaveId()`) - stay in Pick mode, don't
     * switch back to Select, right up until choosing this action, since
     * leaving Pick mode clears whatever was Picked. Switches that entry's
     * own generator type to `Drawn` as part of the same action - see
     * `MindWaveController::setDrawnPath()`'s own docs.
     */
    void usePickedPathAsMindWaveShape();

    /**
     * @brief Copies the current selection's own pixels onto the clipboard -
     *        the actual work behind the Edit menu's Copy action. Delegates
     *        to `SelectionController::copySelection()`; a no-op if there's
     *        no committed selection.
     */
    void copySelection();

    /**
     * @brief Copies the current selection (see copySelection()) and clears
     *        its own source pixels - the actual work behind the Edit
     *        menu's Cut action. Delegates to
     *        `SelectionController::cutSelection()`; a no-op if there's no
     *        committed selection.
     */
    void cutSelection();

    /**
     * @brief Pastes the clipboard onto whichever layer a freehand stroke
     *        started right now would paint into (`paintTargetLayerId()`) -
     *        the actual work behind the Edit menu's Paste action.
     *
     * The paste target is resolved independently of the clipboard's own
     * source layer - per `SelectionController::pasteInto()`'s own docs, a
     * copy from one layer can be pasted onto a completely different one.
     * A no-op if there's nothing on the clipboard, or no project is open.
     * Blends onto the target per `SelectionConfigurationPanel::
     * pasteBlendMode()`'s own current value, read fresh at the moment of
     * this call - `v0.Y.37.1` (Deferred Blend Modes).
     *
     * On success, switches to Pick mode and selects the newly pasted
     * region there (`PickController::selectOperation()`) - immediately
     * movable/modifiable/deletable/restackable with no separate switch-
     * to-Pick-and-click-it step needed to find it again, regardless of
     * whatever tool mode was active before pasting.
     */
    void paste();

    /**
     * @brief Captures the current selection into a new, named entry in the
     *        project's Mind Shot library - the actual work behind the Edit
     *        menu's "Capture as Mind Shot" action (`docs/sound-mind-
     *        design.md`'s "Mind Shots"). Delegates to
     *        `SelectionController::captureMindShot()` (via
     *        `toolPaletteController_`); a no-op if there's no committed
     *        selection or no project open.
     *
     * Auto-names the new entry `"Mind Shot <N>"`, `N` one more than the
     * library's own current size - no naming dialog, matching this
     * codebase's own "quick action, not a richer flow" precedent for
     * Copy/Cut (this is architecturally the same capture, just stored
     * permanently). Confirms success via a status-bar message, the same
     * lightweight feedback `importAudioFile()`/`importImageFiles()` already
     * give for a comparably one-off, meaningfully-persistent action.
     */
    void captureMindShot();

    /**
     * @brief Captures the current selection's own `{layer, bounds}` into a
     *        new, named entry in the project's Mind Grain library - the
     *        actual work behind the Edit menu's "Capture as Mind Grain"
     *        action (`docs/sound-mind-design.md`'s "Mind Grains"). Delegates
     *        to `SelectionController::captureMindGrain()` (via
     *        `toolPaletteController_`); a no-op if there's no committed
     *        selection or no project open.
     *
     * Unlike captureMindShot(), no pixel content is ever captured - see
     * `SelectionController::captureMindGrain()`'s own docs. Auto-names the
     * new entry `"Mind Grain <N>"`, the same convention captureMindShot()
     * uses, and confirms success the same way (a status-bar message).
     */
    void captureMindGrain();

    /**
     * @brief Finishes the Path tool's own in-progress node placement,
     *        committing it as a new paint object - the actual work behind
     *        the Edit menu's "Finish Path" action. Delegates to
     *        `PathController::finishPath()`; a no-op if no placement is in
     *        progress.
     */
    void finishPath();

    /// @brief Discards the Path tool's own in-progress node placement
    ///        without committing anything - the actual work behind the
    ///        Edit menu's "Cancel Path" action. Delegates to
    ///        `PathController::cancelPath()`; a no-op if no placement is
    ///        in progress.
    void cancelPath();

    /**
     * @brief Sets which node type the Path tool places next - the actual
     *        work behind the toolbar's "Smooth Nodes" checkable toggle.
     *
     * Delegates to `PathController::setDefaultNodeType()` - see its own
     * docs (the design doc's own "standing default that can be flipped at
     * any time"). Affects only nodes placed after this call.
     *
     * @param smooth `true` for `PathNodeType::Smooth`; `false` for
     *        `PathNodeType::Corner` (the default).
     */
    void setPathPlacesSmoothNodes(bool smooth);

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
     * (import, Pool, a Loop/Recording capture adding or updating a layer,
     * or any real `Operation` appended to the project's `OperationLog` -
     * paint, fill, paste, pick, and their own undo()/redo() - since
     * Phase 3's own milestones) and cleared by a successful save or by
     * setProject() (a fresh/loaded project matches what's on disk, or -
     * for `newProject()` - has nothing on disk to differ from yet).
     * Deliberately still simpler than diffing against the operation log's
     * own replay, even though real `Operation` subtypes now exist to diff
     * against: this flag only needs to answer "has anything changed since
     * the last save", not "exactly what changed" - a plain flag answers
     * that with no false negatives (every mutating call site sets it) as
     * cheaply as a boolean write, where a replay-diff would need to
     * actually re-render and compare content. Revisit only if a real need
     * for the finer-grained answer (e.g. a per-layer "modified" indicator)
     * emerges.
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
    /// setConfiguredInputDevice(). Empty for the system default, or if no
    /// project has ever been opened yet.
    /// @return The preferred input device name.
    [[nodiscard]] QString loopInputDevice() const;

    /// @brief The output device Loop Mode's next start() will prefer -
    /// see setConfiguredOutputDevice(). Empty for the system default, or
    /// if no project has ever been opened yet.
    /// @return The preferred output device name.
    [[nodiscard]] QString loopOutputDevice() const;

    /// @brief The input device Recording's next start() will prefer - see
    /// setConfiguredInputDevice(). Empty means the system default.
    /// @return The preferred input device name.
    [[nodiscard]] QString recordInputDevice() const;

    /// @brief Loop Mode's own currently configured input gain - see
    /// setConfiguredInputGain(). `1.0` is unity, or if no project has ever
    /// been opened yet.
    /// @return The underlying LoopEngine's inputGain().
    [[nodiscard]] float loopInputGain() const noexcept;

    /// @brief The current Playback output gain - see setPlaybackVolume().
    /// @return `1.0` is unity; the underlying PlaybackEngine's volume().
    [[nodiscard]] float playbackVolume() const noexcept;

    /**
     * @brief Imports every project-length snippet of an audio file as new
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
     * @param path Path to the audio file to import.
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
     * @param path Path to the audio file to analyze.
     * @param errorMessage If non-null and this returns empty, set to a
     *        human-readable description of what went wrong.
     * @param offsetSeconds See `sound_mind::studio::audioSnippetsForFile()`'s
     *        own docs (real-world testing pass finding #23) - `0.0` (the
     *        default) reproduces the pre-finding-#23 behavior exactly.
     * @return One entry per snippet, in order; empty if no project is
     *         open, the file couldn't be read, or `offsetSeconds` leaves
     *         nothing to split.
     */
    [[nodiscard]] std::vector<AudioSnippetPickerDialog::RowData> audioSnippetsForFile(
        const std::filesystem::path& path, QString* errorMessage = nullptr, double offsetSeconds = 0.0) const;

    /**
     * @brief Imports specific snippets (see audioSnippetsForFile()) of an
     *        audio file as new layers, synchronously and without prompting
     *        or showing an error dialog on failure - the actual work
     *        behind importAudioFile() (which requests every snippet), kept
     *        as the direct, blocking, headless-testable entry point even
     *        though importAudio()'s own snippet-picker interactive slot no
     *        longer calls this directly - see importAudioSnippetsAsync()'s
     *        own docs (finding #12, Installment F) for what actually backs
     *        it now.
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
     * @param path Path to the audio file to import from.
     * @param snippetIndices Which of the source's snippets to import, in
     *        any order and with any duplicates ignored; an index at or
     *        beyond the source's actual snippet count is silently
     *        skipped, not an error.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @param offsetSeconds See audioSnippetsForFile()'s own docs - must
     *        match whatever offset `snippetIndices` was itself computed
     *        against.
     * @return `true` if the file was read and at least one requested
     *         snippet was imported; `false` if the file couldn't be read,
     *         no project is open, or nothing was actually imported (an
     *         empty `snippetIndices`, or every given index out of range).
     */
    bool importAudioSnippets(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                              QString* errorMessage = nullptr, double offsetSeconds = 0.0);

    /**
     * @brief Starts an asynchronous import of specific snippets (see
     *        audioSnippetsForFile()) of an audio file - the real work
     *        behind importAudio()'s own snippet-picker interactive flow,
     *        split out so tests can drive it directly - real-world testing
     *        pass finding #12 ("a real, non-blocking cancel affordance for
     *        long operations"), Installment F, the first installment
     *        actually needing rollback.
     *
     * Encodes on a `sound_mind::core::BackgroundTask` via
     * `sound_mind::studio::encodeAudioSnippets()` - the encode-only half of
     * `importAudioSnippetsInto()`, which touches no shared/mutable project
     * state at all (it takes a plain `ProjectSettings` value, not a live
     * `Project&`), so the background thread never needs to synchronize
     * against `project_`. The encoded layers are only ever added to
     * `project_` afterward, on the UI thread, once the background encode
     * has actually finished successfully - see pollImportProgress()'s own
     * docs. Cancelling therefore rolls back cleanly by construction: the
     * encoded-so-far layers are simply discarded, having never touched
     * `project_` in the first place - simpler than Export's own "delete
     * the partial file" rollback (Decision #136), since nothing persistent
     * or shared was ever written to begin with.
     *
     * A no-op (just a status bar message, no dialog) if no project is open,
     * or if isImportRunning() is already `true` - a second call would
     * otherwise destroy the still-running `importTask_`, whose destructor
     * blocks until its thread joins, silently freezing the UI. Also why
     * newProject()/openProject()/openProjectAt()/closeEvent() all refuse
     * outright while isImportRunning(): switching or closing the project
     * out from under a background import that will later call
     * `project_->addLayer()` on whatever `project_` turns out to be by
     * then would silently misattribute those layers to the wrong project.
     *
     * A separate slot from exportTask_/exportProgressTimer_/
     * exportCancelButton_, not shared with them - unlike Video/Audio
     * export (which read the same topmost layer and are never meaningfully
     * run together), importing new content and exporting existing content
     * are independent operations with no reason to block each other.
     *
     * @param path Path to the audio file to import from.
     * @param snippetIndices Which of the source's snippets to import - see
     *        importAudioSnippets()'s own docs.
     * @param offsetSeconds See audioSnippetsForFile()'s own docs - must
     *        match whatever offset `snippetIndices` was itself computed
     *        against.
     */
    void importAudioSnippetsAsync(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                                   double offsetSeconds = 0.0);

    /// @brief Whether an importAudioSnippetsAsync() import is still
    /// running.
    /// @return `true` from importAudioSnippetsAsync() (once it actually
    ///         started a background task) until the background encode
    ///         finishes, one way or another.
    [[nodiscard]] bool isImportRunning() const noexcept;

    /// @brief Requests cancellation of the currently running import - the
    /// actual work behind the status bar's own cancel button. A no-op if
    /// isImportRunning() is `false`.
    void cancelImport();

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
     * independently, each with `mode` and no translation
     * (`translationColumns()` stays `0`) - the same effect as calling
     * importImageFile() once per path (as of `v0.Y.23.1`, both delegate to
     * the same underlying `sound_mind::studio::importImageFileInto()`).
     * When `true`, `mode` is
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
     * A recognized audio extension (see `isAudioExtension()` - WAV, MP3,
     * FLAC, Ogg, AIFF, M4A, or Opus, since `v0.0.42.3`) goes to
     * importAudioSnippets() with whatever indices `audioSnippetSelections`
     * gives that path, or - for a path with no entry there - the same
     * "every computed snippet, no picker" behavior importAudioFile()
     * always had; every image extension
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
     *        given recognized-audio-extension path among `paths` - see
     *        `audioSnippetsForFile()`/`importAudioSnippets()`'s own docs. A
     *        path with no entry here imports every snippet it has, the
     *        pre-existing default every test predating this parameter
     *        still gets.
     * @param audioSnippetOffsets The offset (see audioSnippetsForFile()'s
     *        own docs, real-world testing pass finding #23)
     *        `audioSnippetSelections`' own indices were computed against,
     *        for a given path - a path with no entry here uses `0.0`, the
     *        pre-existing default every test predating this parameter
     *        still gets. A separate map, not folded into
     *        `audioSnippetSelections` itself, so that parameter's own
     *        pre-existing shape (and every test already constructing it)
     *        stays untouched.
     */
    void handleDroppedFiles(
        const std::vector<std::filesystem::path>& paths,
        ImageScalePickerDialog::Mode imageMode = ImageScalePickerDialog::Mode::RescaleToFitProject,
        bool importImagesAsSequence = false,
        const std::map<std::filesystem::path, std::vector<std::size_t>>& audioSnippetSelections = {},
        const std::map<std::filesystem::path, double>& audioSnippetOffsets = {});

    /**
     * @brief Pools the topmost layer with content and writes its Stream
     *        and Pool renders as PNG files, synchronously and without
     *        showing any dialog - kept as the direct, blocking,
     *        headless-testable entry point (matching importAudioFile()'s/
     *        importImageFile()'s own reason for existing) even though
     *        poolTopmostLayer()'s own interactive slot no longer calls this
     *        directly - see poolTopmostLayerAsync()'s own docs (finding
     *        #12, Installment G) for what actually backs it now.
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
     * @brief Starts an asynchronous Pool of the topmost layer with content -
     *        the real work behind the interactive poolTopmostLayer() slot -
     *        real-world testing pass finding #12 ("a real, non-blocking
     *        cancel affordance for long operations"), Installment G.
     *
     * Copies the layer's own current content out by value, then runs
     * `sound_mind::core::computePooledContent()` on a `BackgroundTask` with
     * a cancellation callback - the same "compute independently, commit on
     * the UI thread only once fully successful" shape
     * importAudioSnippetsAsync() already establishes (see its own docs),
     * applied here to a layer *replacing* its own content rather than new
     * layers being added. Cancelling therefore rolls back cleanly: the
     * computed-but-uncommitted `PooledContent` is simply discarded, the
     * layer's own real content never having been touched at all.
     *
     * The target layer is tracked by id (`poolLayerId_`), not a raw
     * pointer, since `Project::layers()` is a `std::vector<Layer>` that
     * could reallocate while the background pool runs - `layerController_->
     * layerById()` re-resolves it fresh once the pool finishes, and simply
     * does nothing (beyond reporting it) if the layer no longer exists by
     * then.
     *
     * A no-op (just a status bar message, no dialog) if no project is open,
     * there's no layer with content to pool, or isPoolRunning() is already
     * `true` - matching importAudioSnippetsAsync()'s own reasoning for the
     * same guard. A separate slot from exportTask_/importTask_, not shared
     * with either - Export never mutates the project at all, and Pool's own
     * completion semantics (finding the target layer by id and replacing
     * its content) don't match Import's (adding brand new layers) closely
     * enough to share that machinery either. newProject()/openProject()/
     * openProjectAt()/closeEvent() all also refuse outright while
     * isPoolRunning(), the same reason (and pattern) they already refuse
     * while isImportRunning().
     *
     * Does *not* write the Stream/Pool comparison PNGs
     * poolTopmostLayerNow() does - that's a debug/verification feature no
     * interactive caller has ever actually used (poolTopmostLayer() itself
     * always passes `nullptr` for both).
     */
    void poolTopmostLayerAsync();

    /// @brief Whether a poolTopmostLayerAsync() pool is still running.
    /// @return `true` from poolTopmostLayerAsync() (once it actually
    ///         started a background task) until the background compute
    ///         finishes, one way or another.
    [[nodiscard]] bool isPoolRunning() const noexcept;

    /// @brief Requests cancellation of the currently running pool - the
    /// actual work behind the status bar's own cancel button. A no-op if
    /// isPoolRunning() is `false`.
    void cancelPool();

    /**
     * @brief Exports the topmost layer with content's audio to `path`,
     *        synchronously and without prompting or showing an error dialog
     *        on failure - kept as the direct, blocking, headless-testable
     *        entry point (matching importAudioFile()'s own reason for
     *        existing) even though exportAudio()'s own interactive slot no
     *        longer calls this directly - see
     *        exportTopmostLayerAudioAsync()'s own docs (`v0.0.45.17`,
     *        finding #12 Installment E) for what actually backs it now.
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
     *        `path`, synchronously and without prompting or showing an
     *        error dialog on failure - kept as the direct, blocking,
     *        headless-testable entry point (matching importAudioFile()'s
     *        own reason for existing) even though exportVideo()'s own
     *        interactive slot no longer calls this directly - see
     *        exportTopmostLayerVideoAsync()'s own docs (`v0.0.45.15`,
     *        finding #12 Installment C) for what actually backs it now.
     *
     * @param path Destination path.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if there was no layer to export,
     *         or the underlying codec export failed.
     */
    bool exportTopmostLayerVideoNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /**
     * @brief Starts an asynchronous MP4 video export of the topmost layer
     *        with content - the real work behind the interactive
     *        exportVideo() slot's own file-dialog flow, split out so tests
     *        can drive it directly without a real `QFileDialog` (matching
     *        exportTopmostLayerVideoNow()'s own reason for existing, but
     *        for the async path instead of the synchronous one) -
     *        real-world testing pass finding #12 ("a real, non-blocking
     *        cancel affordance for long operations"), Installment C.
     *
     * Decodes/renders the layer's own content synchronously first (fast -
     * matching exportTopmostLayerVideoNow()'s own "no layer to export"
     * check), then hands the resulting, already-independent
     * `AudioBuffer`/`RgbImage` values to a `sound_mind::core::BackgroundTask`
     * running `sound_mind::codec::exportVideo()` - the encode itself
     * touches no shared/mutable project state at all once started, so
     * editing the project while this one runs is safe by construction, not
     * by locking anything. A no-op (just a status bar message, no dialog)
     * if isExportRunning() is already `true` (whether that's a video *or*
     * an audio export already in progress - see exportTopmostLayerAudioAsync()'s
     * own docs on why they share one slot) - a second call would otherwise
     * destroy the still-running `exportTask_`, whose destructor blocks
     * until its thread joins, silently freezing the UI for however long
     * the first export had left to run.
     *
     * Returns immediately once the background encode has started - see
     * isExportRunning()/cancelExport() for inspecting/stopping it.
     * Completion is detected by an internal `QTimer` poll (the same
     * "background worker + UI-thread polling timer" pattern
     * `loopUpdateTimer_`/`updateLoopLayer()` already establish for
     * `LoopEngine`): on success, a status bar message naming the
     * destination; on cancellation, the (necessarily partial, and
     * therefore deleted) output file is removed and a "cancelled" status
     * bar message shown instead; on a genuine encode failure,
     * `QMessageBox::critical()` - matching exportVideo()'s own existing
     * failure presentation.
     *
     * Shows `QMessageBox::critical()` immediately, without starting
     * anything, if there's no layer with content to export at all - unlike
     * a genuine encode failure, this is knowable synchronously (before the
     * background task would even start), so there's nothing to poll for.
     *
     * @param path Destination path.
     */
    void exportTopmostLayerVideoAsync(const std::filesystem::path& path);

    /**
     * @brief Starts an asynchronous compressed-audio export of the topmost
     *        layer with content - the real work behind the interactive
     *        exportAudio() slot's own file-dialog flow, mirroring
     *        exportTopmostLayerVideoAsync()'s own shape exactly - real-world
     *        testing pass finding #12, Installment E.
     *
     * Shares `exportTask_`/`exportProgressTimer_`/`exportCancelButton_` with
     * the video path rather than keeping two independent sets: the status
     * bar has room for one message and one cancel button at a time, and a
     * user is never meaningfully exporting both at once from a single
     * project - see isExportRunning()'s own docs for what "already
     * running" means across both. `exportKind_` records which one is
     * actually in flight, purely so pollExportProgress() can phrase its own
     * status bar message correctly ("audio"/"video").
     *
     * @param path Destination path; its extension (`.flac`/`.ogg`/`.mp3`)
     *        selects the compressed format, matching exportAudio()'s own
     *        existing `QFileDialog` filter.
     * @param format Which compressed format to write - see
     *        `sound_mind::codec::exportCompressedAudio()`.
     */
    void exportTopmostLayerAudioAsync(const std::filesystem::path& path, sound_mind::codec::CompressedAudioFormat format);

    /// @brief Whether an exportTopmostLayerVideoAsync()/
    /// exportTopmostLayerAudioAsync() export - either kind - is still
    /// running. Only one export (of either kind) runs at a time - see
    /// exportTopmostLayerAudioAsync()'s own docs on why they share one slot.
    /// @return `true` from either async export method (once it actually
    ///         started a background task - not for its own synchronous
    ///         "nothing to export" early-out) until the background encode
    ///         finishes, one way or another.
    [[nodiscard]] bool isExportRunning() const noexcept;

    /// @brief Requests cancellation of the currently running export
    /// (whichever kind it is) - the actual work behind the status bar's own
    /// cancel button. A no-op if isExportRunning() is `false`.
    void cancelExport();

    /**
     * @brief Opens the bundled user-facing HTML doc at
     *        `"<applicationDirPath()>/docs/<htmlFilename>"` via
     *        openExternalUrl() if it exists there - the headless-testable
     *        half of every Help-menu/Landing-Page documentation link
     *        (`v0.0.42.4`, Workflow & Device Polish, Installment D), split
     *        out for the same reason importAudioFile() splits its own
     *        actual work from the interactive slot that shows a dialog on
     *        failure (see its docs).
     *
     * `applicationDirPath()`, not a source-tree-relative path: the same
     * "next to the running executable" location `windeployqt`'s own Qt
     * DLLs and the `VCPKG_APPLOCAL_DEPS`-copied third-party DLLs already
     * land in (see `sound-mind-studio/CMakeLists.txt`'s own install rules)
     * - the `docs` CMake target's own build step copies the generated
     * user-doc HTML there too, so a local dev build and an installed/
     * packaged one both resolve the exact same way.
     *
     * @param htmlFilename The bundled doc's own filename, e.g.
     *        `"index.html"` or `"user_guide.html"`.
     * @return `true` if the file exists and openExternalUrl() was called;
     *         `false` if it doesn't (nothing else happens - no dialog is
     *         shown; that's openUserDocOrShowFallback()'s own job).
     */
    bool openUserDocIfBundled(const QString& htmlFilename);

protected:
    /**
     * @brief Actually opens `url` via the OS's own default handler
     *        (`QDesktopServices::openUrl()`) - the one real side effect
     *        openUserDocIfBundled() causes, factored out into its own
     *        overridable seam so a test can observe *that* a doc would
     *        have opened, and with which URL, without a headless test run
     *        actually launching a real browser every time (`TestMainWindow`
     *        overrides this to just record the call - see
     *        `test_main_window.cpp`).
     * @param url The `file://` URL to open.
     */
    virtual void openExternalUrl(const QUrl& url);

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
     * - Each recognized-audio-extension file (see `isAudioExtension()`)
     *   among `event`'s files gets its own `audioSnippetsForFile()` check;
     *   one with more than one snippet
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
     * @brief Wires `dialog`'s own offsetChanged() to recompute `path`'s
     *        own snippet split and push it back in via `setSnippets()` -
     *        real-world testing pass finding #23, shared by every call
     *        site that shows an `AudioSnippetPickerDialog` (importAudio(),
     *        and dropEvent()'s own per-file loop).
     *
     * `dialog` isn't UI-agnostic of file access the way `AudioSnippetPickerDialog`
     * itself deliberately stays (see its own docs on why) - this is exactly
     * the caller-side half of that division of responsibility: `MainWindow`
     * already has the file path and project, so it recomputes and pushes
     * the result back in, rather than the dialog reaching for either
     * itself.
     *
     * @param dialog The dialog to wire - must outlive this connection
     *        (i.e. still be on the stack, about to have `exec()` called on
     *        it), the same lifetime every other per-dialog `connect()` in
     *        this codebase already assumes.
     * @param path The audio file `dialog`'s own snippets were computed
     *        from, re-read on every offset change.
     */
    void wireSnippetOffsetRecompute(AudioSnippetPickerDialog& dialog, const std::filesystem::path& path);

    /**
     * @brief Sets canvas_'s own tool mode to `mode` (or `None` if
     *        `enabled` is `false`), and syncs paintAction_/pickAction_/
     *        selectAction_'s own checked state so exactly `activated` (or
     *        none of them) ends up checked - the shared exclusivity
     *        behind setPaintModeEnabled()/setPickModeEnabled()/
     *        setSelectModeEnabled(), since `CanvasWidget::ToolMode` only
     *        ever has one active value at a time.
     *
     * Deliberately hand-managed rather than a `QActionGroup`: a group's
     * own exclusivity would fire *two* `toggled()` calls per click (the
     * newly-checked action's own, and the now-unchecked previous one's)
     * in an order Qt doesn't document as stable, and each of the four
     * handlers above trusting only its own late-arriving call could stomp
     * on another's `canvas_->setToolMode()` result depending on that
     * order - a real bug, caught before it shipped (see
     * `docs/sound-mind-architecture.md`'s Decisions Made). Setting every
     * action's checked state directly and unconditionally (blocked, so
     * this doesn't recurse back into any of the four callers) also keeps
     * the toolbar buttons correctly in sync when one of them is called
     * directly (e.g. by a test), not just via a real click.
     *
     * @param activated Which action to leave checked when `enabled` is
     *        `true` - `paintAction_`, `pickAction_`, `selectAction_`,
     *        `pathAction_`, or `chordAction_`.
     * @param enabled Whether `activated`'s own tool mode should become
     *        active.
     * @param mode The tool mode `activated` corresponds to.
     */
    void setExclusiveToolMode(QAction* activated, bool enabled, CanvasWidget::ToolMode mode);

    /**
     * @brief Recomputes every UI guardrail for `docs/sound-mind-design.md`'s
     *        "Mind Grains" ordering rule ("only paintable on a layer above
     *        its own source") - `v0.Y.33.1` Installment B.
     *
     * Called whenever either input to the check could have changed: the
     * active layer (`layerController_->paintTargetLayerId()` - Layers Panel
     * selection changing, or a fresh `setProject()`), or the currently
     * configured tool (`toolConfigurationPanel_->toolConfigurationChanged()`
     * - a type switch, a different Mind Grain picked, or any other edit).
     *
     * Drives every layer of the guardrail at once, all from this single
     * recomputation:
     * - `toolConfigurationPanel_->setActiveLayer()` - its own red-highlight/
     *   tooltip on the Mind Grain group (see that method's own docs).
     * - `layersPanel_->setDisallowedLayers()` - a red "✕" on every layer
     *   at-or-below the configured Mind Grain's own source, whenever the
     *   configured tool actually is one; an empty list (clearing every
     *   mark) otherwise.
     * - `paintAction_->setEnabled()`/`setToolTip()` - disabled, with an
     *   explanatory tooltip, whenever the configured tool is a Mind Grain
     *   not usable on the *active* layer specifically; force-deactivates
     *   Paint mode first (`setPaintModeEnabled(false)`) if it was currently
     *   checked, so a user can never be left with Paint mode still active
     *   on a now-disabled button. Re-enabled, tooltip cleared, otherwise.
     *
     * A no-op-safe default (nothing disabled/marked) whenever no project is
     * open, no layer is active yet, or the configured tool isn't a Mind
     * Grain at all.
     */
    void updateMindGrainGuardrails();

    /**
     * @brief Enables/disables Configure Devices' own input and output
     *        device pickers to match whether an engine currently has that
     *        device actually open - preserves the "locked while running"
     *        safety behavior `RecordPanel`'s/`LoopPanel`'s own now-removed
     *        device pickers used to provide locally (real-world testing
     *        pass, 2026-09-20, finding #7).
     *
     * Input is locked while either `recordEngine_` or `loopEngine_` is
     * active - both share the panel's own one input device/gain (see
     * `ConfigureDevicesPanel`'s own class docs). Output is locked only
     * while `loopEngine_` is running - ordinary Playback was never a
     * reason to lock the output picker even before this milestone (see
     * `setOutputDeviceSelectionEnabled()`'s own docs on preserving that
     * exact distinction).
     *
     * Called after every place this app starts or stops Recording/Loop
     * Mode, so the picker's own enabled state never goes stale.
     */
    void updateConfiguredDeviceLockState();

    /// @brief Pushes `undoStack_`'s own current descriptions/position into
    ///        `historyPanel_` - `v0.Y.46.1` Installment D ("History
    ///        Panel"). Called wherever `undoStack_` might just have
    ///        changed: after any `LayerController::layersChanged()` (every
    ///        property mutation already pushes there) and after any
    ///        `ToolPaletteController::contentChanged()` (every content
    ///        commit, undo, or redo already emits it - see `PaintController::
    ///        rebuildLayerContentAndCascade()`'s own docs).
    void refreshHistoryPanel();

    /**
     * @brief Rebuilds `composerPanel_`'s own track list from the current
     *        project - Composer Mode, `v0.Y.48.1` Installment A.
     *
     * One track per visible layer, topmost first, each carrying its own
     * freshly-rendered `sound_mind::core::renderLayerAmplitudeSummary()`/
     * `renderLayerThumbnail()` images and every operation currently
     * targeting it (`OperationLog::activeOperationsTargeting()`, each
     * normalized to a `[0, 1]` fraction of the project's own total canvas
     * duration). Called wherever `refreshHistoryPanel()` already is
     * (the same `layersChanged()`/`contentChanged()` signals cover every
     * case that could change what a track ought to show), plus in
     * `setProject()`.
     *
     * A deliberate, acknowledged inefficiency for this first, view-only
     * installment: both images are always recomputed for every track on
     * every refresh, even for a track whose currently-selected background
     * style doesn't need one - `composerPanel_` itself owns which style
     * each track is showing (session-only UI state, never round-tripped
     * back here), so there's no way to skip computing the one it doesn't
     * currently need without threading that state back out of the panel,
     * which would cost more than it saves for this installment's own
     * modest scope.
     */
    void refreshComposerPanel();

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

    /// @brief Sets the window title to "Sound Mind Studio v<version>",
    /// plus " - <project name>" (currentPath_'s own file stem) once a
    /// project has been saved/opened at a real path - called after every
    /// currentPath_ assignment (createProjectAt()/openProjectAt()/
    /// saveProjectAs()).
    void updateWindowTitle();

    /// @brief loopUpdateTimer_'s slot: refreshes the Loop layer's content
    /// and repaints the canvas, and shows loopEngine_->loopsBehind() in the
    /// status bar, while Loop Mode is running - see toggleLoopMode()'s
    /// docs. As of `v0.0.41.1` (Loop Mode Live Preview), prefers
    /// loopEngine_->currentPreviewImage() (the in-progress loop's own
    /// growing preview) whenever it has content, falling back to
    /// loopEngine_->currentImage() (the last *completed* loop) right after
    /// a loop boundary - see currentPreviewImage()'s own docs.
    void updateLoopLayer();

    /// @brief recordDrainTimer_'s slot: moves whatever's newly captured
    /// out of recordEngine_'s ring buffer, while Recording is running -
    /// see RecordEngine::drainAvailable()'s docs for why this needs to
    /// happen periodically rather than only once recording stops.
    void drainRecording();

    /// @brief testInputLevelTimer_'s slot: drains deviceTestRecordEngine_'s
    /// own ring buffer (so it never overflows, same reasoning as
    /// drainRecording()) and pushes its currentInputLevel() into
    /// configureDevicesPanel_'s own level meter, while testing an input
    /// device. `v0.0.42.1`.
    void pollTestInputLevel();

    /// @brief exportProgressTimer_'s slot: a no-op while isExportRunning()
    /// (still polling); once the background encode finishes, stops the
    /// timer, hides exportCancelButton_, and reports the outcome per
    /// exportOutcome_ - success (a status bar message naming the
    /// destination, phrased per exportKind_), cancelled (deletes the
    /// necessarily-partial output file first, then a status bar message),
    /// or failed (`QMessageBox::critical()`, matching exportVideo()'s/
    /// exportAudio()'s own prior synchronous failure presentation). See
    /// exportTopmostLayerVideoAsync()'s/exportTopmostLayerAudioAsync()'s own
    /// docs. `v0.0.45.15`, finding #12 Installment C (video), generalized
    /// to cover audio too in Installment E.
    void pollExportProgress();

    /// @brief importProgressTimer_'s slot: a no-op while isImportRunning()
    /// (still polling); once the background encode finishes, stops the
    /// timer, hides importCancelButton_, and reports the outcome per
    /// importOutcome_ - success (moves importedLayers_ into project_ one by
    /// one, then the usual canvas/playback-invalidate/refresh-panel/status-
    /// message sequence importAudioSnippets() already does), cancelled
    /// (discards importedLayers_ without ever touching project_ - see
    /// importAudioSnippetsAsync()'s own docs on why that's the whole
    /// rollback needed), or failed (`QMessageBox::critical()`). See
    /// importAudioSnippetsAsync()'s own docs. Finding #12 Installment F.
    void pollImportProgress();

    /// @brief poolProgressTimer_'s slot: a no-op while isPoolRunning()
    /// (still polling); once the background compute finishes, stops the
    /// timer, hides poolCancelButton_, and reports the outcome per
    /// poolOutcome_ - success (re-resolves poolLayerId_ via
    /// layerController_->layerById() and applies pooledContent_ to it, then
    /// the usual canvas/playback-invalidate/status-message sequence
    /// poolTopmostLayerNow() already does - or a status message noting the
    /// layer is gone, if it no longer exists), cancelled (discards
    /// pooledContent_ without ever touching the layer - see
    /// poolTopmostLayerAsync()'s own docs on why that's the whole rollback
    /// needed), or failed (`QMessageBox::critical()`). Finding #12
    /// Installment G.
    void pollPoolProgress();

    /// @brief compositeProgressTimer_'s slot: a no-op while
    /// isCompositingForPlayback() (still polling); once the background
    /// composite finishes, stops the timer, hides
    /// compositeCancelButton_, and reports the outcome per
    /// compositeOutcome_ - success (loads compositedResult_ into
    /// playbackController_ and starts playback, the same
    /// load()-then-play() sequence startPlayback() always did
    /// synchronously - or a status message noting there was nothing to
    /// play, if compositedResult_ ended up empty), cancelled (discards
    /// compositedResult_ without ever touching playbackController_ - see
    /// startPlayback()'s own docs on why that's the whole rollback
    /// needed), or failed (`QMessageBox::critical()`). Finding #12
    /// Installment H.
    void pollCompositeProgress();

    /**
     * @brief Repeat Playback's/one-shot preview's shared edit hook -
     *        connected to `toolPaletteController_::contentChanged()`
     *        (which already merges every content-changing action:
     *        Paint/Pick/Fill/Paste/a stamped sequence, undo, and
     *        redo). A no-op with no `project_` open; otherwise gated by
     *        `repeatEnabled_` and `playbackScope_` together - see the two
     *        cases below.
     *
     * Re-renders the project's own current composite and reloads it
     * (`PlaybackController::load()` stops playback first, per its own
     * docs - "audible restart on edit", confirmed with the user over
     * building genuine seamless live double-buffering into
     * `PlaybackEngine`), then repositions and resumes per
     * `playbackScope_`:
     * - `Track`: keeps the pre-edit position
     *   (`currentPlaybackPositionSeconds_`) - the edit is heard "in
     *   place", playback never jumps. Only reacts while `repeatEnabled_`
     *   *and* playback is already active - Track has no narrower "the
     *   edit" region to preview on its own.
     * - `Delta`/`Review`: jumps to `layer`'s own most recently active
     *   operation's own `bounds().startTimeSeconds` - a practical
     *   approximation of "what just changed" that works uniformly for a
     *   fresh paint stroke *and* an undo/redo (neither of which has a
     *   freshly-appended operation of its own to read `bounds()` from) -
     *   see this method's own definition for the exact reasoning. Reacts
     *   to *every* edit regardless of `repeatEnabled_` or whether playback
     *   was already active - per `docs/sound-mind-design.md`'s "Repeat
     *   Playback" section, this jump-on-edit behavior is driven by Scope
     *   alone; Repeat only decides what happens once the edited range's
     *   own end is reached (see `checkRepeatPlaybackRange()`'s own docs).
     *   With Repeat off and nothing already playing, this is what starts
     *   a fresh, automatic one-shot preview of the just-made edit.
     *
     * @param layer Which layer changed.
     */
    void handleContentChangedForPlayback(sound_mind::core::LayerId layer);

    /**
     * @brief Repeat Playback's/one-shot preview's shared range-end check -
     *        called from the existing `PlaybackController::positionChanged()`
     *        poll (already running at ~30fps while playing).
     *
     * Once `positionSeconds` reaches `repeatRangeEndSeconds_`: with
     * `repeatEnabled_`, seeks back to `repeatLoopBackSeconds_` and resumes
     * - the "loops... when it reaches the end" half of Repeat Playback;
     * without it, pauses right there instead (`pause()`, not `stop()` - the
     * playhead stays put rather than resetting to `0.0`) - per
     * `docs/sound-mind-design.md`'s "Repeat Playback" section, Delta/Review
     * "halts... depending on the repeat checkbox" once their own range
     * ends, regardless of which way that check falls; only the "or
     * repeats" half is conditional on it. Both branches cover `Track`,
     * `Delta`, and `Review` alike with no per-scope branching needed here
     * at all (the per-scope difference for `Review` - looping back to the
     * track's start rather than the edit's - is already baked into
     * `repeatLoopBackSeconds_` by handleContentChangedForPlayback()).
     *
     * A no-op whenever `repeatRangeEndSeconds_ <= repeatRangeStartSeconds_`
     * - not just the pre-edit "no range yet" default (both `0.0`), but
     * also a genuinely zero-width `Delta`/`Review` range: a single-click
     * (as opposed to dragged) paint stroke's own `bounds()` has an equal
     * start/end, which `handleContentChangedForPlayback()` copies straight
     * into `repeatRangeStartSeconds_`/`repeatRangeEndSeconds_`. Since
     * `PlaybackController::seek()` emits `positionChanged()` synchronously,
     * re-entering this same method, treating a zero-width range as
     * "already past the end" would `seek()` back to its own start over
     * and over with no base case - unbounded recursion until the stack
     * overflows (a real, since-fixed crash - see `CHANGELOG.md`'s
     * `v0.0.42.3` entry). Nothing meaningful to loop over just plays
     * straight through instead, with no halt or loop-back for that edit.
     *
     * @param positionSeconds The current playback position, in seconds.
     */
    void checkRepeatPlaybackRange(double positionSeconds);

    /**
     * @brief openUserDocIfBundled(), plus a `QMessageBox::information()`
     *        fallback (mentioning this project's own GitHub repository) if
     *        the doc isn't bundled - the actual interactive behavior behind
     *        every Help-menu action and Landing Page documentation button
     *        (`v0.0.42.4`, Workflow & Device Polish, Installment D).
     *
     * Untestable directly, like every other modal-showing interactive slot
     * in this file (see `importAudio()`'s own docs on the same pattern) -
     * `openUserDocIfBundled()`'s own headless-safe half is what
     * `test_main_window.cpp` actually exercises.
     *
     * @param htmlFilename Forwarded to openUserDocIfBundled() unchanged.
     * @param friendlyName A human-readable name for the doc (e.g. "the
     *        Quick Start guide"), used only in the fallback message.
     */
    void openUserDocOrShowFallback(const QString& htmlFilename, const QString& friendlyName);

    std::optional<sound_mind::core::Project> project_;
    std::optional<std::filesystem::path> currentPath_;

    /// @brief Backing flag for hasUnsavedChanges() - see its own docs for
    /// what sets and clears it.
    bool hasUnsavedChanges_ = false;

    /// @brief Alternates between landingPage_ (index 0, shown until a
    /// project exists) and canvasScrollArea_ (index 1) - see
    /// setProject()'s docs.
    QStackedWidget* stack_ = nullptr;
    LandingPage* landingPage_ = nullptr;
    CanvasWidget* canvas_ = nullptr;

    /// @brief Clips/scrolls canvas_ once it's larger than the visible
    /// area (Canvas Navigation's own Zoom feature,
    /// docs/sound-mind-design.md) - see CanvasWidget::zoomModeChanged()'s
    /// own docs for how its setWidgetResizable() stays in sync with
    /// canvas_'s own current zoom mode.
    QScrollArea* canvasScrollArea_ = nullptr;

    /// @brief Shows the cursor's position (widget pixels and, when a
    /// project is open, time/frequency) in the status bar's normal
    /// (left-hand) area, live - added via `statusBar()->addWidget()`, not
    /// `addPermanentWidget()`, in the constructor. Kept up to date by
    /// `canvas_`'s `cursorMoved()`/`cursorLeft()` signals; can be
    /// temporarily covered by a `statusBar()->showMessage()` call
    /// elsewhere in this class (e.g. "Imported ...") - an accepted,
    /// standard `QStatusBar` behavior (temporary messages sit in front of
    /// the normal-area widgets), not a bug.
    QLabel* cursorPositionLabel_ = nullptr;

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

    /// @brief The `AudioDeviceMode` this window was constructed with - see
    /// the constructor's own docs. Stored so setProject() can construct
    /// each project's `loopEngine_` with the same mode `playbackController_`/
    /// `recordEngine_` already used, rather than that one construction site
    /// silently reverting to the `Real` default.
    sound_mind::core::AudioDeviceMode audioDeviceMode_ = sound_mind::core::AudioDeviceMode::Real;

    /// @brief Owns the PlaybackEngine and its position-polling timer -
    /// extracted from a plain member + free-standing timer/flag as part of
    /// the Phase 2.5 Refactor & Clean Up milestone (`v0.Y.23.1`). Loading
    /// the project's own real composite (as of `v0.Y.27.1` - see
    /// startPlayback()'s own docs) and wiring its signals to
    /// playbackPanel_/canvas_ both stay MainWindow's own job - see
    /// PlaybackController's own docs for why it doesn't know about either.
    PlaybackController* playbackController_ = nullptr;

    /// @brief The single, unified undo/redo history behind the Edit menu's
    ///        Undo/Redo actions - see its own class docs. Owned by value
    ///        (not `new`'d like the controllers below) since nothing else
    ///        needs to own it and it has no Qt parent-ownership of its
    ///        own to participate in; `toolPaletteController_`/
    ///        `layerController_` each get a non-owning pointer to it at
    ///        construction. `clear()`ed in setProject() - see that
    ///        method's own body.
    UndoStack undoStack_;

    /// @brief Owns the four Paint/Pick/Select/Path tool controllers and
    /// all of their wiring to `canvas_`/`toolConfigurationPanel_` - see its
    /// own class docs. Extracted out of this class as part of the
    /// Refactor & Clean Up milestone (`v0.Y.29.1`, Installment C); "which
    /// layer" a freehand gesture targets (`paintTargetLayerId()`) and the
    /// four toolbar `QAction`s' own mutual exclusivity
    /// (`setExclusiveToolMode()`) both stay this class's own job - see the
    /// class docs' own `v0.Y.29.1` note.
    ToolPaletteController* toolPaletteController_ = nullptr;

    /// @brief The toolbar's Paint tool toggle - checked while the canvas
    /// accepts freehand paint input (`CanvasWidget::ToolMode::Paint`).
    /// Kept as a member (rather than a local in the constructor) so
    /// setProject() can uncheck it when a new/different project is
    /// opened, the same way it resets every other per-project session
    /// state.
    QAction* paintAction_ = nullptr;

    /// @brief The toolbar's Pick tool toggle - checked while the canvas
    /// accepts Pick input (`CanvasWidget::ToolMode::Pick`). Kept mutually
    /// exclusive with paintAction_/selectAction_/pathAction_ by
    /// setExclusiveToolMode() (see its own docs), not a `QActionGroup`;
    /// kept as a member for the same setProject()-resets-it reason as
    /// paintAction_.
    QAction* pickAction_ = nullptr;

    /// @brief The toolbar's Select tool toggle - checked while the canvas
    /// accepts Select input (`CanvasWidget::ToolMode::Select`). Kept
    /// mutually exclusive with paintAction_/pickAction_/pathAction_ by
    /// setExclusiveToolMode(); kept as a member for the same
    /// setProject()-resets-it reason as paintAction_.
    QAction* selectAction_ = nullptr;

    /// @brief The toolbar's Path tool toggle - checked while the canvas
    /// accepts Path input (`CanvasWidget::ToolMode::Path`). Kept mutually
    /// exclusive with paintAction_/pickAction_/selectAction_ by
    /// setExclusiveToolMode(); kept as a member for the same
    /// setProject()-resets-it reason as paintAction_.
    QAction* pathAction_ = nullptr;

    /// @brief The toolbar's Chord Stamp tool toggle - checked while the
    /// canvas accepts Chord Stamp input (`CanvasWidget::ToolMode::ChordStamp`).
    /// Kept mutually exclusive with paintAction_/pickAction_/selectAction_/
    /// pathAction_ by setExclusiveToolMode(); kept as a member for the same
    /// setProject()-resets-it reason as paintAction_.
    QAction* chordAction_ = nullptr;

    /// @brief The toolbar's "Smooth Nodes" checkable toggle - the Path
    /// tool's own standing default node type (see
    /// `PathController::setDefaultNodeType()`'s own docs), independent of
    /// (and not reset by) which tool mode is currently active.
    QAction* smoothNodesAction_ = nullptr;

    /// @brief The Edit menu's Image Mode checkable toggle (`v0.Y.47.1`,
    /// Principal Modes) - checked while the current project's own
    /// `PrincipalMode` is `Image`. Kept as a member since, unlike Hardware
    /// Acceleration's own app-wide toggle, this reflects *per-project*
    /// state and must be re-synced (via a `QSignalBlocker`-guarded
    /// `setChecked()`, the same pattern its own initial-state application
    /// already uses) every time `setProject()` loads a different project.
    QAction* principalModeAction_ = nullptr;

    /// @brief The dockable panel exposing the current paint tool's own
    /// parameters - see its own class docs for what's deliberately not
    /// built yet (the Wizard button, the Tool Preset drop-down). Hidden
    /// by default, matching Playback/Record/Loop's own "off until shown"
    /// convention (not Layers', which is shown automatically once).
    ToolConfigurationPanel* toolConfigurationPanel_ = nullptr;

    /// @brief The dockable panel exposing the Chord Generator's own
    /// parameters - see its own class docs. Hidden by default, the same
    /// "off until shown" convention toolConfigurationPanel_ already
    /// follows. `v0.0.40.2` (Chords/Arpeggiator/Sequencer, Installment B).
    ChordGeneratorPanel* chordGeneratorPanel_ = nullptr;

    /// @brief The dockable panel consolidating input/output device
    /// selection, gain, and testing into one place - see its own class
    /// docs. Hidden by default, the same "off until shown" convention
    /// toolConfigurationPanel_ already follows. `v0.0.42.1` (Workflow &
    /// Device Polish, Installment A).
    ConfigureDevicesPanel* configureDevicesPanel_ = nullptr;

    /// @brief The dockable panel exposing Select mode's own Selection Type
    /// (Rectangle/Lasso) - see its own class docs. Hidden by default, the
    /// same "off until shown" convention toolConfigurationPanel_ already
    /// follows.
    SelectionConfigurationPanel* selectionConfigurationPanel_ = nullptr;

    /// @brief The dockable panel exposing canvas-overlay settings - Axis
    /// Labels for now, growing into Overlay Grids/Snap to Grid in later
    /// installments of this same milestone (see its own class docs).
    /// Hidden by default, the same "off until shown" convention
    /// toolConfigurationPanel_ already follows.
    GridPanel* gridPanel_ = nullptr;

    /// @brief The dockable panel exposing the currently selected Filter
    /// layer's own parameters - see its own class docs. Hidden by
    /// default, the same "off until shown" convention
    /// toolConfigurationPanel_ already follows; disabled (not just
    /// hidden) whenever `layersPanel_`'s own current selection isn't a
    /// `Filter`-type layer at all - see `handleLayerSelectionChanged()`'s
    /// own docs.
    FilterConfigurationPanel* filterConfigurationPanel_ = nullptr;

    /// @brief Owns layer-stack lookup/mutation and Layers Panel/Filter
    /// Configuration Panel refresh - see its own class docs. Extracted out
    /// of this class as part of the Refactor & Clean Up milestone
    /// (`v0.Y.29.1`, Installment D).
    LayerController* layerController_ = nullptr;

    /// @brief The dockable panel managing the current project's MindWave
    /// library - see its own class docs (`v0.Y.31.1` Installment C2).
    /// Hidden by default, the same "off until shown" convention
    /// filterConfigurationPanel_ already follows.
    MindWavesPanel* mindWavesPanel_ = nullptr;

    /// @brief The dockable read-only history panel - see its own class
    /// docs (`v0.Y.46.1` Installment D). Hidden by default, the same
    /// reasoning mindWavesPanel_ above already gives.
    HistoryPanel* historyPanel_ = nullptr;

    /// @brief The dockable, read-only Composer Mode track view - see its
    /// own class docs (`v0.Y.48.1` Installment A). Hidden by default, the
    /// same reasoning mindWavesPanel_ above already gives. Bottom-docked,
    /// not right-docked like every other panel here - confirmed with the
    /// user, matching the design doc's own "alongside the direct
    /// spectrogram canvas" framing (a bottom timeline sits beneath the
    /// canvas rather than competing with it for the same side strip every
    /// narrow property panel already shares).
    ComposerPanel* composerPanel_ = nullptr;

    /// @brief Owns MindWave-library add/remove/rename/edit and
    /// MindWavesPanel/LayersPanel refresh - see its own class docs
    /// (`v0.Y.31.1` Installment C2).
    MindWaveController* mindWaveController_ = nullptr;

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

    /// @brief Constructed in the member-initializer list (not a fixed
    /// in-class default) since it needs `audioDeviceMode_`'s value, which
    /// is only known once the constructor's own parameter is available.
    sound_mind::core::RecordEngine recordEngine_;
    QTimer* recordDrainTimer_ = nullptr;

    /// @brief A dedicated `RecordEngine` used *only* for the Configure
    /// Devices panel's own "test an input device" affordance - never the
    /// same instance `recordEngine_` uses for real recording, so testing a
    /// device never interferes with (or is interfered by) an in-progress
    /// real recording/loop session. Started/stopped by
    /// toggleTestInputDevice(); polled by testInputLevelTimer_ while
    /// running. `v0.0.42.1` (Workflow & Device Polish, Installment A).
    sound_mind::core::RecordEngine deviceTestRecordEngine_;

    /// @brief Polls deviceTestRecordEngine_'s own currentInputLevel() into
    /// configureDevicesPanel_'s own level meter while testing an input
    /// device - same 100ms cadence as recordDrainTimer_, for the same
    /// "nothing visual depends on finer granularity than that" reasoning.
    QTimer* testInputLevelTimer_ = nullptr;

    /// @brief Plays the Configure Devices panel's own "test an output
    /// device" tone - see its own class docs. `v0.0.42.1`.
    sound_mind::core::DeviceTestTonePlayer deviceTestTonePlayer_;

    /// @brief The Configure Devices panel's own currently configured input/
    /// output device names, tracked here (rather than read back from
    /// `configureDevicesPanel_` itself, which exposes no such getter) so
    /// toggleTestInputDevice()/toggleTestOutputDevice() know which device to
    /// actually open for testing - kept up to date by
    /// setConfiguredInputDevice()/setConfiguredOutputDevice(). Empty means
    /// the system default, the same convention every device name in this
    /// class already follows. **Also** what setProject() re-applies to each
    /// freshly (re)constructed `loopEngine_` - see its own docs on why that
    /// matters (real-world testing pass, 2026-09-20, finding #10).
    QString configuredInputDeviceName_;
    QString configuredOutputDeviceName_;

    /// @brief The Configure Devices panel's own currently configured input
    /// gain, tracked here for the same reason as
    /// configuredInputDeviceName_/configuredOutputDeviceName_ just above -
    /// `recordEngine_`/`deviceTestRecordEngine_` are persistent members that
    /// never lose a `setInputGain()` call, but `loopEngine_` is rebuilt
    /// fresh by setProject() on every new/opened project, so this is what
    /// setProject() re-applies to it. `1.0f` (unity) is the same default
    /// `LoopEngine`/`RecordEngine` themselves start at.
    float configuredInputGain_ = 1.0f;

    /// @brief Repeat Playback's own current state - see
    /// setPlaybackRepeat()'s/setPlaybackScope()'s own docs. `v0.0.42.2`.
    bool repeatEnabled_ = false;
    sound_mind::studio::PlaybackScope playbackScope_ = sound_mind::studio::PlaybackScope::Track;

    /// @brief The currently active playback range, in seconds - `[0,
    /// totalSeconds()]` (the whole track) right after startPlayback()/a
    /// manual seekPlayback(); narrowed to the edited operation's own
    /// bounds() by handleContentChangedForPlayback() while `playbackScope_`
    /// is `Delta`/`Review` - independent of `repeatEnabled_`, which only
    /// decides what checkRepeatPlaybackRange() does once this range's own
    /// end is reached (see its own docs), not whether the range itself is
    /// tracked.
    double repeatRangeStartSeconds_ = 0.0;
    double repeatRangeEndSeconds_ = 0.0;

    /// @brief Where checkRepeatPlaybackRange() seeks back to once playback
    /// reaches repeatRangeEndSeconds_, when `repeatEnabled_` - equal to
    /// repeatRangeStartSeconds_
    /// for `Track`/`Delta` (looping the same range it's built from), but
    /// `0.0` for `Review`: `Review`'s own range runs from the edit's start
    /// to the whole track's end (see docs/sound-mind-design.md's "Repeat
    /// Playback" - "reaches the end of the track... repeats from the start
    /// of the track"), so the loop-back target is the *track's* start, not
    /// the edit's. Kept in sync with repeatRangeStartSeconds_ everywhere
    /// the latter is reset to `0.0` (a fresh load, a manual seek, Repeat
    /// turned off), since those all mean "no active Delta/Review range".
    double repeatLoopBackSeconds_ = 0.0;

    /// @brief The most recently reported playback position, in seconds -
    /// tracked here (PlaybackController exposes no positionSeconds()
    /// getter, only the positionChanged() signal) so
    /// handleContentChangedForPlayback() knows where to resume from for
    /// `PlaybackScope::Track` (which never jumps on an edit).
    double currentPlaybackPositionSeconds_ = 0.0;

    /// @brief The currently running export (video or audio), if any - see
    /// exportTopmostLayerVideoAsync()'s/exportTopmostLayerAudioAsync()'s own
    /// docs on why both kinds share this one slot. `nullptr` whenever
    /// isExportRunning() is `false` (including before the first export ever
    /// starts) - reset once pollExportProgress() has finished reporting a
    /// run's own outcome, not left holding a finished task around.
    /// `v0.0.45.15`, finding #12 Installment C (video); generalized to cover
    /// audio too in Installment E.
    std::unique_ptr<sound_mind::core::BackgroundTask> exportTask_;
    QTimer* exportProgressTimer_ = nullptr;
    QPushButton* exportCancelButton_ = nullptr;

    /// @brief Which kind of export exportTask_ actually is - purely so
    /// pollExportProgress() can phrase its own status bar message
    /// correctly ("audio"/"video").
    enum class ExportKind { Video, Audio };
    ExportKind exportKind_ = ExportKind::Video;

    /// @brief Where the currently (or most recently) running export is/was
    /// writing to - pollExportProgress()'s own docs on why this is deleted
    /// rather than kept on cancellation.
    std::filesystem::path exportPath_;

    /// @brief exportTask_'s own work function's outcome, written just
    /// before it returns and read by pollExportProgress() only after
    /// observing isExportRunning() == false - safe without its own
    /// separate synchronization thanks to BackgroundTask::isRunning()'s own
    /// release/acquire pairing (see its docs). Reset to `Success` at the
    /// start of every new exportTopmostLayerVideoAsync()/
    /// exportTopmostLayerAudioAsync() call, not left holding a stale value
    /// from whatever the previous run's own outcome was.
    enum class ExportOutcome { Success, Cancelled, Failed };
    ExportOutcome exportOutcome_ = ExportOutcome::Success;
    QString exportErrorMessage_;

    /// @brief The currently running audio import, if any - see
    /// importAudioSnippetsAsync()'s own docs. A separate slot from
    /// exportTask_ - see its own docs on why the two aren't shared.
    /// Finding #12 Installment F.
    std::unique_ptr<sound_mind::core::BackgroundTask> importTask_;
    QTimer* importProgressTimer_ = nullptr;
    QPushButton* importCancelButton_ = nullptr;

    /// @brief Where the currently (or most recently) running import is
    /// reading from - purely for pollImportProgress()'s own status bar
    /// message (naming the source file), unlike exportPath_ there's
    /// nothing to delete here on cancellation.
    std::filesystem::path importPath_;

    /// @brief importTask_'s own work function's result on success - the
    /// encoded-but-not-yet-added layers, moved into project_ one by one by
    /// pollImportProgress() only once the whole encode succeeded, or
    /// discarded untouched if it was cancelled. Written just before the
    /// work function returns and read only after observing
    /// isImportRunning() == false - safe without its own synchronization
    /// for the same release/acquire reason exportOutcome_ is (see
    /// BackgroundTask::isRunning()'s own docs). Cleared at the start of
    /// every new importAudioSnippetsAsync() call.
    std::vector<sound_mind::core::Layer> importedLayers_;

    enum class ImportOutcome { Success, Cancelled, Failed };
    ImportOutcome importOutcome_ = ImportOutcome::Success;
    QString importErrorMessage_;

    /// @brief The currently running Pool, if any - see
    /// poolTopmostLayerAsync()'s own docs. A separate slot from
    /// exportTask_/importTask_ - see its own docs on why none of the three
    /// are shared. Finding #12 Installment G.
    std::unique_ptr<sound_mind::core::BackgroundTask> poolTask_;
    QTimer* poolProgressTimer_ = nullptr;
    QPushButton* poolCancelButton_ = nullptr;

    /// @brief Which layer poolTask_ is pooling - re-resolved via
    /// layerController_->layerById() once the background compute finishes,
    /// rather than a raw `Layer*` held across it (`Project::layers()` could
    /// reallocate while it runs). `std::nullopt` whenever isPoolRunning()
    /// is `false`.
    std::optional<sound_mind::core::LayerId> poolLayerId_;
    QString poolLayerName_;  ///< poolLayerId_'s own name, for pollPoolProgress()'s status message.

    /// @brief poolTask_'s own work function's result on success - written
    /// just before it returns and read only after observing
    /// isPoolRunning() == false, safe for the same release/acquire reason
    /// exportOutcome_/importedLayers_ are (see BackgroundTask::isRunning()'s
    /// own docs). `std::nullopt` on cancellation or failure - nothing to
    /// apply.
    std::optional<sound_mind::core::PooledContent> pooledContent_;

    enum class PoolOutcome { Success, Cancelled, Failed };
    PoolOutcome poolOutcome_ = PoolOutcome::Success;
    QString poolErrorMessage_;

    /// @brief The currently running playback composite, if any - see
    /// startPlayback()'s own docs. A separate slot from exportTask_/
    /// importTask_/poolTask_ - see poolTask_'s own docs on why none of
    /// these are shared. Finding #12 Installment H.
    std::unique_ptr<sound_mind::core::BackgroundTask> compositeTask_;
    QTimer* compositeProgressTimer_ = nullptr;
    QPushButton* compositeCancelButton_ = nullptr;

    /// @brief compositeTask_'s own work function's result on success -
    /// written just before it returns and read only after observing
    /// isCompositingForPlayback() == false, safe for the same release/
    /// acquire reason exportOutcome_/pooledContent_ are (see
    /// BackgroundTask::isRunning()'s own docs). `std::nullopt` on
    /// cancellation, failure, or a composite with nothing to play.
    std::optional<sound_mind::codec::StreamImage> compositedResult_;

    enum class CompositeOutcome { Success, Cancelled, Failed };
    CompositeOutcome compositeOutcome_ = CompositeOutcome::Success;
    QString compositeErrorMessage_;
};

}  // namespace sound_mind::studio
