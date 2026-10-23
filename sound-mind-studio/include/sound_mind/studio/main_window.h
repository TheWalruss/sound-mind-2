#pragma once

#include <atomic>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <QColor>
#include <QMainWindow>
#include <QSettings>
#include <QUrl>

#include "sound_mind/codec/audio_export.h"
#include "sound_mind/codec/stream_incremental_encoder.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/background_task.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/device_test_tone_player.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/loop_engine.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_sources.h"
#include "sound_mind/core/record_engine.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/export_controller.h"
#include "sound_mind/studio/import_controller.h"
#include "sound_mind/studio/playback_preparer.h"
#include "sound_mind/studio/analysis_controller.h"
#include "sound_mind/studio/recording_controller.h"
#include "sound_mind/studio/loop_controller.h"
#include "sound_mind/studio/macro_controller.h"
#include "sound_mind/studio/branch_curve_session.h"
#include "sound_mind/studio/project_change.h"
#include "sound_mind/studio/analysis_panel.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/text_tool_panel.h"
#include "sound_mind/studio/configure_devices_panel.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/grid_panel.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/layer_controller.h"
#include "sound_mind/studio/macro_recorder.h"
#include "sound_mind/studio/midi_configuration_panel.h"
#include "sound_mind/studio/midi_import.h"
#include "sound_mind/studio/midi_import_dialog.h"
#include "sound_mind/studio/mind_wave_controller.h"
#include "sound_mind/studio/playback_controller.h"
#include "sound_mind/studio/playback_panel.h"
#include "sound_mind/studio/recent_projects.h"
#include "sound_mind/studio/selection_configuration_panel.h"
#include "sound_mind/studio/tool_configuration_panel.h"
#include "sound_mind/studio/tool_palette_controller.h"
#include "sound_mind/studio/project_history.h"
#include "sound_mind/studio/remaster.h"
#include "sound_mind/codec/video_options.h"
#include "sound_mind/core/macro_renderer.h"

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
class QToolButton;

namespace sound_mind::studio {

class MacroPlayer;

class ComposerPanel;
class CreateProjectWizard;
class HistoryPanel;
class LandingPage;
class LayersPanel;
class LoopPanel;
class MindWavesPanel;
class PlaybackPanel;
class RecordPanel;
class ResourceBrowserController;
class ResourceBrowserPanel;

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
 * stops both engines and LoopController's layer regardless, as a defensive
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
 * deleteLayer()/reorderLayers().
 *
 * **As of `v0.Y.12.1` (Loop Mode, renamed from Live Mode):** the original
 * `LiveEngine` is renamed/reimplemented as `sound_mind::core::LoopEngine` -
 * see its own docs for the fixed-length loop-pedal redesign. LoopController's engine
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
 * work. The Loop layer's refresh (renamed from updateLiveLayer(); in
 * LoopController since `v0.1.48.7`) additionally reports `LoopEngine::loopsBehind()` in
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
 * `paintTargetLayerId()`), mutation
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
    ///        (`playbackController_`, `recordEngine_`, LoopController's engine) -
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
     * the status bar (non-modal), since it can take a while; only a failure shows a modal.
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
     * reported via the status bar (non-modal), since it can take a while; only a failure shows a modal.
     */
    void importImage();

    /**
     * @brief Prompts for a Standard MIDI File, then - when there's a real
     *        choice to make - which of its channels/project-length
     *        snippets to import and whether to keep them in separate
     *        layers, and imports the result as new, editable layer(s) -
     *        see `docs/sound-mind-design.md`'s "Import" and
     *        `docs/sound-mind-roadmap.md`'s MIDI track import milestone
     *        (`v0.Y.55.1`).
     *
     * Skips `MidiImportDialog` entirely for a file with exactly one
     * channel and one computed snippet (nothing to choose either way),
     * matching `importAudio()`'s own "trivial case, skip the picker"
     * precedent - imports every channel, into separate layers, via
     * importMidiFile() directly in that case. Otherwise shows
     * `MidiImportDialog` (built from `midiImportPreviewForFile()`'s own
     * result) and imports only the checked channels/snippets, with the
     * dialog's own "Separate layer per channel" choice, via
     * importMidiSelection(); cancelling it, or accepting with nothing
     * checked in either list, imports nothing.
     *
     * **Every channel still plays through a plain default Tool
     * Configuration** - mapping a channel/program to one of the project's
     * own saved Tool Presets is the still-unbuilt "MIDI Configuration
     * panel" installment's job, not this one's.
     */
    void importMidi();

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

    /// @brief Whether Delta/Review playback of an edit is still being
    ///        prepared (`v0.1.47.1`): the composite and decode run on a
    ///        background task, so the edit itself returns at once; playback
    ///        starts when it's ready. A newer edit replaces a pending one.
    /// @return `true` from the edit until the result has been applied (or
    ///         discarded).
    [[nodiscard]] bool isPreparingEditPlayback() const noexcept;

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
     * @brief Sets whether the Sound Flower polar view is active - the
     *        actual work behind the View menu's own Sound Flower checkable
     *        action (`Ctrl+Alt+F`), `docs/sound-mind-roadmap.md`'s
     *        `v0.Y.53.1` (Sound Flower).
     *
     * Forwards to `canvas_->setPolarMode()` (see its own docs for what
     * actually changes on screen and in mouse handling) and re-applies
     * Snap to Grid (`applyGridSnapping()`) - suspended while polar mode is
     * active, matching the legacy Python Studio's own precedent (a grid
     * overlay drawn as straight lines has no meaningful "snap to" target
     * once it's suppressed for not translating onto a disk - see
     * `CanvasWidget::setPolarMode()`'s own docs on why the grid overlay
     * itself is suppressed there). Not persisted across sessions - a
     * fresh project always opens in flat view, matching the legacy
     * Studio's own "the toggle state is not saved with the project"
     * precedent.
     *
     * @param enabled Whether polar view should be active.
     */
    void setSoundFlowerModeEnabled(bool enabled);

    /**
     * @brief Sets whether Repeat Playback is active - the actual work
     *        behind the Playback panel's own Repeat checkbox (`v0.0.42.2`,
     *        Workflow & Device Polish, Installment B).
     *
     * Does *not* gate whether an edit re-renders and jumps under Delta/
     * Review scope - see `handleContentChangedForPlayback()`'s own docs on
     * why that's Scope's job alone, not this flag's. What this *does* gate
     * is only what happens once the active range's own end is reached:
     * loops back to its own start (pushed to the engine at once, even when
     * the range itself doesn't change - see `PlaybackPreparer::setRepeat()`,
     * which this forwards to since `v0.1.48.4`) while `true`, halts there while `false` - and, for `Track`
     * scope specifically, whether an edit re-renders in place at all
     * (`Track` has no narrower region to preview on its own, so with
     * Repeat off it does nothing). Turning Repeat off immediately widens
     * the active range back to the whole track (`[0, totalSeconds()]`), so
     * nothing about a stale Delta/Review range lingers once it's
     * unchecked.
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
     * (LoopController's engine doesn't exist until setProject() has been called at
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
     * actually captured - encoded and saved in the background
     * (commitRecordingAsync(), `v0.1.47.1`), so the layer appears a moment
     * later; stopping with nothing captured (e.g. no input device was
     * available) leaves the project unchanged.
     *
     * Marks hasUnsavedChanges() when a layer is actually added. Per the
     * Project Lifecycle milestone (`v0.Y.10.1`), New/Open/Close all refuse
     * outright while Recording is running - see toggleLoopMode()'s docs.
     */
    void toggleRecording();

    /**
     * @brief Re-queries available input/output devices and refreshes both
     *        `configureDevicesPanel_`'s own device pickers **and**
     *        `landingPage_->deviceConfiguration()`'s own (`v0.Y.62.1`
     *        Installment H - two independent `DeviceConfigurationWidget`
     *        instances, see its own class docs) - the only pickers left
     *        in the app otherwise (real-world testing pass, 2026-09-20,
     *        finding #7) - the actual work behind either instance's own
     *        "Refresh Devices" button (`v0.0.42.1`). Never opens or closes
     *        a device itself - see `sound_mind::core::
     *        availableAudioDeviceNames()`'s own docs for how a fresh scan
     *        already works without one.
     */
    void refreshConfiguredDevices();

    /**
     * @brief Sets the input device `recordEngine_`'s and LoopController's engine's
     *        (if a project is open) *next* start() should each open - the
     *        actual work behind the Configure Devices panel's own input
     *        device picker (`v0.0.42.1`) - now the only input device
     *        picker in the app (real-world testing pass, 2026-09-20,
     *        finding #7; see `ConfigureDevicesPanel`'s own class docs on
     *        why this applies to both engines at once).
     *
     * **As of `v0.Y.62.1` Installment H**, also syncs the *other*
     * `DeviceConfigurationWidget` instance's own displayed selection
     * (`configureDevicesPanel_`'s own, or `landingPage_->deviceConfiguration()`'s
     * own - whichever didn't originate this call) via
     * `setSelectedInputDevice()`, which blocks its own signal, so this is
     * always safe to call on both unconditionally.
     * @param deviceName The device to prefer, or empty for the system
     *        default.
     */
    void setConfiguredInputDevice(const QString& deviceName);

    /// @brief Sets the output device `playbackController_`'s and
    /// LoopController's engine's (if a project is open) should each switch to - the
    /// actual work behind the Configure Devices panel's own output device
    /// picker (`v0.0.42.1`) - now the only output device picker in the app
    /// (real-world testing pass, 2026-09-20, finding #7). See
    /// setConfiguredInputDevice()'s own docs for the same "applies to both
    /// engines at once" reasoning, and (`v0.Y.62.1` Installment H) the same
    /// "sync both DeviceConfigurationWidget instances unconditionally" one.
    /// @param deviceName The device to prefer, or empty for the system
    ///        default.
    void setConfiguredOutputDevice(const QString& deviceName);

    /// @brief Sets `recordEngine_`'s and LoopController's engine's (if a project is
    /// open) own input gain - the actual work behind the Configure Devices
    /// panel's own input gain slider (`v0.0.42.1`). As of `v0.Y.62.1`
    /// Installment H, also syncs the Landing Page's own
    /// `DeviceConfigurationWidget` instance's displayed gain - see
    /// setConfiguredInputDevice()'s own docs on this same dual-sync shape.
    /// @param percent `[0, ConfigureDevicesPanel::kMaxGainPercent]` (200) -
    ///        `100` is unity gain.
    void setConfiguredInputGain(int percent);

    /// @brief Sets `playbackController_`'s own output gain, **and**
    /// `deviceTestTonePlayer_`'s own gain (`v0.Y.62.1` Installment A) - the
    /// actual work behind the Configure Devices panel's own output gain
    /// slider (`v0.0.42.1`). The former forwards straight to
    /// setPlaybackVolume() (the same underlying `PlaybackController::
    /// setVolume()`) - `PlaybackPanel`'s own, now-removed volume slider
    /// used to need syncing here too (real-world testing pass, 2026-09-20,
    /// finding #7: it was a pure duplicate of this same gain). The latter
    /// fixes a second real-world-testing-pass finding (2026-10-04): the
    /// "Test" tone used to always play at a fixed amplitude, entirely
    /// ignoring this slider, since `DeviceTestTonePlayer` is deliberately
    /// independent of `PlaybackController`/`PlaybackEngine` (see its own
    /// class docs) and so was never reached by it at all.
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
     * **As of `v0.Y.62.1` Installment H**, may be triggered by either
     * `DeviceConfigurationWidget` instance (the dock's own, or the
     * Landing Page's own); both own "Test" buttons are kept checked/
     * unchecked in sync regardless of which one the user actually
     * clicked, and both own level meters reset together on stop.
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
     * currently names. See toggleTestInputDevice()'s own docs on keeping
     * both `DeviceConfigurationWidget` instances' own "Test" buttons in
     * sync (`v0.Y.62.1` Installment H).
     *
     * @param testing `true` to start playing the test tone, `false` to
     *        stop.
     */
    void toggleTestOutputDevice(bool testing);

    /**
     * @brief Prompts for a destination file and exports the whole
     *        project's audio - the full composite Playback hears - to it
     *        (see exportCompositeAudioAsync()).
     *
     * The chosen format (Flac/Ogg/MP3) is inferred from the destination
     * file's extension. Does nothing if no layer has content, or none is
     * open. Progress and completion are reported via the status bar
     * (non-modal), since it can take a while; only a failure shows a modal.
     */
    void exportAudio();

    /**
     * @brief Prompts for a destination file and exports the whole project
     *        as an MP4 video (see exportCompositeVideoAsync()): the full
     *        composite canvas animated with a playhead synced to the
     *        composite audio.
     *
     * Does nothing if no layer has content, or none is open. Progress and
     * completion are reported via the status bar (non-modal), since it can take a while; only a failure shows a
     * modal.
     */
    void exportVideo();

    /**
     * @brief Macro → Export Macro Video...: picks a recorded Macro (if more
     *        than one) and a destination file, then exportMacroVideoAsync().
     */
    void exportMacroVideo();

    /// @brief File -> Export Image... (`v0.1.52.1`): asks where, in which
    ///        format and at what size (full resolution by default) in an
    ///        ImageExportDialog, then exports in the background - see
    ///        exportImageAsync().
    void exportImage();

    /// @brief Exports the canvas as an image in the background - forwards
    ///        to `ExportController::exportImageAsync()`.
    /// @param path Destination; its extension picks the format.
    /// @param options Size, view and quality.
    void exportImageAsync(const std::filesystem::path& path, const ImageExportOptions& options);

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

    /// @brief Hides every layer except `keepVisible` - the Layers Panel's
    ///        own right-click "Hide other layers" (`v0.Y.62.1`
    ///        Installment F). Forwards straight to
    ///        `LayerController::hideOtherLayers()`; `hasUnsavedChanges()`
    ///        and the History/Composer panels already refresh via the
    ///        same `LayerController::layersChanged()` connection every
    ///        other layer-visibility mutator here relies on, so there's
    ///        nothing extra to do here. Deliberately not recorded as a
    ///        Macro event (unlike cycleLayerVisibilityState()) - doing so
    ///        correctly would need `MacroEventType` to carry a whole list
    ///        of affected layers, not just one id; out of scope for this
    ///        installment.
    /// @param keepVisible The right-clicked layer's own id.
    void hideOtherLayers(sound_mind::core::LayerId keepVisible);

    /// @brief Restores every layer hideOtherLayers() last hid - the
    ///        Layers Panel's own right-click "Unhide other layers"
    ///        (`v0.Y.62.1` Installment F). Forwards straight to
    ///        `LayerController::unhideOtherLayers()` - see
    ///        hideOtherLayers()'s own docs on bookkeeping/Macro scope.
    void unhideOtherLayers();

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
     * @brief Makes `id`'s own Filter/Equalizer configuration immediately
     *        editable - the actual work behind `LayersPanel`'s own "edit
     *        filter" (⚙) button, real-world testing pass 2026-09-27
     *        ("editing a filter layer should be immediately accessible
     *        from the layers panel").
     *
     * Two steps, both already-established mechanisms reused rather than
     * duplicated: `layersPanel_->selectLayer(id)` (which fires
     * selectionChanged() into `LayerController::handleLayerSelectionChanged()`,
     * already responsible for loading a selected Filter/Equalizer layer's
     * own configuration into `filterConfigurationPanel_` - see that
     * method's own docs), then `filterConfigurationPanel_->show()`/
     * `raise()`, since nothing previously surfaced that panel automatically
     * if it happened to be hidden (its own toolbar toggle is "off by
     * default" - see its own docs).
     *
     * @param id The Filter/Equalizer layer to edit. A no-op for any other
     *        layer type or an unknown id - `LayersPanel` itself only ever
     *        emits this for a row where `sound_mind::core::isFilterLayerType()`
     *        is true, and `selectLayer()` is already a no-op for an unknown
     *        id (see its own docs).
     */
    void editFilterLayer(sound_mind::core::LayerId id);

    /**
     * @brief Shows and raises `configureDevicesPanel_` - the actual work
     *        behind the "Configure Devices" button each of
     *        `playbackPanel_`/`recordPanel_`/`loopPanel_` now carries
     *        (`v0.Y.58.1`, "Reduce top-level buttons"), replacing
     *        `configureDevicesPanel_`'s own now-removed standalone toolbar
     *        toggle. The same "surface an off-by-default panel
     *        automatically rather than leaving the user to find its own
     *        toggle" pattern `editFilterLayer()` already established.
     */
    void showConfigureDevicesPanel();

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
     * silent, project-dimensioned placeholder (LoopController's engine's `emptyImage()`
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
     * @brief Opens `GeneratorDialog` and, if accepted, adds a new
     *        generated layer via `LayerController::addGeneratedLayer()`
     *        - `docs/sound-mind-design.md`'s "Generators", `v0.Y.51.1`.
     *        A no-op (dialog never shown) if no project is open.
     */
    void generateLayer();

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
     * @brief Turns the Equalizer's own on-canvas preview overlay on or
     *        off - the actual work behind `FilterConfigurationPanel`'s own
     *        "Preview on Canvas" checkbox (`docs/sound-mind-roadmap.md`'s
     *        "Equalizer usability", flagged 2026-09-27 from real-world
     *        use).
     *
     * A sticky, session-only preference (like `layersPanel_`'s own
     * visibility across project switches) - stays on across New/Open
     * Project and across switching `FilterConfigurationPanel`'s own
     * selection away from the Equalizer layer and back, re-resolving the
     * Equalizer layer's own *current* gradient fresh via
     * updateEqualizerPreview() each time, rather than freezing whatever
     * gradient happened to be loaded at the moment this was last toggled.
     *
     * @param enabled The checkbox's own new checked state.
     */
    void setEqualizerPreviewEnabled(bool enabled);

    /**
     * @brief Prompts for a name and appends a new entry to the current
     *        project's own convolution kernel library, refreshing
     *        `filterConfigurationPanel_`'s own Load Kernel combo - the
     *        actual work behind `FilterConfigurationPanel::
     *        saveConvolutionKernelRequested()`.
     *
     * Defaults the prompt to "Kernel 1", "Kernel 2", and so on (still
     * editable), the same `QInputDialog` naming prompt `saveFilterPreset()`/
     * `createResonanceFromPickedPathNamed()` already use - confirmed with
     * the user ("saving convolution kernels doesn't give the user the
     * option to name them"), replacing this method's own former silent
     * auto-naming. A no-op if no project is open, or the user cancels/
     * leaves the name empty.
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
     * @brief Prompts for a name and appends a new entry to the current
     *        project's own Filter Preset library - the actual work behind
     *        `FilterConfigurationPanel::savePresetRequested()`. A no-op if
     *        no project is open, or the user cancels/leaves the name
     *        empty.
     * @param config The configuration to save - the panel's own current,
     *        complete configuration at the moment "Save..." was clicked.
     */
    void saveFilterPreset(const sound_mind::core::FilterConfiguration& config);

    /**
     * @brief Feeds the current project's own Filter Preset library into
     *        `filterConfigurationPanel_->setAvailableFilterPresets()` -
     *        called whenever a project is opened/created and after
     *        `saveFilterPreset()` appends a new entry, the same role
     *        `refreshConvolutionKernelCombo()` plays for convolution
     *        kernels.
     */
    void refreshFilterPresetCombo();

    /**
     * @brief Prompts for a name (resolving any collision via
     *        `sound_mind::studio::promptSaveName()`) and appends - or, on
     *        Replace, overwrites in place - an entry in the current
     *        project's own Grid Preset library, reading `gridPanel_`'s
     *        own current configuration - the actual work behind
     *        `GridPanel::savePresetRequested()`. A no-op if no project is
     *        open, or the user cancels the name prompt.
     */
    void saveGridPreset();

    /**
     * @brief Removes the given entry from the current project's own Grid
     *        Preset library and refreshes `gridPanel_`'s own combo - the
     *        actual work behind `GridPanel::deletePresetRequested()`.
     * @param id The Grid Preset to remove.
     */
    void deleteGridPreset(sound_mind::core::GridPresetId id);

    /**
     * @brief Feeds the current project's own Grid Preset library into
     *        `gridPanel_->setAvailableGridPresets()` - called whenever a
     *        project is opened/created and after `saveGridPreset()`/
     *        `deleteGridPreset()` change the library, the same role
     *        `refreshFilterPresetCombo()` plays for Filter Presets.
     */
    void refreshGridPresetCombo();

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
     * @brief Selects the neutral "Pan" entry of the Tool dropdown - plain,
     *        non-interactive canvas display (`CanvasWidget::ToolMode::
     *        None`), added in `v0.Y.58.1`'s "Reduce top-level buttons" UI
     *        polish pass as the Tool dropdown's own explicit default so a
     *        user can navigate the canvas without inadvertently painting,
     *        picking, or placing path nodes.
     *
     * Unlike setPaintModeEnabled()/setPickModeEnabled()/
     * setSelectModeEnabled()/setPathModeEnabled() below, there is no
     * in-progress gesture to cancel - Pan has no gesture of its own at all.
     * See setPaintModeEnabled()'s own docs for the shared exclusivity with
     * Paint/Pick/Select/Path/Text.
     *
     * @param enabled `true` to select Pan; `false` has no effect of its own
     *        (falling back to Pan happens automatically, via
     *        setExclusiveToolMode(), whenever another tool is turned off).
     */
    void setPanModeEnabled(bool enabled);

    /**
     * @brief Turns the canvas's Paint tool on or off - the actual work
     *        behind the Tool dropdown's Paint entry.
     *
     * Turning it off cancels any in-progress stroke (see
     * `PaintController::cancelStroke()`'s own docs) rather than leaving
     * it dangling; turning it on has no effect if no project is open.
     *
     * Pan/Paint/Pick/Select/Path/Text share one canvas tool mode
     * (`CanvasWidget::ToolMode`) and so can never have more than one active
     * - enforced by setExclusiveToolMode() (see its own docs for why that's
     * hand-managed rather than a `QActionGroup`), not repeated here or in
     * setPanModeEnabled()/setPickModeEnabled()/setSelectModeEnabled()/
     * setPathModeEnabled().
     *
     * @param enabled `true` to accept freehand paint input on the canvas;
     *        `false` to return to plain, non-interactive display (Pan).
     */
    void setPaintModeEnabled(bool enabled);

    /**
     * @brief Toggles between Pick and plain (`ToolMode::None`) canvas
     *        interaction - the actual work behind the Tool dropdown's Pick
     *        entry.
     *
     * Turning it off clears the current selection (see `PickController::
     * clearSelection()`'s own docs) rather than leaving it dangling;
     * turning it on has no effect if no project is open. See
     * setPaintModeEnabled()'s own docs for the shared exclusivity with
     * Pan/Paint/Select/Path/Text.
     *
     * @param enabled `true` to accept Pick input on the canvas; `false`
     *        to return to plain, non-interactive display (Pan).
     */
    void setPickModeEnabled(bool enabled);

    /**
     * @brief Toggles between Select and plain (`ToolMode::None`) canvas
     *        interaction - the actual work behind the Tool dropdown's
     *        Select entry.
     *
     * Turning it off cancels any in-progress selection drag (see
     * `SelectionController::cancelSelectionDrag()`'s own docs) - a
     * *committed* selection stays exactly as it is, since it scopes
     * Fill/Cut/Copy/Paste independent of which tool is currently active
     * (`docs/sound-mind-design.md`'s own "Selection" framing); turning it
     * on has no effect if no project is open. See setPaintModeEnabled()'s
     * own docs for the shared exclusivity with Pan/Paint/Pick/Path/Text.
     *
     * @param enabled `true` to accept Select input on the canvas; `false`
     *        to return to plain, non-interactive display (Pan).
     */
    void setSelectModeEnabled(bool enabled);

    /**
     * @brief Toggles between Path and plain (`ToolMode::None`) canvas
     *        interaction - the actual work behind the Tool dropdown's Path
     *        entry.
     *
     * Turning it off cancels any in-progress node placement (see
     * `PathController::cancelPath()`'s own docs) - discarded, not
     * committed; turning it on has no effect if no project is open. See
     * setPaintModeEnabled()'s own docs for the shared exclusivity with
     * Pan/Paint/Pick/Select/Text.
     *
     * @param enabled `true` to accept Path input on the canvas; `false` to
     *        return to plain, non-interactive display (Pan).
     */
    void setPathModeEnabled(bool enabled);

    /**
     * @brief Turns the Text tool on or off - the Tool dropdown's "Text"
     *        (Pre-Beta Plan `v0.Y.78.1`). On, it shows the Text Tool panel;
     *        off, it discards any box not yet applied. See `TextController`.
     * @param enabled `true` to place text on the canvas.
     */
    void setTextModeEnabled(bool enabled);

    /**
     * @brief Measures the project's loudness - the Analysis panel's Analyze
     *        button (Pre-Beta Plan `v0.Y.80.1`). Composites, decodes and
     *        measures the mix and each audible layer on a background task
     *        (`sound_mind::core::analyzeProjectLoudness()`), then shows the
     *        report in the Analysis panel. A no-op with no project open or
     *        while an analysis is already running.
     */
    void analyzeLoudness();

    /**
     * @brief Turns the master limiter on or off and sets its ceiling - in
     *        playback, Loop Mode (since `v0.1.40.6`) and exports (Pre-Beta
     *        Plan `v0.Y.80.1`). Remembered across sessions; on, at -1 dBFS,
     *        by default.
     * @param enabled Whether to limit.
     * @param ceilingDb The ceiling, dBFS.
     */
    void setMasterLimiter(bool enabled, double ceilingDb);

    /**
     * @brief Turns output loudness normalization on or off and sets its
     *        target (Pre-Beta Plan `v0.Y.80.1`): the playback output is turned
     *        up or down by `target - integrated loudness` of the loaded mix
     *        (within -24..+12 dB), measured each time playback audio is
     *        loaded or refreshed - so it holds steady across pause and resume.
     *        Playback only; exports keep the mix's own level. Remembered
     *        across sessions; off, at -14 LUFS, by default.
     * @param enabled Whether to normalize.
     * @param targetLufs The target loudness, LUFS.
     */
    void setLoudnessNormalization(bool enabled, double targetLufs);

    /// @brief Whether the current project's Loop Mode output is limited - a
    ///        test hook for setMasterLimiter().
    /// @return `false` with no project open (no Loop engine yet).
    [[nodiscard]] bool loopLimiterEnabled() const noexcept;

    /// @brief The playback output's current normalization gain.
    /// @return The gain, dB (`0` when off).
    [[nodiscard]] double normalizationGainDb() const noexcept;

    /// @brief Whether a loudness analysis is running.
    /// @return `true` from analyzeLoudness() until its report arrives.
    [[nodiscard]] bool isAnalyzingLoudness() const noexcept { return analysisController_->isAnalyzing(); }

    /**
     * @brief Turns the Analysis panel's Auto-update on or off (`v0.1.40.7`).
     *        While on, every project change (the same ones that mark the
     *        report out of date) cancels a running analysis at once and
     *        starts a fresh one after 300 ms without further
     *        changes - so a burst of edits costs one analysis. Turning it on
     *        analyzes straight away when the report is missing or out of
     *        date; opening a project analyzes it. Remembered across sessions;
     *        off by default.
     * @param enabled Whether to re-analyze automatically.
     */
    void setAutoAnalyze(bool enabled);

    /// @brief How many running analyses a change has cancelled - a test
    ///        hook for setAutoAnalyze().
    /// @return The count since this window was built.
    [[nodiscard]] int cancelledAnalysisCount() const noexcept { return analysisController_->cancelledCount(); }

    /**
     * @brief Undoes the latest History entry, if any - the actual work
     *        behind the Edit menu's Undo action.
     *
     * Delegates to `history_` (`ProjectHistory`, `v0.1.43.1`): every
     * recorded action is undoable - imports, recordings, layer changes,
     * painting, resources - in the order it happened, and the history is
     * saved with the project, so undo works after reopening it too. A
     * no-op if nothing is undoable.
     */
    void undo();

    /// @brief Redoes the next undone History entry, if any - the actual
    ///        work behind the Edit menu's Redo action. Delegates to
    ///        `history_`; a no-op if nothing is redoable.
    void redo();

    /**
     * @brief Jumps directly to an arbitrary point in the project's History -
     *        the actual work behind `HistoryPanel`'s double-click gesture.
     *
     * Delegates to `ProjectHistory::jumpTo()`, which undoes or redoes each
     * entry in between; the controllers and handleHistoryChange() refresh
     * everything affected, and the History panel follows
     * `ProjectHistory::historyChanged()`.
     *
     * @param index The target position - how many entries stay applied.
     */
    void jumpToHistoryIndex(std::size_t index);

    /**
     * @brief Saves audio as a lossless project source (`v0.1.43.2`) - a
     *        recording or a Loop take - and registers it.
     * @param audio The audio.
     * @param kind See `ProjectSource::kind`.
     * @param name A descriptive name.
     * @return The source's id; `std::nullopt` without a project folder, for
     *         empty audio, or if writing failed.
     */
    std::optional<sound_mind::core::SourceId> storeAudioSource(const sound_mind::codec::AudioBuffer& audio,
                                                               const std::string& kind, const QString& name);

    /**
     * @brief File → Remaster... (`v0.1.43.4`): asks for new settings and
     *        replacement sources in a `RemasterDialog`, rebuilds the project
     *        as a new one (remasterTo()), then offers to open it. Explains
     *        instead for a project whose history doesn't start at its
     *        creation.
     */
    void remaster();

    /**
     * @brief Rebuilds the open project as a new one, per `request`, on a
     *        background thread behind a cancellable progress dialog - the
     *        work behind remaster(), callable directly (tests).
     * @param request What to change, and where to save.
     * @param errorMessage Receives the reason on failure; may be `nullptr`.
     * @return `true` if the new project was built and saved.
     */
    bool remasterTo(const sound_mind::studio::RemasterRequest& request, QString* errorMessage = nullptr);

    /// @brief After opening a project, warns if any layer made from a
    ///        recipe has no content (its media file is missing), pointing
    ///        at Remaster to rebuild it (`v0.1.43.4`).
    void warnAboutMissingContent();

    /// @brief Commits the open project's changes to its History with
    ///        `description` (`v0.1.43.1`) - a no-op without a project.
    /// @param description What the user did.
    void commitHistory(const QString& description);

    /**
     * @brief Refreshes what the controllers don't after an undo, redo or
     *        History jump (`v0.1.43.1`): resource combos and panels, the
     *        record targets, the Equalizer preview, the Composer panel.
     * @param change What changed.
     */
    void handleHistoryChange(const sound_mind::core::HistoryChange& change);

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

    /**
     * @brief Paints the current tool configuration along the Picked path's
     *        every branch, as one undoable step - the Edit menu's and the
     *        right-click menu's "Stroke Path with Current Tool" (Pre-Beta
     *        Plan `v0.Y.77.1`). The guide path itself stays. A no-op (with a
     *        status-bar note) if nothing suitable is Picked or the layer
     *        can't be painted on.
     */
    void strokePickedPath();

    /**
     * @brief Merges the Picked paths into one branching path - the Edit
     *        menu's "Merge Picked Paths into Branching Path" and the
     *        multi-selection right-click menu's "Merge into Branching Path"
     *        (Pre-Beta Plan `v0.Y.77.1`). See
     *        `PickController::mergeSelectionIntoBranchingPath()`. A no-op
     *        unless two or more Picked objects have a path.
     */
    void mergePickedPaths();

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

    /**
     * @brief Builds and shows the canvas's own right-click context menu
     *        - "right-clicking on an object on the canvas, or an empty
     *        part of the canvas... a small menu... with likely actions
     *        to take" (direct user feedback). Connected to
     *        `CanvasWidget::contextMenuRequested()`.
     *
     * Attempts a Pick at `point` first (the same `ToolPaletteController::
     * beginPick()` a real left-click-to-Pick already calls), then shows
     * one of two small, fixed menus depending on whether that found
     * something: **Edit Path**, **Bring to Front**, **Send to Back**,
     * **Delete** for a Picked object; or **Paste**, **Fill Selection...**,
     * **Apply Filter to Selection**, **Deselect** otherwise (acting on the
     * separate rectangle/lasso Selection or the clipboard - these stay
     * "always present, no-op when inapplicable", the same as their own
     * Edit menu entries; this doesn't check whether a Selection actually
     * covers `point` first). Graying out whichever entries don't
     * actually apply is a deliberate, separate next step.
     *
     * @param point The right-click position, converted to time/frequency
     *        space.
     * @param globalPos Screen-global position to pop the menu up at.
     */
    void showCanvasContextMenu(sound_mind::core::TimeFrequencyPoint point, QPoint globalPos);

    /**
     * @brief Plays a preview of `toolConfigurationPanel_`'s own current
     *        configuration - direct user feedback: "practically
     *        wherever there is a visual preview of something, give the
     *        user the ability to play an audio preview of whatever it
     *        is." Connected to `ToolConfigurationPanel::previewRequested()`.
     *
     * Synthesizes the same representative 3-second stroke the Resource
     * Browser's own Tool Preset preview uses
     * (`sound_mind::core::toolConfigurationPreviewStreamImage()`),
     * decodes it, and plays it through `toolConfigurationPreviewEngine_`
     * - a dedicated engine, independent of the Studio's own main
     * playback path, the same "its own isolated engine" precedent
     * `ResourceBrowserController`'s own preview engine already
     * establishes. A no-op with no project open.
     */
    void previewToolConfiguration();

    /// @brief Stops `toolConfigurationPreviewEngine_` and resets
    ///        `toolConfigurationPanel_`'s own Play/Stop buttons - see
    ///        previewToolConfiguration()'s own docs. Connected to
    ///        `ToolConfigurationPanel::stopPreviewRequested()`.
    void stopToolConfigurationPreview();

    /**
     * @brief Starts or stops the filter preview - the Filter Configuration
     *        panel's "Loop Preview" button (Pre-Beta Plan `v0.Y.76.1`).
     *
     * Starting fixes the looped window once
     * (`sound_mind::core::filterPreviewWindow()`: the selection's time
     * range, or a few seconds from the playback position), pauses main
     * playback, and renders that window in the background
     * (`sound_mind::core::renderFilterPreview()`) with the panel's current
     * configuration aimed at the selected Filter layer, the selection, or
     * the whole mix. The result loops on a dedicated `PlaybackEngine`;
     * every later edit of the configuration (and every change of selected
     * layer) re-renders it, and the new audio is swapped in at the next
     * loop-back with `PlaybackEngine::stageAudio()`, so there's no gap.
     *
     * Stops on its own - unchecking the button - if there's nothing to
     * hear, when main playback starts, or when the project changes. A
     * no-op start with no project open.
     *
     * @param enabled `true` to start looping, `false` to stop.
     * @note GUI thread only; rendering never runs on it.
     */
    void setFilterPreviewEnabled(bool enabled);

    /// @brief Whether the filter preview is on - see setFilterPreviewEnabled().
    /// @return `true` from the start until it stops.
    [[nodiscard]] bool isFilterPreviewActive() const noexcept { return filterPreviewActive_; }

    /// @brief How many filter preview renders have reached the preview
    ///        engine since this window was created - lets a caller (or a
    ///        test) tell that an edit has been heard.
    /// @return The count.
    [[nodiscard]] int filterPreviewRendersCompleted() const noexcept { return filterPreviewRenders_; }

    /// @brief The length of the audio the filter preview loops.
    /// @return The length in samples, or `0` before the first render.
    [[nodiscard]] std::size_t filterPreviewLengthSamples() const noexcept;

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
     * @brief Sets which decimal mark every numeric field shows and accepts
     *        (Edit -> Decimal Separator), and stores it in the open
     *        project's own settings - see `ProjectSettings::
     *        decimalSeparator`. Applied app-wide immediately; with no
     *        project open, only the display changes.
     * @param separator The mark to use.
     */
    void setDecimalSeparator(sound_mind::core::DecimalSeparator separator);

    /**
     * @brief Stores a finished recording where the Record panel says:
     *        replacing the content of the chosen recorded layer (one
     *        undoable step), or as a new layer with the panel's own name,
     *        marked as recorded so it's offered as a target next time.
     *        What toggleRecording() does when recording stops - public so
     *        it can be tested without a real input device.
     * @param captured The recorded audio.
     * @return `false` (nothing changes) with no project open or nothing
     *         captured.
     */
    bool commitRecording(const sound_mind::codec::AudioBuffer& captured);

    /**
     * @brief commitRecording() with the slow part - encoding the take and
     *        writing it to the project's sources as FLAC - on a background
     *        task (`v0.1.47.1`), so stopping a long recording doesn't
     *        freeze the window. What toggleRecording() uses when recording
     *        stops; the layer appears when it's done. New/Open/Close wait
     *        for it first (flushRecordingSave()), so a take is never lost.
     * @param captured The recorded audio. Ignored (with a status message)
     *        if nothing was captured or no project is open.
     */
    void commitRecordingAsync(sound_mind::codec::AudioBuffer captured);

    /// @brief Whether commitRecordingAsync()'s save is still pending.
    /// @return `true` until its result has been applied.
    [[nodiscard]] bool isSavingRecording() const noexcept;

    /// @brief Gives the Record panel the project's current recorded layers
    ///        as targets.
    void refreshRecordTargets();

    /**
     * @brief Enables each registered Edit-menu action only if it applies
     *        right now (something Picked, a selection, a clipboard, a path
     *        edit, a Branching Curve session...), and each Edit submenu
     *        only if at least one of its actions does. Runs every time the
     *        Edit menu opens; restoreEditActions() re-enables everything
     *        when it closes, so keyboard shortcuts - whose handlers are all
     *        safe no-ops when they don't apply - keep working regardless of
     *        when the state last changed.
     */
    void updateEditActionStates();

    /// @brief Re-enables every action and submenu updateEditActionStates()
    ///        may have disabled.
    void restoreEditActions();

    /**
     * @brief Sizes the canvas for Fit to Window. In Image Mode, the canvas
     *        gets the largest size that fits the visible area with square
     *        cells (one frame as wide as one bin is tall), centred with
     *        black bars - so a round brush stamp looks round. Otherwise it
     *        simply fills the visible area, as before. Manual zoom is left
     *        alone.
     */
    void updateCanvasFit();

    /**
     * @brief Macro → Record Macro (`v0.1.44.1`): starts recording a new
     *        Macro ("Macro N") into the project, or stops the one being
     *        recorded and reports its length and event count. A no-op with
     *        no project open. See `MacroRecorder`.
     * @param enabled `true` starts recording; `false` stops it.
     */
    void setMacroRecordingEnabled(bool enabled);

    /**
     * @brief Macro → Play Macro (`v0.1.44.3`): picks a recorded Macro and
     *        plays it with macroPlayer_ - rendered ahead on a copy, then
     *        played seamlessly on the canvas; the project isn't touched.
     *        While one plays, the action reads "Stop Macro" and stops it;
     *        the transport's Pause/Play/Stop control it too.
     */
    void playMacro();

    /// @brief Macro → Manage Macros...: rename or delete recorded Macros
    ///        (`MacroManagerDialog`).
    void manageMacros();

    /**
     * @brief Asks which recorded Macro to use, when there's more than one.
     * @param title The prompt's title.
     * @return The chosen Macro, or `std::nullopt` (none recorded, or
     *         cancelled - explaining the former).
     */
    std::optional<sound_mind::core::MacroId> chooseMacro(const QString& title);

    /// @brief Where the playhead is and whether the transport runs - Loop
    ///        Mode's own playhead while it runs, Playback's otherwise.
    /// @return Position (seconds) and whether playing.
    [[nodiscard]] std::pair<double, bool> macroPlayhead() const;

    /// @brief Reports the playhead to the Macro being recorded - on every
    ///        position update and transport change; see
    ///        MacroController::notePlayhead().
    void noteMacroPlayhead();

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
     * @brief Computes a Wave Kernel Signature spectrum from whichever
     *        path/curve is currently Picked and stores it as a new, named
     *        entry in the project's Resonance profile library -
     *        `docs/sound-mind-roadmap.md`'s "Resonant Instruments"
     *        (`v0.Y.59.1`) own item 2 ("With a Curve selected, the user
     *        selects the option to create a 'Resonance'"), the
     *        actual work behind the Edit menu's "Create Resonance from
     *        Picked Path..." action.
     *
     * **The workflow, in full**: draw or place a path as an ordinary paint
     * stroke, switch to Pick and click it (the same "stay in Pick, don't
     * switch back to Select" workflow usePickedPathAsMindWaveShape()'s own
     * docs describe), then choose Edit → Create Resonance from
     * Picked Path - prompts for a name (`QInputDialog`, the same plain
     * "just needs a name" mechanism `ToolConfigurationPanel::
     * saveCurrentAsToolPreset()` already uses, not the richer
     * `MindCaptureDialog` Mind Shot/Mind Grain use, since neither of their
     * own extra fields - a fundamental frequency, a start-time offset -
     * means anything for a computed spectrum). A no-op unless something is
     * currently Picked (`ToolPaletteController::selectedPath()`), or the
     * name prompt is cancelled/left empty.
     *
     * Shows the name prompt, then delegates the actual computation/storage
     * to createResonanceFromPickedPathNamed() - see that method's
     * own docs on why tests call it directly instead of this one (the
     * same `captureMindShot()`/`captureMindShotWithDetails()` split this
     * codebase already establishes for every modal-dialog-showing action).
     */
    void createResonanceFromPickedPath();

    /**
     * @brief The dialog-free half of createResonanceFromPickedPath()
     * - see `captureMindShotWithDetails()`'s own identical docs on why
     * this exists (so tests never have to call the real, modal-dialog-
     * showing slot directly).
     *
     * Resamples the picked `Path` into a `CurveGraph`
     * (`curveGraphFromPath()`, `kResonanceNodeCount` nodes) and
     * computes its own aggregated spectrum (`computeWaveKernelSignature()`,
     * `kResonanceSpectrumSize` samples) - both in `resonant_
     * instrument.h` - then stores the result via
     * `Project::addResonanceProfile()`. A permanent snapshot, the same as
     * `captureMindShot()`'s own `Clip` capture: editing the source path
     * afterward has no effect on the new library entry. A no-op if
     * nothing is currently Picked or no project is open.
     *
     * Explicitly calls `toolConfigurationPanel_->refreshResonanceProfiles()`
     * afterward - unlike `captureMindShot()`/`captureMindGrain()` (wired
     * through `SelectionController`'s own `mindShotCaptured()`/
     * `mindGrainCaptured()` signals, connected once in
     * `ToolPaletteController`'s own constructor), this capture writes
     * directly to `project_` from here, so no such signal already exists
     * to pick this one up automatically.
     *
     * @param name Display name for the new library entry.
     */
    void createResonanceFromPickedPathNamed(const std::string& name);

    /// @brief The resonance geometry of whatever is Picked: a branching
    ///        guide path's every branch (`curveGraphFromBranches()`), or a
    ///        single path (`curveGraphFromPath()`).
    /// @return The graph, or `std::nullopt` with nothing suitable Picked.
    [[nodiscard]] std::optional<sound_mind::core::CurveGraph> pickedResonanceGraph() const;

    /// @brief Keeps the Branching Curve session's branches as a branching
    ///        guide path on the trunk's own layer - called as the session
    ///        finishes into a Resonance Profile, so the tree stays on the
    ///        canvas.
    void keepBranchingCurveAsPath();

    /**
     * @brief Starts a new `BranchCurveSession`, using whatever's currently
     *        Picked as its own trunk - the first step of the Branching
     *        Curve editor (`docs/sound-mind-roadmap.md`'s "Resonant
     *        Instruments" item 1's own deferred half: "letting someone
     *        build a real tree/graph by hand... rather than only
     *        resampling an existing linear stroke"), the actual work
     *        behind the Edit menu's "Start Branching Curve from Picked
     *        Path" action.
     *
     * **The workflow, in full**: draw a trunk stroke as an ordinary paint
     * stroke, switch to Pick and click it, then choose this action. To
     * grow a branch: click a precise point on the trunk (or any already-
     * added branch) - every Pick click, while a session is active, is
     * reported to `branchCurveSession_` via `noteBranchCurveGraftCandidate()`
     * (see its own docs); draw a new stroke there, Pick it, then choose
     * Edit → Add Picked Path as Branch (`addPickedPathAsBranch()`).
     * Repeat for as many branches as wanted, then Edit → Create Resonance
     * from Picked Graph... (`createResonanceFromPickedGraph()`)
     * finalizes the whole tree. A no-op unless something is currently
     * Picked.
     *
     * Starting a session discards any previous one's own gathered
     * branches entirely - the same "starting fresh always wins" rule
     * `PaintController::beginStroke()`'s own docs establish for an
     * already-in-progress stroke, applied here to a whole session instead
     * of a single stroke.
     */
    void startBranchingCurveFromPickedPath();

    /**
     * @brief Adds whatever's currently Picked as a new branch of the
     *        in-progress `branchCurveSession_`, grafted wherever the most
     *        recent qualifying Pick click landed - the actual work behind
     *        the Edit menu's "Add Picked Path as Branch" action. See
     *        `startBranchingCurveFromPickedPath()`'s own docs for the
     *        workflow this fits into.
     *
     * A no-op (per `BranchCurveSession::addBranch()`'s own docs) unless a
     * session is currently active, something is currently Picked, *and* a
     * graft candidate is currently armed (`branchCurveSession_.hasPendingGraft()`)
     * - every branch past the trunk must be explicitly grafted onto an
     * already-added branch first; there is no "just append it loosely, not
     * connected to anything" fallback.
     */
    void addPickedPathAsBranch();

    /**
     * @brief Discards the in-progress `branchCurveSession_` entirely,
     *        without finalizing it - the actual work behind the Edit
     *        menu's "Cancel Branching Curve" action, for abandoning a
     *        tree part-way through rather than being forced to finish or
     *        restart from the trunk. A no-op if no session is active.
     */
    void cancelBranchingCurve();

    /**
     * @brief Computes a Wave Kernel Signature spectrum from the whole
     *        in-progress `branchCurveSession_` (every branch gathered so
     *        far, trunk included) and stores it as a new, named entry in
     *        the project's Resonance profile library - the
     *        genuinely branching counterpart to
     *        createResonanceFromPickedPath() above, via
     *        `curveGraphFromBranches()` instead of `curveGraphFromPath()`.
     *        The actual work behind the Edit menu's "Create Resonance
     *        from Picked Graph..." action.
     *
     * Prompts for a name the same way createResonanceFromPickedPath()
     * does, then delegates to createResonanceFromPickedGraphNamed() -
     * see that method's own docs on why tests call it directly instead of
     * this one. A no-op unless a session is currently active, or the name
     * prompt is cancelled/left empty.
     */
    void createResonanceFromPickedGraph();

    /**
     * @brief The dialog-free half of createResonanceFromPickedGraph() -
     *        see `createResonanceFromPickedPathNamed()`'s own
     *        identical docs on why this exists.
     *
     * Builds the branching `CurveGraph` via `curveGraphFromBranches()`
     * (`branchCurveSession_.branches()`, `kResonanceNodeCount`
     * nodes per branch) and computes its own aggregated spectrum
     * (`computeWaveKernelSignature()`, `kResonanceSpectrumSize`
     * samples), then stores the result via `Project::addResonanceProfile()`
     * and ends the session - a no-op (not ending the session) if no
     * session is currently active or no project is open.
     *
     * @param name Display name for the new library entry.
     */
    void createResonanceFromPickedGraphNamed(const std::string& name);

    /**
     * @brief Reports a Pick click to `branchCurveSession_`, if one is
     *        currently active - called from every place `ToolPaletteController::
     *        beginPick()` already is, right alongside it, so ordinary Pick
     *        behavior needs no change of its own to support this (see
     *        `BranchCurveSession::noteGraftCandidate()`'s own docs on why
     *        an unrelated click is always a safe no-op).
     * @param point The click position, in the same raw `TimeFrequencyPoint`
     *        space the click was already being forwarded in.
     */
    void noteBranchCurveGraftCandidate(sound_mind::core::TimeFrequencyPoint point);

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
     * Opens a `MindCaptureDialog` (`v0.Y.55.1`), pre-filled with an
     * auto-suggested name `"Mind Shot <N>"` (`N` one more than the
     * library's own current size) - unlike Copy/Cut's own no-dialog "quick
     * action" precedent, a real dialog is needed here because
     * `fundamentalFrequencyHz()`/`startTimeOffsetSeconds()` (this
     * milestone's own MIDI-import prerequisite) have no other UI to be set
     * from at all yet (see `NamedMindGrain`'s own "no library management UI
     * yet" note). The dialog is only ever shown when there's actually a
     * committed selection to capture (checked *before* `exec()`, via
     * `toolPaletteController_->hasSelection()`) - both so clicking the
     * menu action with nothing selected stays the same harmless no-op it
     * always was, and so a headless caller with no selection (the
     * existing regression test in particular) never blocks on a modal
     * dialog no one is there to close. Confirms success via a status-bar
     * message, the same lightweight feedback `importAudioFile()`/
     * `importImageFiles()` already give for a comparably one-off,
     * meaningfully-persistent action.
     */
    void captureMindShot();

    /**
     * @brief The dialog-free half of captureMindShot() - actually performs
     *        the capture given already-known details, without ever
     *        showing `MindCaptureDialog` itself. `captureMindShot()`
     *        gathers those details from the dialog and calls straight
     *        through to this; a caller that already knows what it wants
     *        (in particular, this codebase's own test suite - the same
     *        "test the dialog-free half, never the real modal-dialog-
     *        showing slot" precedent `importAudioSnippetsAsync()` already
     *        establishes for `importAudio()`) can call this directly
     *        instead.
     * @param name Display name for the new library entry.
     * @param fundamentalFrequencyHz See `SelectionController::
     *        captureMindShot()`'s own identical parameter docs. Defaults
     *        to `0.0` ("not set").
     * @param startTimeOffsetSeconds See `SelectionController::
     *        captureMindShot()`'s own identical parameter docs. Defaults
     *        to `0.0` ("no offset").
     */
    void captureMindShotWithDetails(const std::string& name, double fundamentalFrequencyHz = 0.0,
                                      double startTimeOffsetSeconds = 0.0);

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
     * `SelectionController::captureMindGrain()`'s own docs. Opens the same
     * `MindCaptureDialog` captureMindShot() does (auto-suggesting
     * `"Mind Grain <N>"`), only when there's a selection to capture (see
     * captureMindShot()'s own docs on why), and confirms success the same
     * way (a status-bar message).
     */
    void captureMindGrain();

    /// @brief The dialog-free half of captureMindGrain() - see
    ///        captureMindShotWithDetails()'s own identical docs.
    /// @param name Display name for the new library entry.
    /// @param fundamentalFrequencyHz Defaults to `0.0` ("not set").
    /// @param startTimeOffsetSeconds Defaults to `0.0` ("no offset").
    void captureMindGrainWithDetails(const std::string& name, double fundamentalFrequencyHz = 0.0,
                                       double startTimeOffsetSeconds = 0.0);
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
     * (import, a Loop/Recording capture adding or updating a layer,
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
    ///         project has ever been opened yet (LoopController's engine doesn't exist).
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
     * has actually finished successfully - see ImportController (`v0.1.48.3`;
     * this forwards to it). Cancelling therefore rolls back cleanly by construction: the
     * encoded-so-far layers are simply discarded, having never touched
     * `project_` in the first place - simpler than Export's own "delete
     * the partial file" rollback (Decision #136), since nothing persistent
     * or shared was ever written to begin with.
     *
     * A no-op (just a status bar message, no dialog) if no project is open,
     * or if isImportRunning() is already `true` - a second call would
     * otherwise collide with the running one. Also why
     * newProject()/openProject()/openProjectAt()/closeEvent() all refuse
     * outright while isImportRunning(): switching or closing the project
     * out from under a background import that will later call
     * `project_->addLayer()` on whatever `project_` turns out to be by
     * then would silently misattribute those layers to the wrong project.
     *
     * A separate job (ImportController's) from export's, not shared with it - unlike Video/Audio
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
     * @param polarParams See `importImageFilesInto()`'s own docs -
     *        meaningful only when `mode` is `ImageScalePickerDialog::
     *        Mode::Polar`.
     * @return `true` if at least one file was imported; `false` if no
     *         project is open, `paths` is empty, or every file failed.
     */
    bool importImageFiles(const std::vector<std::filesystem::path>& paths, ImageScalePickerDialog::Mode mode,
                           bool importAsSequence, QString* errorMessage = nullptr,
                           std::optional<PolarImportParams> polarParams = std::nullopt);

    /**
     * @brief Imports a Standard MIDI File's every note-bearing channel as
     *        new, editable layer(s), without prompting or showing an error
     *        dialog on failure - the testable core behind `importMidi()`'s
     *        file dialog, same split as `importAudioFile()`/
     *        `importImageFile()`.
     *
     * A thin wrapper around `sound_mind::studio::importMidiChannelsInto()`
     * (see its own docs for the actual parsing/`SequenceOperation`
     * construction) that then bakes each new layer's own content in via
     * `toolPaletteController_->rebuildLayerContent()` - unlike audio/image
     * import, which set a layer's `content()` directly, a MIDI import's
     * new layers start content-less until this rebuild happens, since
     * their content comes from the operation log, not a direct assignment.
     * See importAudioFile()'s docs for why this never shows a message box
     * itself.
     *
     * Marks hasUnsavedChanges() on success, same as importAudioFile().
     *
     * @param path Path to the `.mid`/`.midi` file to import.
     * @param separateLayerPerChannel See `importMidiChannelsInto()`'s own
     *        docs.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @param channelNumbers See `importMidiChannelsInto()`'s own docs -
     *        empty (the default) means every channel, unchanged from this
     *        method's own original behavior.
     * @return `true` on success; `false` if no project is open, or the
     *         file couldn't be parsed/had no notes on any (selected)
     *         channel.
     */
    bool importMidiFile(const std::filesystem::path& path, bool separateLayerPerChannel = true,
                         QString* errorMessage = nullptr, const std::vector<int>& channelNumbers = {});

    /**
     * @brief Parses a MIDI file and computes its own channels/snippets,
     *        without importing anything or showing any dialog - a thin,
     *        `!project_`-checking member wrapper around
     *        `sound_mind::studio::midiImportPreviewForFile()`, the same
     *        shape `audioSnippetsForFile()` already establishes for audio.
     * @param path Path to the `.mid`/`.midi` file to analyze.
     * @param errorMessage If non-null and this returns `std::nullopt`, set
     *        to a human-readable description of what went wrong.
     * @return The parsed channels/computed snippets; `std::nullopt` if no
     *         project is open, the file couldn't be parsed, or the
     *         project's own duration is zero.
     */
    [[nodiscard]] std::optional<sound_mind::studio::MidiImportPreview> midiImportPreviewForFile(
        const std::filesystem::path& path, QString* errorMessage = nullptr) const;

    /**
     * @brief Imports specific channels/snippets (see
     *        midiImportPreviewForFile()) of a MIDI file as new layers,
     *        without prompting or showing an error dialog on failure - the
     *        testable core behind `importMidi()`'s own `MidiImportDialog`
     *        flow, the same split `importAudioSnippets()` establishes for
     *        audio's own picker.
     *
     * A thin wrapper around `sound_mind::studio::importMidiSelectionInto()`
     * that then bakes each new layer's own content in via
     * `toolPaletteController_->rebuildLayerContent()`, the same tail
     * importMidiFile() already has.
     *
     * @param path Path to the `.mid`/`.midi` file to import.
     * @param channelNumbers See `importMidiSelectionInto()`'s own docs.
     * @param snippetIndices See `importMidiSelectionInto()`'s own docs.
     * @param separateLayerPerChannel See `importMidiSelectionInto()`'s own
     *        docs.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if no project is open, the file
     *         couldn't be parsed, or nothing was actually imported.
     */
    bool importMidiSelection(const std::filesystem::path& path, const std::vector<int>& channelNumbers,
                              const std::vector<std::size_t>& snippetIndices, bool separateLayerPerChannel,
                              QString* errorMessage = nullptr);

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
     * always had; every image extension `importImageFiles()` accepts is
     * collected and imported as one batch with `imageMode`/
     * `importAsSequence`; a recognized MIDI extension (`.mid`/`.midi`) goes
     * to importMidiFile() or importMidiSelection() per `midiChoices`' own
     * entry for that path (`MidiDropChoice::wholeFile` picks which), or -
     * for a path with no entry there - the same "every channel, whole
     * file, separate layers, no picker" default importMidi()'s own trivial
     * case already used before per-file MIDI drop choices existed
     * (real-world testing pass, 2026-09-29 - previously *every* MIDI drop
     * hardcoded this default regardless of whether the file actually had a
     * real channel/snippet choice to make, unlike audio/image drops, which
     * already prompted correctly). dropEvent() is the one that actually
     * decides all of this (via real `AudioSnippetPickerDialog`/
     * `ImageScalePickerDialog`/`MidiImportDialog` prompts, the same ones
     * File → Import Audio/Image/MIDI themselves show - confirmed with the
     * user: a drop should offer the same choices those menu actions do),
     * so this method itself stays non-prompting; `.smproj` goes to
     * openProjectAt(), guarded by confirmDiscardUnsavedChanges() first -
     * openProjectAt() itself already refuses (no dialog) while Loop Mode or
     * Recording is active. Every other extension is silently ignored, not
     * an error - a stray file dropped by accident shouldn't force anything
     * onto the screen.
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
     * @param imagePolarParams See `importImageFiles()`'s own docs -
     *        meaningful only when `imageMode` is `ImageScalePickerDialog::
     *        Mode::Polar`. Defaults to `std::nullopt`, the pre-existing
     *        default every test predating this parameter still gets.
     * @param midiChoices Each recognized-MIDI-extension path among `paths`'
     *        own resolved import choice (see `MidiDropChoice`'s own docs,
     *        real-world testing pass 2026-09-29) - a path with no entry
     *        here imports the whole file, every channel, one layer each
     *        (`importMidiFile()`'s own pre-existing default), the same
     *        result the trivial "nothing to choose" case already produced
     *        before this parameter existed.
     */
    void handleDroppedFiles(
        const std::vector<std::filesystem::path>& paths,
        ImageScalePickerDialog::Mode imageMode = ImageScalePickerDialog::Mode::RescaleToFitProject,
        bool importImagesAsSequence = false,
        const std::map<std::filesystem::path, std::vector<std::size_t>>& audioSnippetSelections = {},
        const std::map<std::filesystem::path, double>& audioSnippetOffsets = {},
        std::optional<PolarImportParams> imagePolarParams = std::nullopt,
        const std::map<std::filesystem::path, MidiDropChoice>& midiChoices = {});

    /**
     * @brief Exports the whole project's audio (the full composite Playback
     *        hears) to `path`, synchronously and without dialogs - the
     *        blocking, headless-testable counterpart of
     *        exportCompositeAudioAsync(), which the interactive exportAudio()
     *        slot uses.
     *
     * @param path Destination path; its extension (`.flac`/`.ogg`/`.mp3`)
     *        selects the compressed format.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if no project is open, nothing
     *         contributes, the extension didn't match a supported format, or
     *         the underlying export failed.
     */
    bool exportCompositeAudioNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /**
     * @brief Exports the whole project as an MP4 video (the full composite
     *        canvas with a playhead synced to the composite audio) at
     *        `path`, synchronously and without dialogs - the blocking,
     *        headless-testable counterpart of exportCompositeVideoAsync().
     *
     * @param path Destination path.
     * @param errorMessage If non-null and this returns `false`, set to a
     *        human-readable description of what went wrong.
     * @return `true` on success; `false` if no project is open, nothing
     *         contributes, or the underlying export failed.
     */
    bool exportCompositeVideoNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /**
     * @brief Starts an asynchronous video export of the whole project - the
     *        real work behind the interactive exportVideo() slot (whose
     *        `VideoExportDialog` chooses the options), split out so tests can
     *        drive it without a dialog.
     *
     * Checks synchronously that some visible, unmuted, non-filter layer has
     * content (otherwise `QMessageBox::critical()`, nothing started). Then a
     * `sound_mind::core::BackgroundTask` composites a *copy* of the project
     * (`sound_mind::core::exportProjectVideo()`), so editing while it runs
     * is safe by construction. A no-op (status bar message only) if
     * isExportRunning() - see exportCompositeAudioAsync() on why audio and
     * video share one slot.
     *
     * Forwards to `ExportController::exportVideoAsync()` (`v0.1.48.2`).
     * Returns once the task has started - see isExportRunning()/
     * cancelExport(). An internal `QTimer` polls for completion: success
     * shows a status bar message naming the destination (and opens the
     * file if asked); cancellation (during compositing or encoding)
     * removes the partial file and says so; a genuine failure shows
     * `QMessageBox::critical()`. While it runs, the status bar shows the
     * percentage done (`v0.1.45.1`).
     *
     * @param path Destination path.
     * @param options Format, size, frame rate, animation, playhead, polar.
     * @param openWhenDone Open the file in the system's player when done.
     */
    void exportCompositeVideoAsync(const std::filesystem::path& path,
                                   const sound_mind::codec::VideoExportOptions& options = {},
                                   bool openWhenDone = false);

    /**
     * @brief Where an exported video goes by default: beside the project
     *        file, named after it plus `suffix`, with characters Windows
     *        doesn't allow in file names replaced.
     * @param suffix Appended to the project's name ("_export").
     * @return The path (`.mp4`; the export dialog fixes the extension).
     */
    [[nodiscard]] std::filesystem::path suggestedVideoPath(const QString& suffix) const;

    /**
     * @brief Starts an asynchronous compressed-audio export of the whole
     *        project - the real work behind the interactive exportAudio()
     *        slot, with the same shape as exportCompositeVideoAsync().
     *
     * Forwards to `ExportController::exportAudioAsync()` (`v0.1.48.2`),
     * which shares one job and cancel button between all exports: the
     * status bar has room for one message and one cancel button at a time.
     *
     * @param path Destination path; its extension (`.flac`/`.ogg`/`.mp3`)
     *        selects the compressed format.
     * @param format Which compressed format to write - see
     *        `sound_mind::codec::exportCompressedAudio()`.
     */
    void exportCompositeAudioAsync(const std::filesystem::path& path, sound_mind::codec::CompressedAudioFormat format);

    /**
     * @brief Exports a recorded Macro as a video (`v0.1.44.2`,
     *        `docs/sound-mind-design.md`'s *Macros*): a `MacroRenderer`
     *        rebuilds the performance on a project of its own and
     *        `core::exportMacroVideo()` writes it segment by segment, on
     *        `ExportController` (cancellable, sharing the other exports' progress
     *        and cancel button). The open project is never touched.
     * @param path Destination path.
     * @param id The Macro.
     * @param options Format, size, frame rate, animation, playhead, polar
     *        (`v0.1.45.1`).
     * @param openWhenDone Open the file when done.
     */
    void exportMacroVideoAsync(const std::filesystem::path& path, sound_mind::core::MacroId id,
                               const sound_mind::codec::VideoExportOptions& options = {}, bool openWhenDone = false);

    /**
     * @brief A renderer for a recorded Macro, set up on the UI thread with
     *        copies of everything it needs (start entries, the source
     *        lookup, the limiter setting), so it can run on a background
     *        thread.
     * @param id The Macro.
     * @param errorMessage Receives the reason on failure; may be `nullptr`.
     * @return The renderer (not yet prepared), or `nullptr`.
     */
    std::unique_ptr<sound_mind::core::MacroRenderer> macroRendererFor(sound_mind::core::MacroId id,
                                                                     QString* errorMessage = nullptr);

    /// @brief Whether an exportCompositeVideoAsync()/
    /// exportCompositeAudioAsync()/exportMacroVideoAsync() export - any
    /// kind - is still running. Only one export (of any kind) runs at a
    /// time - see exportCompositeAudioAsync()'s own docs on why they
    /// share one slot.
    /// @return `true` from either async export method (once it actually
    ///         started a background task - not for its own synchronous
    ///         "nothing to export" early-out) until the background encode
    ///         finishes, one way or another.
    [[nodiscard]] bool isExportRunning() const noexcept;

    /// @brief Whether any visible, unmuted, non-filter layer has content -
    ///        the synchronous "is there anything to export at all" check
    ///        exportCompositeAudioAsync()/exportCompositeVideoAsync() make
    ///        before starting a background task.
    /// @return `true` if an export would produce something.
    [[nodiscard]] bool hasExportableContent() const;

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
    /// @brief Re-runs updateCanvasFit() when the canvas's visible area is
    ///        resized.
    /// @param watched The object the event is for.
    /// @param event The event.
    /// @return Whatever `QMainWindow::eventFilter()` returns.
    bool eventFilter(QObject* watched, QEvent* event) override;

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
     *        `enabled` is `false`), syncs panAction_/paintAction_/
     *        pickAction_/selectAction_/pathAction_/textAction_'s own
     *        checked state so exactly one of the six ends up checked, and
     *        updates toolButton_'s own label to name whichever one that
     *        is - the shared exclusivity behind setPanModeEnabled()/
     *        setPaintModeEnabled()/setPickModeEnabled()/
     *        setSelectModeEnabled()/setPathModeEnabled()/
     *        setTextModeEnabled(), since `CanvasWidget::ToolMode` only
     *        ever has one active value at a time.
     *
     * When `enabled` is `false` (a tool turning itself off rather than a
     * different one taking over), panAction_ is the one left checked -
     * `ToolMode::None` is Pan's own tool mode, so this is simply
     * `activated`'s own case falling out of the same "exactly one checked"
     * invariant, not a special case.
     *
     * Deliberately hand-managed rather than a `QActionGroup`: a group's
     * own exclusivity would fire *two* `toggled()` calls per click (the
     * newly-checked action's own, and the now-unchecked previous one's)
     * in an order Qt doesn't document as stable, and each of the six
     * handlers above trusting only its own late-arriving call could stomp
     * on another's `canvas_->setToolMode()` result depending on that
     * order - a real bug, caught before it shipped (see
     * `docs/sound-mind-architecture.md`'s Decisions Made). Setting every
     * action's checked state directly and unconditionally (blocked, so
     * this doesn't recurse back into any of the six callers) also keeps
     * the Tool dropdown correctly in sync when one of them is called
     * directly (e.g. by a test), not just via a real menu selection.
     *
     * @param activated Which action to leave checked when `enabled` is
     *        `true` - `panAction_`, `paintAction_`, `pickAction_`,
     *        `selectAction_`, `pathAction_`, or `textAction_`.
     * @param enabled Whether `activated`'s own tool mode should become
     *        active.
     * @param mode The tool mode `activated` corresponds to.
     */
    void setExclusiveToolMode(QAction* activated, bool enabled, CanvasWidget::ToolMode mode);

    /**
     * @brief Sets toolButton_'s own label to name whichever of
     *        panAction_/paintAction_/pickAction_/selectAction_/pathAction_/
     *        textAction_ is currently checked - called only from
     *        setExclusiveToolMode(), the sole place that ever changes which
     *        one of the six is checked.
     */
    void updateToolButtonLabel();

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
     *   explanatory tooltip (still shown despite being disabled - the Tool
     *   dropdown's own menu has `setToolTipsVisible(true)`), whenever the
     *   configured tool is a Mind Grain not usable on the *active* layer
     *   specifically, **or** (`v0.Y.62.1` Installment C, a real-world
     *   testing pass finding) the active layer is a Filter/Equalizer type
     *   at all, regardless of which tool is configured - neither can ever
     *   hold paintable content (`PaintController::beginStroke()`'s own
     *   guard already refuses silently; this is what actually tells the
     *   user *why*, rather than leaving the Tool Configuration panel
     *   showing controls for a layer that's quietly unpaintable). Force-
     *   deactivates Paint mode first (`setPaintModeEnabled(false)`) if it
     *   was currently checked, falling back to Pan via
     *   setExclusiveToolMode()'s own fallback, so a user can never be left
     *   with Paint mode still active on a now-disabled entry. Re-enabled,
     *   tooltip cleared, otherwise. The Filter/Equalizer-type check takes
     *   priority in the tooltip's own wording when both conditions
     *   happen to hold at once (it's the more fundamental of the two -
     *   true regardless of which tool is configured).
     *
     * A no-op-safe default (nothing disabled/marked for the Mind-Grain-
     * ordering half) whenever no project is open, no layer is active yet,
     * or the configured tool isn't a Mind Grain at all - the Filter/
     * Equalizer-type check runs unconditionally whenever a layer is
     * active, independent of which tool is configured.
     *
     * **Worth revisiting**: this method's own name now undersells its
     * scope (two independent guardrails, not just Mind Grain's), kept as-
     * is for this installment to avoid a wider rename across every call
     * site - flagged here rather than silently living with a misleading
     * name forever.
     */
    void updateMindGrainGuardrails();
    /**
     * @brief Enables/disables Configure Devices' own input and output
     *        device pickers (both `DeviceConfigurationWidget` instances,
     *        as of `v0.Y.62.1` Installment H) to match whether an engine
     *        currently has that device actually open - preserves the
     *        "locked while running" safety behavior `RecordPanel`'s/
     *        `LoopPanel`'s own now-removed device pickers used to provide
     *        locally (real-world testing pass, 2026-09-20, finding #7).
     *
     * Input is locked while either `recordEngine_` or LoopController's engine is
     * active - both share the panel's own one input device/gain (see
     * `ConfigureDevicesPanel`'s own class docs). Output is locked only
     * while LoopController's engine is running - ordinary Playback was never a
     * reason to lock the output picker even before this milestone (see
     * `setOutputDeviceSelectionEnabled()`'s own docs on preserving that
     * exact distinction).
     *
     * Called after every place this app starts or stops Recording/Loop
     * Mode, so the picker's own enabled state never goes stale.
     */
    void updateConfiguredDeviceLockState();

    /**
     * @brief Re-resolves the Equalizer layer's own current gradient and
     *        pushes it (or `std::nullopt`) onto `canvas_->
     *        setEqualizerPreview()`, matching `equalizerPreviewEnabled_`'s
     *        own current state - see setEqualizerPreviewEnabled()'s own
     *        docs.
     *
     * A no-op-safe `std::nullopt` push whenever `equalizerPreviewEnabled_`
     * is `false`, no project is open, or (defensively; shouldn't happen -
     * `sound_mind::core::Project::createNew()` always adds one) the
     * project has no `LayerType::Equalizer` layer. Called from
     * setEqualizerPreviewEnabled() directly, from applyFilterConfiguration()
     * (so a live edit to the Equalizer's own curve - via either gradient
     * widget - updates the preview immediately), and from setProject()
     * (so a fresh project's own Equalizer gradient replaces whatever the
     * previous project's preview was showing).
     */
    void updateEqualizerPreview();

    /// @brief Pushes `history_`'s own current descriptions/position into
    ///        `historyPanel_` - `v0.Y.46.1` Installment D ("History
    ///        Panel"). Called wherever `history_` might just have
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

    /// @brief testInputLevelTimer_'s slot: drains deviceTestRecordEngine_'s
    /// own ring buffer (so it never overflows, same reasoning as
    /// Record's drain - see RecordingController) and pushes its currentInputLevel() into
    /// configureDevicesPanel_'s own level meter, while testing an input
    /// device. `v0.0.42.1`.
    void pollTestInputLevel();

    /// @name Menus and toolbars (`v0.1.48.9`, main_window_menus.cpp)
    /// Called once each, in this order, from the constructor, after the
    /// panels and controllers they connect to exist.
    /// @{

    /// @brief The four zoom actions the View menu makes and the View
    ///        toolbar reuses.
    struct ZoomActions {
        QAction* zoomIn = nullptr;      ///< View → Zoom → Zoom In.
        QAction* zoomOut = nullptr;     ///< View → Zoom → Zoom Out.
        QAction* fit = nullptr;         ///< View → Zoom → Fit to Window.
        QAction* actualSize = nullptr;  ///< View → Zoom → Actual Size.
    };

    /// @brief The menu bar's top-right Input/Output and Macro dropdowns.
    void buildCornerMenus();
    /// @brief The File menu.
    void buildFileMenu();
    /// @brief The Edit menu and its submenus.
    void buildEditMenu();
    /// @brief The View menu.
    /// @return Its zoom actions, for buildToolBars().
    ZoomActions buildViewMenu();
    /// @brief The Resource Browser toggle, the Help menu and the Landing
    ///        Page's documentation links.
    void buildHelpMenu();
    /// @brief The Transport toolbar (Tool and Configure dropdowns, panel
    ///        toggles) and the View toolbar.
    /// @param zoomActions From buildViewMenu().
    void buildToolBars(const ZoomActions& zoomActions);
    /// @}

    /// @brief The current project's Loop engine - LoopController's.
    /// @return `nullptr` before the first project, or while constructing.
    [[nodiscard]] sound_mind::core::LoopEngine* loopEngine() const noexcept;


    /**
     * @brief While Repeat is on and playback is running, re-prepares the
     *        audio on a background thread so the next loop restart is
     *        fresh.
     *
     * Connected to PlaybackController::invalidated(), so it fires after
     * every edit (and every other change that makes prepared audio stale).
     * Playback is never interrupted: the result is handed to
     * PlaybackController::stageAudio() and the engine swaps it in at the
     * loop-back sample. A change arriving mid-composite coalesces into one
     * follow-up run, and the stale result is discarded. A no-op unless
     * Repeat is on and playback is running. UI thread only.
     */
    void scheduleRepeatRefresh();

    /// @brief Waits for a pending commitRecordingAsync() save and applies
    ///        it - before New/Open/Close, so the take is part of the project
    ///        they ask about. A no-op when none is pending.
    void flushRecordingSave();

    /// @brief Starts a background render of the filter preview, or marks
    ///        one pending if a render is already running - see
    ///        setFilterPreviewEnabled(). A no-op while the preview is off.
    void scheduleFilterPreviewRender();

    /// @brief Finishes a filter preview render: starts the pending one if
    ///        settings changed meanwhile, otherwise hands the render to the
    ///        preview engine (staged, or loaded on the first render).
    /// @param result The rendered audio, or nothing if the run failed.
    void finishFilterPreviewRender(std::optional<sound_mind::codec::AudioBuffer> result);

    /**
     * @brief Repeat Playback's/one-shot preview's shared edit hook -
     *        connected (through applyProjectChange()'s caller) to
     *        `toolPaletteController_::contentChanged()`, which merges every
     *        content-changing action (Paint/Pick/Fill/Paste/a stamped
     *        sequence, undo, redo). Forwards to
     *        `PlaybackPreparer::contentChanged()` (`v0.1.48.4`): Track with
     *        Repeat refreshes in the background; Delta/Review play the edit
     *        (a one-shot preview with Repeat off). A no-op with no project,
     *        or while handleMindWaveEdited() replays strokes.
     *
     * @param layer Which layer changed.
     */
    void handleContentChangedForPlayback(sound_mind::core::LayerId layer);

    /**
     * @brief Brings everything that shows or plays the project up to date
     *        after an edit (`v0.1.48.1`) - the single place edit sites hand
     *        their change to, instead of each calling its own selection of
     *        reactions. Marks the project unsaved; then, per `change`:
     *        invalidates the canvas composite (from a layer, or all),
     *        prepared playback (which marks the analysis stale too) or just
     *        the analysis, refreshes the Layers panel (all rows, or one
     *        thumbnail), the History and Composer panels, and the Record
     *        panel's targets. UI thread only.
     * @param change What the edit changed - see ProjectChange.
     */
    void applyProjectChange(const ProjectChange& change);

    /**
     * @brief Re-renders what MindWave `id` affects, and only that
     *        (`v0.1.47.4`; it used to invalidate every layer's composite and
     *        paint cache): replays the strokes bound to it, refolds the
     *        composite from the lowest dependent layer up, and marks
     *        prepared playback stale - nothing at all if nothing uses it.
     *        The replays don't trigger Delta/Review playback, as MindWave
     *        edits never have.
     * @param id The MindWave that changed.
     */
    void handleMindWaveEdited(sound_mind::core::MindWaveId id);

    /**
     * @brief Re-reads `gridPanel_`'s own current Snap to Grid checkbox and
     *        both grid configurations, and re-applies them to
     *        `toolPaletteController_` - the single shared body every
     *        `GridPanel` config-change signal and setSoundFlowerModeEnabled()
     *        need, since Snap to Grid's own *effective* state depends on
     *        both the panel's own checkbox and whether Sound Flower is
     *        currently active (suspended while it is - see
     *        setSoundFlowerModeEnabled()'s own docs).
     */
    void applyGridSnapping();


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
    /// each project's LoopController's engine with the same mode `playbackController_`/
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
    ProjectHistory history_;

    /// @brief The in-progress Branching Curve editor's own state, if any -
    ///        see `BranchCurveSession`'s own docs and
    ///        `startBranchingCurveFromPickedPath()`'s for the workflow.
    ///        Owned by value, the same reasoning `history_` above
    ///        already gives - a plain value type, nothing else needs to
    ///        own it. Not reset in setProject() - unlike `history_`'s
    ///        own history, an in-progress branching curve has nothing
    ///        project-specific about its own gathered `Path`s to go stale,
    ///        though switching projects mid-session is an unusual enough
    ///        thing to do that it isn't specifically tested for either.
    BranchCurveSession branchCurveSession_;

    /// @brief Plays recorded Macros back - see `MacroPlayer`, playMacro().
    MacroPlayer* macroPlayer_ = nullptr;

    /// @brief See setEqualizerPreviewEnabled()'s own docs - a sticky,
    /// session-only preference, not reset by setProject() (matching
    /// layersPanel_'s own visibility-persists-across-projects precedent).
    bool equalizerPreviewEnabled_ = false;

    /// @brief Owns the four Paint/Pick/Select/Path tool controllers and
    /// all of their wiring to `canvas_`/`toolConfigurationPanel_` - see its
    /// own class docs. Extracted out of this class as part of the
    /// Refactor & Clean Up milestone (`v0.Y.29.1`, Installment C); "which
    /// layer" a freehand gesture targets (`paintTargetLayerId()`) and the
    /// six Tool-dropdown `QAction`s' own mutual exclusivity
    /// (`setExclusiveToolMode()`) both stay this class's own job - see the
    /// class docs' own `v0.Y.29.1` note.
    ToolPaletteController* toolPaletteController_ = nullptr;

    /// @brief The Tool dropdown's own button (`toolButton_`'s menu) - a
    /// `QToolButton` in `QToolButton::InstantPopup` mode rather than a
    /// `QComboBox`, added in `v0.Y.58.1`'s "Reduce top-level buttons" UI
    /// polish pass to collapse what used to be five separate toolbar
    /// toggles (Paint/Pick/Select/Path/Text) plus the new Pan entry into
    /// one control. Chosen over `QComboBox` so each entry keeps its own
    /// `QAction` - preserving per-entry enable/disable + tooltip (see
    /// updateMindGrainGuardrails()'s own docs) without `QStandardItemModel`
    /// item-flag plumbing. Its label is kept in sync with whichever entry
    /// is checked by updateToolButtonLabel(), called from
    /// setExclusiveToolMode().
    QToolButton* toolButton_ = nullptr;

    /// @brief The Tool dropdown's Pan entry - checked while the canvas is
    /// in its neutral, non-interactive state (`CanvasWidget::ToolMode::
    /// None`). Added in `v0.Y.58.1`'s "Reduce top-level buttons" UI polish
    /// pass as the dropdown's own explicit default, so navigating the
    /// canvas without inadvertently painting/picking/placing nodes has its
    /// own named entry rather than being reachable only by turning every
    /// other tool off. Kept as a member for the same setProject()-resets-it
    /// reason as paintAction_ below.
    QAction* panAction_ = nullptr;

    /// @brief The Tool dropdown's Paint entry - checked while the canvas
    /// accepts freehand paint input (`CanvasWidget::ToolMode::Paint`).
    /// Kept as a member (rather than a local in the constructor) so
    /// setProject() can uncheck it when a new/different project is
    /// opened, the same way it resets every other per-project session
    /// state.
    QAction* paintAction_ = nullptr;

    /// @brief The Tool dropdown's Pick entry - checked while the canvas
    /// accepts Pick input (`CanvasWidget::ToolMode::Pick`). Kept mutually
    /// exclusive with panAction_/paintAction_/selectAction_/pathAction_/
    /// textAction_ by setExclusiveToolMode() (see its own docs), not a
    /// `QActionGroup`; kept as a member for the same setProject()-resets-it
    /// reason as paintAction_.
    QAction* pickAction_ = nullptr;

    /// @brief The Tool dropdown's Select entry - checked while the canvas
    /// accepts Select input (`CanvasWidget::ToolMode::Select`). Kept
    /// mutually exclusive with panAction_/paintAction_/pickAction_/
    /// pathAction_/textAction_ by setExclusiveToolMode(); kept as a
    /// member for the same setProject()-resets-it reason as paintAction_.
    QAction* selectAction_ = nullptr;

    /// @brief The Tool dropdown's Path entry - checked while the canvas
    /// accepts Path input (`CanvasWidget::ToolMode::Path`). Kept mutually
    /// exclusive with panAction_/paintAction_/pickAction_/selectAction_/
    /// textAction_ by setExclusiveToolMode(); kept as a member for the
    /// same setProject()-resets-it reason as paintAction_.
    QAction* pathAction_ = nullptr;

    /// @brief The Tool dropdown's "Text" - see setTextModeEnabled().
    QAction* textAction_ = nullptr;

    /// @brief The Analysis panel - see analyzeLoudness().
    AnalysisPanel* analysisPanel_ = nullptr;
    /// @brief See setMasterLimiter()/setLoudnessNormalization().
    bool limiterEnabled_ = true;
    double limiterCeilingDb_ = -1.0;
    bool normalizationEnabled_ = false;
    double normalizationTargetLufs_ = -14.0;
    /// @brief The loaded playback mix's integrated loudness, if measured.
    std::optional<double> loadedMixLoudnessLufs_;
    /// @brief Applies the current normalization settings to the loaded mix's
    ///        measured loudness - see setLoudnessNormalization().
    void applyNormalizationGain();
    /// @brief Records the integrated loudness of the audio just loaded
    ///        (or staged) for playback, and re-applies the normalization
    ///        gain. The measurement itself runs on the background task that
    ///        prepared the audio (PlaybackPreparer's), never on
    ///        the GUI thread.
    /// @param lufs The loudness, LUFS (`kLoudnessFloorLufs` if silent).
    void noteLoadedPlaybackLoudness(double lufs);
    /// @brief Every project change the analysis report could depend on:
    ///        marks it out of date and, with Auto-update on, cancels a
    ///        running analysis and restarts the debounce - see
    ///        AnalysisController::noteProjectChanged().
    void noteProjectChangedForAnalysis();
    /// @brief Text panel edits to the Picked text, applied after a short
    ///        pause (`v0.1.40.13`): typing a word is one undo step, not one
    ///        per letter. See pickedTextEditTimer_.
    std::optional<TextSettings> pendingPickedTextSettings_;
    /// @brief Fires kPickedTextEditDelayMs after the last Text panel edit
    ///        to the Picked text, applying pendingPickedTextSettings_.
    QTimer* pickedTextEditTimer_ = nullptr;
    static constexpr int kPickedTextEditDelayMs = 400;
    /// @brief Applies pendingPickedTextSettings_ to the Picked text now.
    void flushPickedTextEdit();
    /// @brief The Text tool's panel - see `TextToolPanel`.
    TextToolPanel* textToolPanel_ = nullptr;

    /// @brief Each Edit-menu action that only applies sometimes, with the
    ///        condition under which it does - see updateEditActionStates().
    std::vector<std::pair<QAction*, std::function<bool()>>> editActionConditions_;
    /// @brief The Edit menu itself, whose submenus updateEditActionStates()
    ///        grays out when nothing in them applies.
    QMenu* editMenu_ = nullptr;
    /// @brief The Edit menu's Image Mode checkable toggle (`v0.Y.47.1`,
    /// Principal Modes) - checked while the current project's own
    /// `PrincipalMode` is `Image`. Kept as a member since, unlike Hardware
    /// Acceleration's own app-wide toggle, this reflects *per-project*
    /// state and must be re-synced (via a `QSignalBlocker`-guarded
    /// `setChecked()`, the same pattern its own initial-state application
    /// already uses) every time `setProject()` loads a different project.
    QAction* principalModeAction_ = nullptr;
    /// @brief Edit -> Decimal Separator's two exclusive choices - re-synced
    ///        from each project's own setting in setProject(), like
    ///        principalModeAction_.
    QAction* decimalPeriodAction_ = nullptr;
    QAction* decimalCommaAction_ = nullptr;

    /// @brief Re-entrancy guard shared by every Right-dock-area panel's
    /// own `visibilityChanged()`-to-`raise()` connection (Decision #216) -
    /// `true` for the duration of an actual `raise()` call, so a
    /// `visibilityChanged()` it synchronously fires for a *different*
    /// panel (the previously active tab in the same group, losing focus)
    /// can't schedule that other panel's own raise() in turn. Without
    /// this, two tabs could ping-pong "raise me back"/"raise me back"
    /// indefinitely, one zero-delay `QTimer::singleShot()` at a time - a
    /// real, user-reported hang (one CPU core pegged, UI locked),
    /// reproducible only with a real display, never under this project's
    /// own offscreen test platform.
    bool raisingDockPanel_ = false;

    /// @brief Dedicated to `toolConfigurationPanel_`'s own audio preview
    ///        playback - see `previewToolConfiguration()`'s own docs on
    ///        why this is a separate engine instance.
    sound_mind::core::PlaybackEngine toolConfigurationPreviewEngine_{sound_mind::core::AudioDeviceMode::Real};

    /// @brief The transport toolbar's "Record Macro" checkable toggle -
    /// `v0.Y.49.1` (Macro Mode) Installment A. Checked while
    /// a Macro is being recorded (MacroController) - kept as a member so
    /// setMacroRecordingEnabled() and setProject() can both keep it in
    /// sync (a project switch mid-recording stops the recording outright,
    /// the same "unrelated event, start clean" reasoning setProject()'s
    /// own paintAction_/pickAction_ reset already uses).
    QAction* macroRecordAction_ = nullptr;

    /// @brief The transport toolbar's "Play Macro" momentary action -
    /// `v0.Y.49.1` (Macro Mode) Installment B. Not checkable, unlike
    /// macroRecordAction_ - triggers playMacro() once per click, the same
    /// "always present, no-op when inapplicable" shape deleteAction_'s
    /// own docs describe, rather than tracking an enabled/disabled state.
    QAction* macroPlayAction_ = nullptr;

    /// @brief The transport toolbar's "Export Macro Video..." momentary
    /// action - `v0.Y.49.1` (Macro Mode) Installment C. Not checkable,
    /// same reasoning as macroPlayAction_ above.
    QAction* macroExportVideoAction_ = nullptr;

    /// @brief Macro → Manage Macros... - see manageMacros().
    QAction* macroManageAction_ = nullptr;

    /// @brief The dockable panel exposing the current paint tool's own
    /// parameters, including its Tool Preset drop-down - see its own class
    /// docs for what's still deliberately not built (the Wizard button).
    /// Hidden by default, matching Playback/Record/Loop's own "off until
    /// shown" convention (not Layers', which is shown automatically once).
    ToolConfigurationPanel* toolConfigurationPanel_ = nullptr;

    /// @brief The dockable panel mapping General MIDI programs to saved
    /// Tool Presets - `v0.Y.55.1`'s own last installment, see its own
    /// class docs. Hidden by default, the same convention
    /// toolConfigurationPanel_ above already establishes.
    MidiConfigurationPanel* midiConfigurationPanel_ = nullptr;

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

    /// @brief The dockable panel browsing the current project's (or a
    /// second, read-only `.smproj`'s) resource libraries - see its own
    /// class docs (`docs/sound-mind-roadmap.md`'s `v0.Y.57.1` Installment
    /// C). Hidden by default, the same reasoning mindWavesPanel_ above
    /// already gives.
    ResourceBrowserPanel* resourceBrowserPanel_ = nullptr;

    /// @brief Owns resourceBrowserPanel_'s own actual Project reads/
    /// mutations - see its own class docs.
    ResourceBrowserController* resourceBrowserController_ = nullptr;

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

    /// @brief Constructed in the member-initializer list (not a fixed
    /// in-class default) since it needs `audioDeviceMode_`'s value, which
    /// is only known once the constructor's own parameter is available.
    sound_mind::core::RecordEngine recordEngine_;

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
    /// device - every 100ms, since nothing visual depends on finer
    /// granularity than that.
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
    /// freshly (re)constructed LoopController's engine - see its own docs on why that
    /// matters (real-world testing pass, 2026-09-20, finding #10).
    QString configuredInputDeviceName_;
    QString configuredOutputDeviceName_;

    /// @brief The Configure Devices panel's own currently configured input
    /// gain, tracked here for the same reason as
    /// configuredInputDeviceName_/configuredOutputDeviceName_ just above -
    /// `recordEngine_`/`deviceTestRecordEngine_` are persistent members that
    /// never lose a `setInputGain()` call, but LoopController's engine is rebuilt
    /// fresh by setProject() on every new/opened project, so this is what
    /// setProject() re-applies to it. `1.0f` (unity) is the same default
    /// `LoopEngine`/`RecordEngine` themselves start at.
    float configuredInputGain_ = 1.0f;

    /// @brief The most recently reported playback position, in seconds -
    /// tracked here (PlaybackController exposes no positionSeconds()
    /// getter, only the positionChanged() signal) so
    /// PlaybackPreparer knows where to resume from for `PlaybackScope::Track`
    /// (which never jumps on an edit). Also read by
    /// MacroController, through macroPlayhead(), (`v0.Y.49.1`, Macro Mode)
    /// as the timestamp to record against.
    double currentPlaybackPositionSeconds_ = 0.0;

    /// @brief Set while handleMindWaveEdited() replays strokes, so those
    ///        replays don't start Delta/Review playback.
    bool replayingForMindWave_ = false;

    /// @brief The filter preview's own looping engine, created on first use
    ///        with audioDeviceMode_ - see setFilterPreviewEnabled().
    std::unique_ptr<sound_mind::core::PlaybackEngine> filterPreviewEngine_;
    bool filterPreviewActive_ = false;
    /// @brief The looped window, `{start, end}` in seconds - fixed while
    ///        the preview runs, so every render has the same length.
    std::pair<double, double> filterPreviewWindow_{0.0, 0.0};
    int filterPreviewRenders_ = 0;
    /// @brief An edit arrived while filterPreviewJob_ was running.
    bool filterPreviewDirty_ = false;

    /// @name Long operations (`v0.1.47.2`)
    /// One BackgroundJob each - see its docs. Declared last, so they are
    /// destroyed first: a running job is cancelled and waited for before
    /// any member its work or completion uses goes away.
    /// @{
    std::unique_ptr<ExportController> exportController_;  ///< Export audio/video/macro video (`v0.1.48.2`).
    std::unique_ptr<ImportController> importController_;  ///< Audio, image and MIDI import (`v0.1.48.3`).
    std::unique_ptr<PlaybackPreparer> playbackPreparer_;  ///< Play's composite, Repeat, Delta/Review (`v0.1.48.4`).
    std::unique_ptr<RecordingController> recordingController_;  ///< Record's take and its save (`v0.1.48.6`).
    std::unique_ptr<BackgroundJob> filterPreviewJob_;  ///< Filter preview renders.
    std::unique_ptr<AnalysisController> analysisController_;  ///< The Analysis panel's runs (`v0.1.48.5`).
    std::unique_ptr<MacroController> macroController_;  ///< Recording and playing Macros (`v0.1.48.8`).
    std::unique_ptr<LoopController> loopController_;  ///< Loop Mode: its engine, backing audio and takes (`v0.1.48.7`).
    /// @}
};

}  // namespace sound_mind::studio
