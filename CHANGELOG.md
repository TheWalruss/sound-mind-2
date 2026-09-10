# Changelog

All notable changes to Sound Mind Studio are recorded here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/); versioning is the
`vX.Y.Z.W` scheme documented in `docs/sound-mind-roadmap.md` (X stays `0`
until `v1.0.0.0`; Y for a breaking file-format change; Z per feature
milestone; W per binary build).

## [0.0.24.8] - 2026-09-10

Three requested follow-ups to `v0.0.24.7`'s bug fixes, before starting
the Pick tool.

### Added

- **`LayersPanel` gained a "+ Add Layer" button**, above the layer list -
  the "add" half of the legacy panel's own add/delete pair, deliberately
  left out until now (see `docs/sound-mind-architecture.md` Decision
  #39) since there was nothing yet to give a new layer content. Emits
  `addLayerRequested()`.
- **`MainWindow::addEmptyLayer()`** - the handler behind it. Adds a new
  `Normal` layer with a real, silent, project-dimensioned placeholder
  (`LoopEngine::emptyImage()`, the same one a fresh Loop Input layer
  gets), and immediately selects it via the new `LayersPanel::
  selectLayer()`, ready to paint into without an extra click.
- **The status bar shows the mouse cursor's position while it's over the
  canvas** - both widget pixels and time/frequency, clearing when the
  cursor leaves. `CanvasWidget` gained `cursorMoved()`/`cursorLeft()`
  signals (independent of `toolMode()` - they fire regardless of Paint
  mode) and now tracks the mouse continuously (`setMouseTracking(true)`).
- **`ToolConfigurationPanel`'s "Intensity" dB spin box is now a "Color"
  swatch button**, opening a color picker - per `docs/sound-mind-
  design.md`'s own "Color - stereo balance" framing, the picked color's
  red channel sets the brush's left-channel intensity, green sets the
  right channel's, so a stroke can now be louder on one channel than the
  other rather than always identical on both. New `ToolConfigurationPanel
  ::setColor()`/`color()`, with `setColor()` as the testable core behind
  the real, dialog-showing button handler.

Regression: full `sound-mind-studio-tests`/`sound-mind-core-tests` suite
passes. Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.24.7] - 2026-09-10

Two real bugs found during manual testing of `v0.0.24.6`'s newly-visible
painting, both fixed.

### Fixed

- **A painted stroke landed in the wrong place on screen, vertically
  mirrored from where it was actually dragged.** `CanvasWidget`'s
  widget-pixel-to-bin conversion treated widget `y=0` as bin 0 (the
  lowest frequency); the rendered spectrogram image itself puts the
  *highest* bin at `y=0` (`color_mapping.cpp`'s `toRgbImage()`, "row 0 =
  highest frequency"). The two conversions disagreeing meant a stroke's
  own stored frequency values were wrong relative to what was on screen,
  even though the bounding-box/path overlays (which round-trip through
  the same, consistently-wrong conversion) still drew back exactly where
  the cursor had been - self-consistent, but not matching the actual
  spectrogram underneath. `widgetPointToTimeFrequency()`/
  `timeFrequencyToWidgetPoint()` now flip the bin axis to match. See
  `docs/sound-mind-architecture.md` Decision #40.
- **Painting always targeted the topmost layer, regardless of which
  layer's row (if any) was selected in the Layers panel.** `LayersPanel`
  had no selection concept at all until now (a placeholder called out
  explicitly in Decision #38) - it gained one: clicking a row's name
  selects it (its own native highlight), queryable via
  `selectedLayerId()`. `MainWindow::paintTargetLayerId()` now prefers
  that selection, still falling back to the topmost layer when nothing
  is selected (a fresh project, or a selection that was cleared),
  unchanged from before.

Regression: full `sound-mind-studio-tests`/`sound-mind-core-tests` suite
passes.

Held from push per Commit & Push Policy.

## [0.0.24.6] - 2026-09-10

Part 6 of the Phase 3 "Basic Painting" milestone - the Tool
Configuration Panel and the "Show bounding boxes"/"Show path geometry"
overlays. **Painting is now genuinely visible** - open the new "Tool
Configuration" dock (toolbar toggle), adjust its controls or just use
its own already-opaque defaults, and a painted stroke actually changes
what's on screen.

### Added

- **`sound_mind::studio::ToolConfigurationPanel`** - a dockable panel
  (off by default) with the Procedural brush's own real parameters: tip
  shape, falloff, brush size, and intensity/opacity controls that set
  both gradient stops uniformly (a full multi-stop gradient editor is a
  separate, later feature). Deliberately omits the Wizard button and
  Tool Preset drop-down the design doc describes - neither exists yet
  (no Wizard built, no project-level saved-preset list) - rather than
  showing dead controls; see the class's own docs. Its own constructed
  defaults are a real, fully opaque brush, not `ToolConfiguration`'s own
  transparent default, so a stroke painted before ever touching a
  control already paints something visible.
- **`CanvasWidget` gained `setShowBoundingBoxes()`/`setShowPathGeometry()`** -
  draws every active `PaintOperation` targeting the displayed layer's
  own bounding box (cyan) and/or Path geometry (magenta), both off by
  default. Independent of the live in-progress preview, which is always
  drawn regardless of these settings.
- `MainWindow` wires the panel's `toolConfigurationChanged()` into
  `PaintController::setToolConfiguration()` and its two checkboxes into
  `CanvasWidget`'s new overlay settings, and applies the panel's own
  default configuration to `PaintController` immediately at startup.

Regression: `sound-mind-core-tests` (unaffected) and the full
`sound-mind-studio-tests` suite (new `ToolConfigurationPanelTest` class,
plus new tests in `CanvasWidgetTest`/`MainWindowTest`) both pass.
Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.24.5] - 2026-09-10

Part 5 of the Phase 3 "Basic Painting" milestone - `CanvasWidget`/
`MainWindow` wiring. **Painting is now actually usable end to end for
the first time**: drag on the canvas with the new Paint toolbar toggle
on to paint a stroke, Ctrl+Z/Ctrl+Y (Edit menu) to undo/redo it. The
default tool configuration is still fully transparent (no visible
effect) until the Tool Configuration Panel (the next installment)
exists to set a real brush - painting a *visible* stroke isn't
reachable from the UI quite yet, but the operation itself, its undo/
redo, and its live preview path all are.

### Added

- **`CanvasWidget` gained a `ToolMode`** (`None`/`Paint`) and real mouse
  handling: a left-button press/drag/release in `Paint` mode converts
  each position (using the current project's own canvas geometry) into
  time/frequency space and emits `paintStrokeStarted()`/
  `paintStrokeContinued()`/`paintStrokeEnded()` - purely presentational,
  same as every dock panel. `setPaintPreviewPath()` draws the live
  in-progress stroke as a real cubic Bézier `QPainterPath` overlay.
- **`MainWindow` wires `CanvasWidget` to the new `PaintController`**: a
  checkable "Paint" toolbar toggle (`setPaintModeEnabled()`), an Edit
  menu with Undo/Redo (`undo()`/`redo()`, `Ctrl+Z`/`Ctrl+Y`), and the
  actual stroke-to-operation/preview/repaint signal wiring. Which layer
  a stroke paints into is, for now, simply the topmost layer in the
  stack (`paintTargetLayerId()`) - a real "selected layer for painting"
  concept doesn't exist yet (see the architecture doc's Decision #38).
  `setProject()` resets paint mode off and cancels any in-progress
  stroke, the same as every other per-project session reset there.
- **`sound_mind::core::binIndexToFrequency()`/`frameIndexToTime()`** -
  the inverse of `frequencyToBinIndex()`/`timeToFrameIndex()`, needed to
  convert a mouse position back into time/frequency space.

Regression: `sound-mind-core-tests` (50609 assertions/185 cases, up
from 50597/181) and the full `sound-mind-studio-tests` suite (15 new
tests: 10 in `CanvasWidgetTest`, 5 in `MainWindowTest`) both pass.
Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.24.4] - 2026-09-10

Part 4 of the Phase 3 "Basic Painting" milestone - `sound_mind::studio::PaintController`,
the session-level orchestration between an in-progress freehand stroke
and Core's paint/undo machinery. Still not wired into `CanvasWidget`/
`MainWindow` - real mouse capture and the Tool Configuration Panel/Pick
are the remaining installments.

### Added

- **`sound_mind::studio::PaintController`** - owns an in-progress
  stroke's raw point buffer and live-fitted preview `Path`
  (`beginStroke()`/`continueStroke()`/`endStroke()`/`cancelStroke()`),
  each touched layer's own session-only pre-paint base content (for
  `undo()`/`redo()` correctness), and the current `ToolConfiguration`
  new strokes paint with. Emits `pathChanged()` (live preview) and
  `contentChanged(LayerId)` for `MainWindow` to connect once real
  wiring exists - the same "purely presentational-adjacent" shape as
  `PlaybackController`. `frequencyToTimeScale()` (the normalization
  `fitPathToPoints()`/`applyPaintOperation()` both need) is derived
  from the current project's own canvas geometry.

Regression: `sound-mind-core-tests` (unaffected) and the full
`sound-mind-studio-tests` suite (15 new `PaintControllerTest` cases)
both pass. Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.24.3] - 2026-09-10

Part 3 of the Phase 3 "Basic Painting" milestone - the procedural
brush's own DSP: turning a `PaintOperation` into real amplitude writes.
Still not paintable from the UI; Studio-side canvas capture is next.

### Added

- **`sound_mind::core::applyPaintOperation()`** - stamps a `PaintOperation`'s
  brush tip repeatedly along its own `Path`, densely enough to blend into
  a continuous stroke. Each stamp blends every pixel within the tip's
  radius toward the `Path`'s own `Gradient`-evaluated target intensity
  (written directly as the new dB value), weighted by that stop's opacity
  *and* the tip's own per-pixel falloff. `Circle`/`Square`/`Diamond` tip
  shapes have real, distinct footprints; every other `BrushTipShape`
  currently falls back to `Circle`'s.
- **`sound_mind::core::rebuildPaintedContent()`** - replays a layer's own
  active operations (typically `OperationLog::activeOperationsTargeting()`'s
  result) onto a copy of its pre-paint base content, so undo()/redo() is
  actually reflected in what's rendered/played, not just in which
  operations are logged as active.
- **`frequencyToBinIndex()`/`timeToFrameIndex()`** - the same Hz->bin
  (log-scale) and seconds->frame (linear) mappings `sound_mind::codec::encode()`
  uses internally, re-derived here so painting targets the exact pixel a
  given frequency/time would actually be stored at.

Regression: `sound-mind-core-tests` (50597 assertions/181 cases, up from
582/167) and the full `sound-mind-studio-tests` suite both pass. Doxygen
docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.24.2] - 2026-09-10

Part 2 of the Phase 3 "Basic Painting" milestone - the first real
`Operation` subtype, and the tool-configuration model painting a stroke
needs. Still not yet paintable from the UI; the procedural brush's own
amplitude-writing logic and every Studio-side piece come next.

### Added

- **`sound_mind::core::ToolConfiguration`/`ToolType`/`BrushTipShape`** - a
  named, savable painting-tool setup (see `docs/sound-mind-design.md`'s
  "Tool Configuration"). Only `Procedural`'s own parameters (tip shape,
  falloff, size, a default `Gradient`) are real fields so far; the other
  eight `ToolType` values exist as forward-declared groundwork for the
  Tool Configuration Panel/Wizard's dynamic UI, not yet functional tools.
- **`sound_mind::core::PaintOperation`** - the first concrete `Operation`
  subtype: a `Path` and a `ToolConfiguration`, both owned snapshots (not
  shared references), plus the `targetLayer` every layer-content
  operation carries. `bounds()` is exactly its own `Path::bounds()`.
- **`Operation::targetLayer()`** - a new virtual, defaulting to
  `std::nullopt` for structural operations, overridden by layer-content
  ones - lets `OperationLog` filter to one layer's own operations
  generically, without knowing about every concrete subtype.
- **`OperationLog` is now real** (previously a permanently-empty stub):
  `reserveId()`/`append()`, `undo()`/`redo()` (a simple, linear high-water
  mark - a fresh append past an undo() discards the abandoned redo tail,
  standard undo-stack behavior, deliberately separate from `supersedes()`
  which never removes anything), and `activeOperationsTargeting(layer)` -
  the exact, in-order, non-superseded sequence a real replay consumes.
  Full JSON round-trip for `PaintOperation` entries (a `"kind"`
  discriminator, ready for future `Operation` subtypes), with a
  backward-compatible read path for every existing project file's own
  empty-array-shaped operation log.

Regression: `sound-mind-core-tests` (582 assertions/167 cases, up from
518/137) and the full `sound-mind-studio-tests` suite both pass (a full
non-Debug-tests build confirms nothing else broke). Doxygen docs target
rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.24.1] - 2026-09-10

Part 1 of the Phase 3 "Basic Painting" milestone (`docs/sound-mind-roadmap.md`'s
`v0.Y.24.1`) - the foundational data model underneath painting, not yet
anything paintable. Confirmed scope with the user beforehand: this
milestone builds the full Tool Configuration Wizard/Panel and Pick
system alongside a plain procedural brush (not deferred to a later
installment), per the design doc's own "Freehand Path Capture"/"Tool
Configuration"/"Pick" sections (also added this session, alongside the
architecture doc's supporting Core Data Model additions).

### Added

- **`Gradient`/`GradientStop`** (`sound_mind::core`) - the shared model
  behind `docs/sound-mind-design.md`'s *Gradients*: an ordered, always-
  ≥2-stop list interpolated linearly between neighbors, `insertStop()`/
  `removeStop()` (endpoints protected), `evaluate(t)`, and a
  `linkChannels` UI-convenience flag. A fresh `Gradient` starts fully
  transparent at both endpoints, matching the design doc's "nothing is
  painted by accident."
- **`Path`/`PathNode`/`TimeFrequencyPoint`** (`sound_mind::core`) - a
  segmented cubic Bézier curve (smooth-vs-corner nodes, each carrying its
  own `Gradient`), plus **`fitPathToPoints()`**: curve-fits a freehand
  stroke's dense raw samples into a sparse, smooth `Path` in real time via
  Ramer-Douglas-Peucker simplification and Catmull-Rom-derived Bézier
  handles - see the design doc's *Freehand Path Capture*. `Path::bounds()`
  returns a `TimeFrequencyRect` (reusing `Operation`'s own type) spanning
  every node's anchor *and* handle points - deliberately coarse/cheap, not
  the tighter exact curve extent, matching the "bounding box" the design
  doc's *Pick*/*Tool Configuration* sections describe.

Regression: `sound-mind-core-tests` (518 assertions/137 cases, up from
413/102 - `sound-mind-studio-tests` unaffected, not yet touched by this
installment) all pass. Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.23.2] - 2026-09-10

Test-only/internal - no user-visible behavior change. `ctest`'s
`sound-mind-studio-tests` entry took 766 seconds on its own (778 seconds
for the whole regression suite); root cause: `MainWindow`'s constructor
unconditionally opened two real system audio devices (`recordEngine_` in,
`playbackController_` out) regardless of what a given test actually
exercised, since neither had any way to be told not to.

### Changed

- `MainWindow` and `PlaybackController` both gained an `AudioDeviceMode`
  constructor parameter (default `Real`, so the real app is unaffected) -
  the same enum `PlaybackEngine`/`RecordEngine`/`LoopEngine` already used
  for exactly this purpose, threaded one level further out. `MainWindow`
  passes it to `recordEngine_`, `playbackController_`, and the
  `loopEngine_` it later constructs in `setProject()`.
- `test_main_window.cpp` gained a `TestMainWindow : public MainWindow`
  helper whose default constructor passes `AudioDeviceMode::None`; all
  ~115 `MainWindow` constructions in the file now use it (mechanical
  rename, confirmed beforehand that no test in the file depends on real
  device enumeration/behavior - the few exercising device-preference
  forwarding only check that a caller-given name round-trips as a stored
  string). `test_playback_controller.cpp`: 10 of its 11 tests now
  construct with `AudioDeviceMode::None` too; the one that genuinely
  tests real-device behavior (`outputDeviceMethodsAreCallableWithoutCrashing`)
  keeps the real-device default.
- Result: the full regression suite (137 `ctest` entries) now takes
  ~64 seconds total, down from ~779 seconds (~12x).

### Fixed

- **A real, pre-existing test regression found while isolating the slow
  suite**: `MainWindowTest::changingARealRowsOpacitySliderDoesNotCrash()`
  assumed a fresh project's only layer (its Background one) has an
  `opacitySlider` - true before `v0.0.21.2`'s Background-controls fix
  (Decisions Made #30), not after. Fixed by importing a real Normal layer
  first and checking its opacity instead.

Regression: `sound-mind-core-tests` (413/102, unaffected) and the full
`sound-mind-studio-tests` suite (all 14 classes) both pass. Doxygen docs
target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy until the accompanying `v0.0.23.1`
commit, per the user's own go-ahead to push both once regression passed.

## [0.0.23.1] - 2026-09-10

Part 2 of the Phase 2.5 "Refactor & Clean Up" milestone
(`docs/sound-mind-roadmap.md`'s `v0.Y.23.1`) - purely internal code
quality work, no user-visible behavior change. Extracts the
Import/Export cluster `MainWindow` had grown (audio snippet
splitting/importing, image importing, layer audio/video export) into
its own module. The roadmap heading still stays unmarked - Loop Mode,
Recording, and project lifecycle remain directly in `MainWindow`,
candidates for further installments of the same milestone.

### Changed

- **New `sound_mind::studio::import_export` free functions**
  (`audioSnippetsForFile()`, `importAudioSnippetsInto()`,
  `importImageFileInto()`, `importImageFilesInto()`,
  `exportLayerAudioNow()`, `exportLayerVideoNow()`), with their own
  `test_import_export.cpp` (13 tests). Free functions rather than a
  stateful controller class (unlike `PlaybackController`) - there's no
  persistent engine/timer to own here, just logic operating on a
  `Project&`/`Layer` passed in each call, the same shape
  `sound-mind-core`'s own `layer_export.h` already uses one level
  down. Deliberately UI-free: no status bar messages, no
  `canvas_->update()`, no `hasUnsavedChanges_` - `MainWindow`'s own
  wrapper methods (unchanged signatures) still own all of that,
  calling these in between.
- Every `MainWindow` import/export method (`audioSnippetsForFile()`,
  `importAudioSnippets()`, `importImageFile()`, `importImageFiles()`,
  `exportTopmostLayerAudioNow()`, `exportTopmostLayerVideoNow()`)
  keeps its exact pre-refactor signature, now a thin delegating body.
  `importImageFiles()` no longer loops over the sibling
  `importImageFile()` member internally - it delegates to
  `importImageFilesInto()`'s own internal loop instead, and now shows
  one consolidated "Imported N file(s)." status message at the end
  rather than one message per file; no test depended on the
  per-file message.

Regression: `sound-mind-core-tests` (413 assertions/102 cases,
unaffected) and the full `sound-mind-studio-tests` suite (all 14 test
classes, one of them new) both pass. Doxygen docs target rebuilds
clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.22.1] - 2026-09-10

Part 1 of the Phase 2.5 "Refactor & Clean Up" milestone
(`docs/sound-mind-roadmap.md`'s `v0.Y.23.1`) - purely internal code
quality work, no user-visible behavior change. `MainWindow` had grown
the largest and most direct responsibility of any class in the
codebase across all eight of Phase 2.5's real feature milestones (per
that roadmap entry's own note); this installment extracts the two
lowest-risk, clearest-value pieces of it, confirmed with the user
before starting. The roadmap heading stays unmarked - Loop Mode,
Recording, Import/Export, and project lifecycle are still directly in
`MainWindow`, candidates for later installments of the same milestone.

### Changed

- **New `PlaybackController` class** (its own public API, its own
  `test_playback_controller.cpp`) now owns the `PlaybackEngine` and its
  position-polling timer, previously three separate `MainWindow`
  members plus logic spread across five of its methods.
  Project/layer-agnostic (plays whatever `load()` is given; "which
  layer to play" stays `MainWindow`'s job) and UI-agnostic (only ever
  emits `durationChanged()`/`positionChanged()` - `MainWindow` still
  does all the panel-to-panel wiring, unchanged). Every `MainWindow`
  playback method (`startPlayback()`, `isPlaying()`, etc.) keeps its
  exact pre-refactor signature, now a thin delegating body.
- **New `sound_mind::studio::import_helpers`/`qt_image_conversion`
  free functions** - the pure, previously-private helpers behind
  import/export (extension/format detection, image scaling, the
  `QImage`/`RgbImage` bridge) relocated to their own files with their
  own `test_import_helpers.cpp`, no logic changes.

### Fixed

- **A real cross-file duplication**: `CanvasWidget` and `MainWindow`
  each had their own, near-identical `RgbImage`-to-`QImage` conversion
  (one copying, one not, both for good reason - see
  `docs/sound-mind-architecture.md`'s Decisions Made #31). Now one
  shared non-copying `toQImageView()`, with `MainWindow` wrapping it in
  a one-line `.copy()` locally to keep its own exact prior behavior.
- **A real semantic gap found while extracting `PlaybackController`**:
  several call sites needed to mark loaded playback audio stale
  *without* stopping whatever was currently playing (the topmost layer
  changing shouldn't cut off audio that was already playing before the
  change). Added `PlaybackController::invalidate()` specifically to
  preserve that - a plain `stop()` would have been a real, unwanted
  behavior change.

Regression: `sound-mind-core-tests` (413 assertions/102 cases,
unaffected) and the full `sound-mind-studio-tests` suite (all 13 test
classes, two of them new) both pass. Doxygen docs target rebuilds
clean, 0 warnings.

No Y bump - purely internal restructuring; no project file format
change.

Held from push per Commit & Push Policy.

## [0.0.21.2] - 2026-09-10

Two real bugs found via the user's own manual testing of `v0.0.21.1`,
both fixed here.

### Fixed

- **The Layers toolbar button didn't actually toggle the panel.** Two
  compounding causes, both real: `setProject()`'s direct
  `layersPanel_->show()` call never updated `toggleViewAction()`'s own
  checked state (now explicitly synced right after), and -  the actual
  reason clicking it did nothing even once checked-state was correct -
  `LayersPanel`'s constructor left `QDockWidget::DockWidgetClosable`
  out of its feature set, which `QDockWidget` requires before
  `toggleViewAction()` will actually hide the dock at all. Playback/
  Record/Loop's own panels never restricted their features, which is
  why only Layers was affected. See
  `docs/sound-mind-architecture.md`'s Decisions Made #28 (corrected -
  its first version wrongly called this an unreproducible headless-
  test artifact; it wasn't).
- **The Background layer no longer shows an opacity slider or
  translation/rescale controls** in the Layers panel - neither concept
  applies to the always-opaque, always-first-in-time floor of the
  stack. UI-presentation only: the underlying model/methods
  (`setLayerOpacity()` etc.) still work against it unchanged. See
  Decisions Made #30.

### Notes

- **No Y bump.** Both fixes are presentational; no project file format
  change.

Regression: `sound-mind-core-tests` (413 assertions/102 cases,
unaffected) and the full `sound-mind-studio-tests` suite (all 12 test
classes) both pass. Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.21.1] - 2026-09-10

A UI polish pass across several already-shipped milestones (Drag & Drop
Import, Transport Panels, Layers Panel, Loop Mode, Playback), requested
directly and confirmed point-by-point before implementing - not itself
a new `docs/sound-mind-roadmap.md` entry.

### Added

- **Drag & Drop Import now offers the same choices File -> Import
  Audio/Image would**: dropping images shows the same
  `ImageScalePickerDialog` (rescale mode, and "Import as sequence" for
  multiple images) File -> Import Image does; dropping a `.wav` with
  more than one computed snippet shows the same
  `AudioSnippetPickerDialog` File -> Import Audio would, per file.
  Cancelling any one of these dialogs cancels the whole drop, including
  unrelated files carried alongside it. See
  `docs/sound-mind-architecture.md`'s Decisions Made #27.
- **A Layers toggle button** in the transport toolbar, next to
  Playback/Record/Loop's own - same `QDockWidget::toggleViewAction()`
  mechanism, defaulting to shown (the other three default to hidden).
  Playback/Record/Loop no longer get force-shown every time a project
  is opened/created; a user's own show/hide choice for any of the four
  now persists across New/Open Project within a session. See Decisions
  Made #28.
- **"Keep Looping" renamed to "Freeze Loop"** in the Loop panel - not
  descriptive of the actual behavior (freezing the currently-playing
  take, not literally "keeping" anything looping that wouldn't already
  be). Label only - `setKeepLooping()`/`keepLooping()`/
  `keepLoopingChanged` and the checkbox's own internal object name are
  unchanged.
- **The project name in the window title**: "Sound Mind Studio
  v`<version>` - `<project name>`" once a project has been created,
  opened, or saved-as; just the plain title before that.
- **A Playback position bar and a live playhead line**: a draggable,
  seekable elapsed/total position bar and time label in the Playback
  panel, plus a moving vertical white line over the canvas during
  playback - matching video export's own playhead line exactly, so
  live playback looks the same as the video it would export to.
  `PlaybackEngine` gained `positionSamples()`/`totalSamples()`/
  `sampleRateHz()`/`seek()`. See Decisions Made #29.

### Notes

- **A real headless-testing-only quirk found and documented, not
  fixed** (there's nothing to fix - it doesn't occur in a real, shown
  application): `QDockWidget::toggleViewAction()`'s checked state syncs
  from real Show/Hide *events*, which Qt never dispatches for a
  `MainWindow` that's never actually `show()`n - exactly this test
  suite's own situation. `layersToggleActionShowsAndHidesTheLayersPanel`
  verifies the toggle action exists, is checkable, and is in the
  toolbar, not that triggering it changes `isHidden()` - the same kind
  of "can't verify headlessly" limitation `dropEvent()`/
  `dragEnterEvent()` already have for real OS drag gestures. See
  Decisions Made #28's own note.
- **No Y bump.** Every change here is either presentational (labels,
  window title, toolbar buttons) or additive (new `PlaybackEngine`
  accessors, new optional `handleDroppedFiles()` parameters defaulted
  to prior behavior) - no project file format change.

Regression: `sound-mind-core-tests` (413 assertions/102 cases, up from
404/97 - new `PlaybackEngine` position/seek tests) and the full
`sound-mind-studio-tests` suite (all 12 test classes) both pass.
Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.20.1] - 2026-09-10

The "Image Sequence Import" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5): importing multiple images at once can now lay them out
end-to-end in time instead of importing each independently. Checked
against the legacy Studio's own equivalent (`import_wizard.py`/
`app.py`) before implementing, with every real divergence point
confirmed with the user.

### Added

- **`MainWindow::importImage()`** now shows a multi-file picker
  (`QFileDialog::getOpenFileNames`) instead of a single-file one.
- **`ImageScalePickerDialog`** gained a `sequentialCheckBox`
  ("Import as sequence"), shown only when more than one file was
  selected (`allowSequential` constructor parameter, default `false` -
  every existing call site is unaffected). Checking it disables the
  five mode radios rather than coexisting with them - a sequence
  import always applies `ScaleVerticalProportional` regardless of
  whichever mode was last selected. New `importAsSequence()` accessor.
- **`MainWindow::importImageFiles()`**: the new multi-file testable
  core `importImage()` delegates to. Not sequential: loops
  `importImageFile()` per path independently. Sequential: sorts paths
  first (deterministic, for numbered frame sequences), forces
  `ScaleVerticalProportional` on every file, and gives each layer a
  cumulative `translationColumns()` - each starting right after the
  previous layer's own (proportional) width ends, wrapping back to `0`
  once the running total reaches the project's `canvasWidth` (matching
  the legacy Studio's own cumulative-offset behavior exactly).

### Notes

- **Two real decisions confirmed with the user, both resolved in favor
  of the legacy Studio's own precedent over this codebase's newer
  idiom**: the checkbox-vs-6th-mode-value design (matching legacy's own
  radio-button layout, despite the roadmap text's "check... 'import as
  sequence'" wording suggesting a checkbox was already the plan), and
  the wrap-at-`canvasWidth` behavior (over just letting `renderLayer()`
  crop anything past it, which would have worked too but wasn't what
  the legacy Studio actually did). See
  `docs/sound-mind-architecture.md`'s Decisions Made #26.
- **A batch failure doesn't block the rest**: `importImageFiles()`
  returns `true` if at least one file imported, matching
  `importAudioSnippets()`'s and `handleDroppedFiles()`'s own precedent.
- **No Y bump.** Depended on `v0.Y.20.1`/`v0.Y.21.1` (both already
  shipped); no project file format change - sequencing is just another
  use of `translationColumns()`'s own already-lenient field.

Regression: `sound-mind-core-tests` (404 assertions/97 cases,
unaffected) and the full `sound-mind-studio-tests` suite (all 12 test
classes) both pass. Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.19.1] - 2026-09-10

The "Layer Time Alignment" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5): two new per-layer horizontal transform controls -
translation (shift earlier/later in time) and rescale (stretch/
compress the layer's own timeline) - for lining up and matching timing
between layers.

### Added

- **`Layer::translationColumns()`/`setTranslationColumns()`**: a raw
  spectrogram-column shift (`std::int64_t`, default `0`). Columns, not
  seconds - no sample-rate/hop-length conversion needed at render time.
- **`Layer::rescaleFactor()`/`setRescaleFactor()`**: a plain ratio
  (`double`, default `1.0`), `> 1.0` stretches/slows the layer's own
  timeline, `< 1.0` compresses/speeds it up - unclamped, matching
  `opacity()`'s own precedent. Both fields are additive and lenient in
  `Layer`'s JSON (de)serialization, same treatment as `visible`.
- **`renderLayer()` now takes a required `canvasWidth`** and always
  applies rescale then translation before padding/cropping (black) to
  exactly that width, rather than rendering at a layer's own native
  content width - see `docs/sound-mind-architecture.md`'s Decisions
  Made #25 for the reasoning (every layer's render now lines up
  column-for-column with the project's time axis, ahead of real
  multi-layer compositing). `exportLayerVideo()` gained the same
  `canvasWidth` parameter, threaded through to its own `renderLayer()`
  call.
- **`LayersPanel`**: two new per-row controls, `translationSpinBox`
  (`QSpinBox`) and `rescaleSpinBox` (`QDoubleSpinBox`), wired live -
  unlike `opacitySlider`'s edits, which currently have no visible
  effect (real multi-layer blending doesn't exist yet), editing either
  of these repaints the canvas immediately, since they directly change
  `renderLayer()`'s pixel output.
- **`MainWindow::setLayerTranslation()`/`setLayerRescale()`**: the
  actual work behind the two new spin boxes, mirroring
  `setLayerOpacity()`'s shape.

### Notes

- **Visual/spectrogram-only, confirmed narrower than the roadmap
  entry's own wording**: neither control affects
  `decodeLayerForExport()` or any real playback path - real
  multi-layer audio mixing still doesn't exist anywhere in the
  codebase (Playback/Loop Mode/Record all still only ever play the
  single topmost layer with content), so "lining up" and "matching
  timing" are demonstrated visually, not by ear.
- **A real, if narrow, breaking signature change**: `renderLayer()`'s
  new `canvasWidth` parameter has no default, so every existing caller
  needed updating (`CanvasWidget`, `exportLayerVideo()`, and both
  functions' own test suites). No behavioral regression for any
  existing layer - every import path already produces content at (or
  very near) the project's own `canvasWidth` - but `exportTopmostLayerVideoNow()`
  now encodes at the full project canvas resolution rather than a
  layer's own (sometimes much smaller) native resolution, a deliberate
  cost increase for correctness and future-readiness.
- **No Y bump.** A new, additive per-layer field pair, deserialized
  leniently - a project file saved before this milestone still loads.

Regression: `sound-mind-core-tests` (404 assertions/97 cases) and the
full `sound-mind-studio-tests` suite (all 12 test classes) both pass.
Doxygen docs target rebuilds clean, 0 warnings.

Held from push per Commit & Push Policy.

## [0.0.18.1] - 2026-09-10

The "Drag & Drop Import" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5): dropping local files onto the main window imports/opens
them by extension, reusing the File menu's existing import/open
methods rather than a separate code path.

### Added

- **`MainWindow::dragEnterEvent()`/`dropEvent()`**: thin `QWidget`
  overrides accepting any drag carrying at least one local file URL.
- **`MainWindow::handleDroppedFiles()`**: the actual per-extension
  routing, split out as a testable core since nothing can simulate a
  real OS-level drag gesture headlessly. `.wav` routes to
  `importAudioFile()` (every snippet, no picker); the image extensions
  route to `importImageFile()` with
  `ImageScalePickerDialog::Mode::RescaleToFitProject`; `.smproj` routes
  to `openProjectAt()`, guarded by `confirmDiscardUnsavedChanges()`
  first. Every other extension is silently ignored.

### Notes

- **One deliberate deviation from the File menu's own failure
  handling, confirmed while implementing**: a recognized file that
  fails to import/open reports it via the status bar (non-modal), not
  a blocking `QMessageBox` - a multi-file drop shouldn't stop and
  demand attention partway through over one bad file, and it keeps
  `handleDroppedFiles()` itself unconditionally headless-testable. See
  `docs/sound-mind-architecture.md`'s Decisions Made #24.
- **No Y bump.** A new entry point onto import/open methods that
  already exist; no project file format change.

Full regression suite: `sound-mind-core-tests` unaffected (no
`sound-mind-core` changes this milestone); `sound-mind-studio-tests`
gains 6 new `MainWindow` tests covering `handleDroppedFiles()`'s
routing, multi-file ordering, unrecognized-extension handling, and the
Loop-Mode-running refusal path.

## [0.0.17.1] - 2026-09-09

The "Image Import Scaling" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5): importing an image now offers a choice of how it's resized
to the project's canvas dimensions, presented before the import
proceeds.

### Added

- **`sound_mind::studio::ImageScalePickerDialog`**: a modal `QDialog`
  with five radio-button options - "Rescale to fit project" (the
  default), "Scale vertically to fit project, keep horizontal
  resolution", "Scale horizontally to fit project, keep vertical
  resolution", "Scale vertically to fit project, rescale horizontal in
  proportion", and "Keep native resolution". `MainWindow::importImage()`
  always shows it - unlike Audio Import Snippets' picker, there's no
  trivial case to skip it for.
- **`MainWindow::importImageFile()`** gains a required `mode` parameter
  (`ImageScalePickerDialog::Mode`) and a new private `scaleImageForImport()`
  helper that resizes the source `QImage` via `QImage::scaled()`
  accordingly, before it's ever converted to the codec's `RgbImage`.

### Notes

- **A real, pre-existing gap surfaced while scoping this milestone**:
  before this pass, no image import ever resized anything at all -
  `fromRgbImage()`'s own docs already say an image's pixel grid becomes
  the bin/frame grid directly, so "Keep Native Resolution" was silently
  every import's *only* actual behavior. This milestone is what first
  makes the other four modes possible, not just a new choice layered on
  top of existing scaling.
- **`ScaleVerticalProportional`'s width is computed by hand**, not via
  `QImage::scaled()`'s own `Qt::KeepAspectRatio` (which fits *within* a
  bounding box rather than hitting an exact height) - `sourceWidth *
  canvasHeight / sourceHeight`, rounded, applied via
  `Qt::IgnoreAspectRatio` - guaranteeing the documented exact-height
  result. See `docs/sound-mind-architecture.md`'s Decisions Made #23.
- **The existing `importImageFile(path, errorMessage)` call site (one
  test) was updated** to pass `Mode::KeepNativeResolution` explicitly,
  preserving its own pre-existing intent (confirming a layer gets added
  at all, not exercising scaling) - matching this codebase's preference
  for explicit test intent over a silently-implied default.
- **No Y bump.** An import-time behavior change only.

Full regression suite: `sound-mind-core-tests` unaffected (no
`sound-mind-core` changes this milestone); `sound-mind-studio-tests`
gains a new `ImageScalePickerDialogTest` class (5 tests) and 5 new
`MainWindow` tests, one per scale mode, each asserting a distinct
(frameCount, binCount) result.

## [0.0.16.1] - 2026-09-09

The "Audio Import Snippets" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5): audio longer than the project's own duration is now split
into project-length snippets on import, with a picker to choose which
ones actually become layers - going beyond the legacy Studio's own
"import everything, no picker" behavior.

### Added

- **`sound_mind::studio::AudioSnippetPickerDialog`**: a modal `QDialog`
  listing every snippet an import would produce (a numbered row, each
  showing its timespan, checkable), plus a "Select All" checkbox - every
  row checked by default, so accepting immediately matches the legacy
  behavior exactly. `MainWindow::importAudio()` shows it only when a file
  actually splits into more than one snippet; the common case (audio no
  longer than the project) still imports directly, no dialog beyond the
  file picker itself.
- **`MainWindow::audioSnippetsForFile()`**: the new testable, no-dialog
  analysis step - computes how a file would split (each segment exactly
  the project's own `canvasWidth * hopLength` loop length, matching Loop
  Mode's own derivation) without importing anything.
- **`MainWindow::importAudioSnippets()`**: imports a caller-given set of
  snippet indices as new layers - the actual work behind both
  `importAudioFile()` (which now requests every snippet) and the picker
  dialog (which requests only the checked subset). An index beyond the
  file's actual snippet count is silently skipped, not an error.

### Changed

- **`MainWindow::importAudioFile()`** is now a thin wrapper around
  `audioSnippetsForFile()` + `importAudioSnippets()`, requesting every
  computed snippet - preserving its exact pre-existing single-layer
  behavior and naming for the common case (audio no longer than the
  project), and staying headless-safe for tests either way.

### Notes

- **A real design constraint, confirmed while implementing**:
  `importAudioFile()`'s own established "never shows a dialog" contract
  (needed for headless testability) meant the snippet-splitting analysis
  and the actual import had to be two separate methods, rather than one
  that both analyzes and conditionally pops up a picker - see
  `docs/sound-mind-architecture.md`'s Decisions Made #22.
- **Snippet layer naming keeps its original split position**: with more
  than one snippet, a layer is named `"<stem>_NNNN"` using its position
  in the *full* split (zero-padded to four digits), not renumbered
  sequentially among just the imported subset - so a layer's name stays
  meaningful even when some snippets were skipped. Requested indices are
  de-duplicated and imported in ascending position order regardless of
  the order they were requested in.
- **No Y bump.** A new import-time behavior only; the project file format
  is unchanged.

Full regression suite: `sound-mind-core-tests` unaffected (no
`sound-mind-core` changes this milestone); `sound-mind-studio-tests`
gains a new `AudioSnippetPickerDialogTest` class (4 tests) and 7 new
`MainWindow` tests for the snippet-splitting/import logic.

## [0.0.15.1] - 2026-09-09

The "Transport Panels" milestone from `docs/sound-mind-roadmap.md` (Phase
2.5): Playback/Record/Loop each get their own dockable panel, adapted
from the legacy Studio's separate docks, finally landing real input/
output device selection and an above-unity Playback volume control.

### Changed

- **Play/Pause/Stop/Record/Loop are no longer direct toolbar actions** -
  confirmed before implementing: matching the legacy Studio's own dock
  panels exactly, all five moved fully into three new dock panels, not
  just the newly-added device pickers/volume/Keep Looping. The transport
  toolbar's three remaining actions for these are pure show/hide toggles
  (`QDockWidget::toggleViewAction()`) for the panels, nothing more.
- **The "Keep Looping" checkbox moved** from the transport toolbar (its
  confirmed stopgap location as of `v0.0.14.1`) into the new Loop panel.

### Added

- **`sound_mind::studio::LoopPanel`/`RecordPanel`/`PlaybackPanel`**: three
  new `QDockWidget`s (each wrapped in a `QScrollArea`, per the confirmed
  scroll-bar requirement), hidden until a project exists like
  `LayersPanel`. `LoopPanel`: Start/Stop, input+output device pickers,
  "Keep Looping". `RecordPanel`: Start/Stop, an input device picker.
  `PlaybackPanel`: Play/Pause/Stop, an output device picker, a volume
  slider (0-200%). Every device picker's first entry is
  "(System Default)", mapped to an empty device name - the same
  empty-string-means-default convention the engines themselves use.
- **`sound_mind::core::availableAudioDeviceNames()`**: a new shared free
  function for the JUCE device-enumeration dance (`AudioIODeviceType::
  scanForDevices()` then `getDeviceNames()`), used by all three engines'
  own `availableInputDeviceNames()`/`availableOutputDeviceNames()` rather
  than tripling the logic.
- **`PlaybackEngine::setPreferredOutputDevice()`/`currentOutputDeviceName()`**:
  switches immediately (its device is open for the engine's whole
  lifetime, unlike the other two engines) - confirmed via a real JUCE
  quirk found while testing (see Notes). **`setVolume()`/`volume()`**: a
  real gain boost past unity, clamped to `[0, kMaxVolume]` (`2.0`, 200%) -
  applied in `renderBlock()`.
- **`LoopEngine`/`RecordEngine::setPreferredInputDevice()`** (`LoopEngine`
  also `setPreferredOutputDevice()`): stores a preference applied on the
  *next* start() - both only ever open a device inside start() to begin
  with, unlike `PlaybackEngine`.
- **`MainWindow::setLoopInputDevice()`/`setLoopOutputDevice()`/
  `setRecordInputDevice()`/`setPlaybackOutputDevice()`/
  `setPlaybackVolume()`**: the panels' actual device/volume-picker
  handlers, plus `loopInputDevice()`/`loopOutputDevice()`/
  `recordInputDevice()`/`playbackVolume()` readback accessors (matching
  the `keepLooping()` accessor's own testability precedent).

### Notes

- **Two scope questions confirmed before implementing**: (1) transport
  controls move fully into the panels, matching legacy exactly, not just
  device pickers; (2) device/volume choices are session-only for this
  pass (reset to system defaults every launch) - persistence via
  `QSettings` is deferred, not part of this milestone.
- **A real JUCE quirk, confirmed while testing**:
  `AudioDeviceManager::setAudioDeviceSetup()` only actually validates a
  device name once the manager's device types have been populated at
  least once (`getAvailableDeviceTypes()`/`scanForDevices()`) - calling it
  as the very first thing ever done on a brand-new engine trivially
  "succeeds" against a name never actually checked. Never an issue in
  practice (a real picker always populates itself first), but the tests
  had to work around it explicitly.
- **No Y bump.** No project file format changes; device/volume state
  isn't persisted anywhere yet (see Notes above).

Full regression suite: `sound-mind-core-tests` 86/86 Catch2 test cases
(365 assertions) passing, including a new `test_audio_device_list.cpp`
and 6/3/3 new cases for `PlaybackEngine`/`RecordEngine`/`LoopEngine`'s
device-selection APIs; `sound-mind-studio-tests` runs 130 QTest functions
across ten classes (up from 103 across seven) - three new classes
(`LoopPanelTest` 7, `RecordPanelTest` 4, `PlaybackPanelTest` 6) and 10 new
for `MainWindow`'s panel wiring, including two real embedded-widget,
end-to-end wiring checks (`loopPanelToggleButtonStartsAndStopsTheRealEngine`,
`playbackPanelButtonsDriveRealPlayback`).

## [0.0.14.1] - 2026-09-09

The "Loop Mode" milestone from `docs/sound-mind-roadmap.md` (Phase 2.5),
renamed and reimplemented from Live Mode as the fixed-length loop pedal
the legacy Studio's own "Live Mode" actually was, confirmed as the right
model before implementing.

### Changed

- **`sound_mind::core::LiveEngine` renamed/reimplemented as `LoopEngine`**
  (`live_engine.h`/`.cpp` → `loop_engine.h`/`.cpp`). Constructed with a
  fixed `loopLengthSamples` (the project's own duration - `canvasWidth *
  hopLength` - not user-adjustable) in addition to a `StreamCodecConfig`.
  Each completed loop is sliced off the capture ring and run through the
  same whole-buffer `sound_mind::codec::encode()`/`decode()` any import or
  Recording already uses - not `StreamIncrementalEncoder` - since a loop
  is a fixed, bounded buffer, not a continuously-growing stream. Playback
  reads sequentially from one of two pre-allocated, fixed-length buffers;
  the worker publishes a newly-decoded loop by flipping which one is
  "active," and the audio thread only picks that up once per loop *it
  itself* plays through (never mid-loop, to avoid an audible splice).
  `loopsCaptured()`/`loopsBehind()` report progress and how many whole
  loops the worker's published result currently lags behind.
- **`sound_mind::studio::MainWindow`**: `toggleLiveMode()` → renamed
  `toggleLoopMode()`, `isLiveModeRunning()` → `isLoopModeRunning()`,
  `liveLayerId_` → `loopLayerId_`, the captured layer's default name
  "Live Input" → "Loop Input". `loopEngine_` is no longer a fixed member
  built once with a default config: it's a `std::unique_ptr`, `nullptr`
  until the first `setProject()` call, then (re)constructed there from
  the *current* project's own `streamCodecConfigFor()` config and loop
  length - resolving the construction-time-config gap `v0.0.12.1`'s notes
  deferred to this milestone.

### Added

- **`LoopEngine::setKeepLooping()`/`keepLooping()`**: while enabled,
  newly captured input is never pushed into the ring at all, so the
  currently-active playback buffer just keeps replaying, untouched,
  instead of being recorded over each cycle - a new capability, not
  present in the legacy version. `MainWindow::setKeepLooping()` forwards
  to it, wired to a new "Keep Looping" checkbox in the transport toolbar.
- **A visible loop-delay indicator**: `MainWindow::updateLoopLayer()` now
  also shows `LoopEngine::loopsBehind()` in the status bar
  (`"Looping... (N loops behind)"` once nonzero) - a non-modal message,
  matching every other progress indicator in this codebase, updated on
  the same 33 ms timer that already refreshes the captured layer's
  content.

### Fixed (found in manual testing, before the first push)

- **Restarting Loop Mode piled up duplicate "Loop Input" layers**, and the
  newest one (always topmost, starting with no content) hid whatever the
  previous session had already captured until its own first loop
  finished - reading as "Loop Mode isn't capturing anything" on a project
  where it was the only real content, especially since that first wait
  can be as long as the whole project duration. `toggleLoopMode()` now
  searches the project for an existing Normal layer named "Loop Input"
  and reuses its id if found, only creating a new one otherwise.
- **A genuinely new "Loop Input" layer showed nothing at all until its
  first whole loop finished capturing** - confirmed as expected-but-
  confusing behavior, not a capture bug: the canvas is correct to have
  nothing real to show yet, but a silent, unchanged canvas for up to a
  whole project-duration's wait reads exactly like "not working." New
  **`LoopEngine::emptyImage()`**: a silent, correctly-dimensioned
  placeholder Stream image (a real `encode()` of a zero-filled buffer at
  the engine's own config/loop length), given to a brand-new "Loop Input"
  layer immediately on start - a *reused* layer (the fix above) keeps its
  real previous content instead, untouched by this.

### Notes

- **A real, structural latency confirmed acceptable rather than
  engineered away**: never splicing a newly-published result in
  mid-loop, combined with a loop's audio only being encodable once its
  own capture finishes, means a captured loop is first heard during
  playback loop `N + 2`, not `N + 1`, even when the worker keeps up
  perfectly (`loopsBehind() == 0`). See `LoopEngine`'s own docs for the
  full derivation - a true zero-extra-latency design would need capture
  and playback to run deliberately out of phase with each other, which
  is future work if this baseline ever turns out to matter in practice,
  not part of this milestone's confirmed scope.
- **No compositing with the rest of the project, still** - each loop
  plays back only the current topmost layer with content, the same
  convention Playback and Record already use. Real multi-layer audio
  mixing remains deferred, as it has been since Live Mode's own original
  milestone.
- **No Y bump.** No project file format changes here at all.

Full regression suite: `sound-mind-core-tests` 73/73 Catch2 test cases
(344 assertions) passing, including 8 new for `LoopEngine` (replacing
`LiveEngine`'s 4); `sound-mind-studio-tests` runs 103 QTest functions
across seven classes (up from 97), including 6 new for `MainWindow`'s
Loop Mode reconstruction/Keep Looping/layer-reuse/placeholder-content
behavior and 7 renamed in place (`toggleLiveMode...` →
`toggleLoopMode...`).

## [0.0.13.1] - 2026-09-09

The "Layers Panel" milestone from `docs/sound-mind-roadmap.md` (Phase
2.5). A dockable panel listing the current project's layer stack -
adapted from the legacy Studio's own Layers panel, scoped down to what
`sound_mind::core::Layer` actually supports today.

### Added

- **`sound_mind::studio::LayersPanel`**: a `QDockWidget`, hidden until a
  project exists. Each row: a drag handle (a lock icon instead for
  `Background`/`Equalizer`, which can't be reordered), a visibility
  toggle (disabled for `Background`, which is always visible), the name
  (double-click to rename), a type tag for non-`Normal` layers, an
  opacity slider, and a delete button (hidden for the two locked types).
- **`sound_mind::core::Layer::visible()`/`setVisible()`**: a new field,
  deserialized leniently (defaults to `true` if absent) so a project file
  saved before this milestone still loads.
  `MainWindow::topmostLayerWithContent()` now skips hidden layers.
- **`Project::removeLayer()`/`reorderLayers()`**: new - per
  `Project::layers()`'s own docs, layer-stack membership/order changes go
  through dedicated methods, not the mutable vector it also exposes.
  `reorderLayers()` validates its input is a genuine permutation of the
  current layers before applying anything (see Notes for a real bug this
  caught in its own tests).
- **`MainWindow::toggleLayerVisibility()`/`setLayerOpacity()`/
  `renameLayer()`+`renameLayerTo()`/`deleteLayer()`/`reorderLayers()`**:
  wired to the panel's signals. `renameLayer()`/`renameLayerTo()` mirror
  the interactive/testable split `openProject()`/`openProjectAt()`
  already established - the only one of these that needs a real dialog
  (`QInputDialog`). A rejected reorder still refreshes the panel, so its
  speculative drag-driven display snaps back to the authoritative order.

### Fixed (found in manual testing, before the first push)

- **Dragging a row didn't reorder anything.** `DragHandleLabel::forward()`
  mapped the forwarded mouse event's position with `viewport->mapFrom(this,
  pos)` - backwards: both `mapTo`/`mapFrom` require their "other widget"
  argument to be an *ancestor of the object the method is called on*, and
  here that's `viewport` being an ancestor of the drag handle, not the
  other way around. The forwarded events landed on the wrong point,
  outside any real item, so `QListWidget` never started a drag at all.
  Fixed by calling `mapTo` on the drag handle itself, matching the
  direction the legacy Studio's own equivalent code used.
- **Hiding a layer toggled its own row icon but never changed the
  canvas.** `MainWindow::topmostLayerWithContent()` (Playback/Pool/
  Export's shared notion of "the composite") got the `visible()` check;
  `CanvasWidget`'s own, entirely separate `findTopmostRender()` - the
  function that actually decides what gets drawn - didn't, since it
  renders straight from a `Project*` rather than going through
  `MainWindow`. Fixed by adding the same check there.
- **Renaming a layer, or dragging its opacity slider, crashed the whole
  app.** Root cause: `LayersPanel::setLayers()` deleted the previous
  rows' widgets *synchronously* - including, in these two cases, the very
  widget (the name label, the slider) still further up the call stack,
  mid-emission of its own signal (`renameLayerTo()`/`setLayerOpacity()`
  both reach `setLayers()` via `refreshLayersPanel()`, called from that
  same row's `doubleClicked()`/`valueChanged()` handler). Visibility's
  toggle and the delete button happened not to crash only because
  `QAbstractButton`'s own click handling is specifically written to
  tolerate the button beneath it disappearing mid-click - `QSlider` and
  this file's own `ClickableNameLabel` aren't. Fixed by using
  `deleteLater()` instead of `delete` for the old rows, deferring their
  actual destruction until nothing is still executing on top of them.

### Notes

- **Scoped down from the opening roadmap paragraph's "add/delete"
  mention**: no "+ Add Layer" button this pass - the detailed row-design
  spec and demo never actually called for one, and Painting doesn't exist
  yet (Phase 3) to make a blank layer meaningful to add.
- **A real bug caught while writing `Project::reorderLayers()`'s own
  tests**: an earlier version detected a *missing* id in a proposed
  reorder but not a *duplicated* one - a duplicate would have silently
  stood in for whatever id it crowded out, corrupting the stack (losing a
  real layer) instead of being rejected. Fixed before it ever shipped.
- **A real `QListWidget` gotcha hit and fixed**: `QListWidget::clear()`
  does not delete widgets set via `setItemWidget()` - without an explicit
  fix, every `LayersPanel::setLayers()` call after the first would leak
  the previous rows' widgets, which then kept showing up alongside the
  new ones.
- **Drag-and-drop reordering itself isn't covered by an automated test** -
  matching this codebase's existing precedent for anything that
  fundamentally needs a real, interactive gesture (modal dialogs, real
  file pickers) - confirmed manually instead (see Fixed, above, for what
  manual testing actually caught here).
- **No Y bump.** `Layer::visible` deserializes leniently - additive, not
  a breaking format change.

Full regression suite: 105/105 ctest entries passing (codec, core,
studio) - `sound-mind-studio-tests` now runs 97 QTest functions across
seven classes (up from 73), including 10 new for `LayersPanel`, 13 more
for `MainWindow`'s layer-mutating slots and the opacity-slider crash
regression test, and one for `CanvasWidget`'s own visibility-skip fix;
nine new Catch2 cases cover `Layer::visible` and
`Project::removeLayer()`/`reorderLayers()` in
`sound-mind-core`.

## [0.0.12.1] - 2026-09-09

The "Create Project Wizard" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5). Replaces the no-dialog `newProject()` (an in-memory default
project, nothing asked) with a real creation dialog, plus real wiring: a
project's settings now actually drive how new content gets encoded,
closing a real gap surfaced while scoping this milestone.

### Added

- **`sound_mind::studio::CreateProjectWizard`**: Name, Save Folder, and
  Duration always visible; sample rate, frequency range, bin count, and
  timestep hidden behind an "Advanced" disclosure, defaulted to match
  `ProjectSettings{}` for anyone who never opens it. Purely
  presentational - `settings()`/`path()` are read by `MainWindow`, which
  does the actual creating/saving. Save Folder is a *folder*, not a full
  file path - the destination filename always comes from Name (`<Save
  Folder>/<Name>.smproj`), so the two can never drift apart the way two
  independently-typed fields could (caught in review before the first
  push - the initial version let Location hold a full, independently-
  typed path). The dialog also keeps its size in sync with whichever
  fields are currently visible via `QLayout::SetFixedSize` on its
  top-level layout, so closing "Advanced" actually shrinks it back down
  instead of leaving it at its expanded size (also caught in review -
  Qt doesn't do this on its own when a layout's child is hidden).
- **`MainWindow::createProjectAt()`**: the non-prompting, testable work
  behind `newProject()` (mirroring `openProject()`/`openProjectAt()`) -
  creates the project, saves it to the given path immediately ("the
  wizard's completion *is* the first save"), and makes it current. A
  failed save doesn't roll the new project back - reported via the
  return value/`errorMessage` instead, matching `importAudioFile()`'s
  "report, don't silently revert" precedent.
- **`sound_mind::core::streamCodecConfigFor()`**: bridges a project's
  `ProjectSettings` to a real `sound_mind::codec::StreamCodecConfig`.
  Wired into `MainWindow::importAudioFile()`/`importImageFile()` and
  Recording's post-capture encode - all three previously used a
  hardcoded default regardless of the open project's settings, a real,
  pre-existing gap this milestone closed rather than left standing.
- **`ProjectSettings` gains `binCount`, `minFrequencyHz`,
  `maxFrequencyHz`** - so the wizard's Advanced fields have somewhere to
  live. Deserialized leniently (falling back to their own defaults if
  absent), so a project file saved before this milestone still loads.

### Notes

- **`LiveEngine`'s own capture/encode config deliberately stays
  unwired** - it's constructed once, before any project exists, with no
  way to be reconfigured afterward; Loop Mode's imminent reimplementation
  (next on the roadmap) is already expected to rework its construction/
  lifecycle, so investing in that here would likely be thrown away there.
  See `docs/sound-mind-architecture.md`'s Decisions Made #18.
- **`poolLayer()`/`decode()` needed no changes** - both already derive
  everything from a layer's existing, self-describing Stream content.
- **No Y bump.** The new `ProjectSettings` fields deserialize leniently -
  additive, not a breaking format change.

Full regression suite: 96/96 ctest entries passing (codec, core, studio) -
`sound-mind-studio-tests` now runs 73 QTest functions across six classes
(up from 61), including 9 new for `CreateProjectWizard` (folder-based
Location, the shrink-back-on-collapse fix included) and 4 more for
`MainWindow::createProjectAt()`/the real codec-settings wiring; four new
Catch2 cases cover `streamCodecConfigFor()` and backward-compatible
deserialization in `sound-mind-core`, alongside extended assertions on
the two existing `ProjectSettings` tests.

## [0.0.11.1] - 2026-09-09

The "Project Lifecycle" milestone from `docs/sound-mind-roadmap.md`
(Phase 2.5). Real unsaved-changes guards on every path that can discard
work - New, Open (both the file-dialog and Recent Projects routes), and
closing the window - none of which existed before this milestone, despite
the roadmap's own wording implying window Close already had one (a
planning-stage assumption that didn't match the actual code - corrected
in the roadmap entry itself, not silently built around).

### Added

- **`MainWindow::hasUnsavedChanges()`**: a plain flag, marked on every
  content-mutating action that exists today (import, Pool, Live Mode
  starting, Recording adding a layer) and cleared by a successful save or
  by `setProject()`. Deliberately not an operation-log diff - no
  `Operation` subtype logs these mutations yet (that's Phase 3's job).
- **`MainWindow::closeEvent()`**: new override - guards exactly like
  `newProject()`/`openProject()` (see below), ignoring the close event if
  the guard says not to proceed.
- **A real `QMessageBox` (Save/Discard/Cancel) guard** on `newProject()`,
  `openProject()`, the Landing Page's Recent Projects click handler, and
  `closeEvent()`, whenever `hasUnsavedChanges()` is true.
  `openProjectAt()` stays deliberately unguarded itself (its established
  "non-prompting, testable" contract) - each interactive caller guards
  before calling it instead.
- **A separate, non-modal refusal for Live Mode/Recording**: `newProject()`,
  `openProject()`, `openProjectAt()`, and `closeEvent()` all refuse
  outright (a status-bar message, no dialog) while either is active,
  rather than risking a silently-discarded in-progress hardware capture -
  extends this codebase's existing "refuse rather than surprise-stop"
  policy for Live Mode/Recording's own mutual exclusion (`docs/sound-mind-
  architecture.md`'s Decisions Made #14) to project switching too. See
  Decisions Made #17 for why this stayed a separate mechanism from the
  unsaved-changes prompt.
- **`setProject()`** now unconditionally stops `liveEngine_`/
  `recordEngine_` (and their UI timers) and clears `liveLayerId_`,
  regardless of caller - a defensive invariant, normally unreachable
  through the guarded entry points above, but keeping the actual *audit
  point* correct is what this milestone asked for.

### Notes

- **No Y bump.** `hasUnsavedChanges_` is in-memory only, never
  serialized - no project file format changed.
- **`CanvasWidget` and `PlaybackEngine` needed no changes.** Audited both
  per `setProject()`'s "audit point" framing: `CanvasWidget` already held
  no cache beyond a raw `Project*` it fully re-renders from on every
  paint; `PlaybackEngine`'s loaded buffer is already unconditionally
  overwritten by the next `startPlayback()` (via `playbackLoaded_`),
  never played stale. The real gaps were `MainWindow`'s own missing
  guards and its unmanaged `liveEngine_`/`recordEngine_` state.

Full regression suite: 92/92 ctest entries passing (codec, core, studio) -
`sound-mind-studio-tests` now runs 61 QTest functions across five classes
(up from 50), including 11 new for unsaved-changes tracking, the
save/switch-clears-it paths, and the Live Mode/Recording refusals.

## [0.0.10.1] - 2026-09-09

The "Visual Identity" milestone from `docs/sound-mind-roadmap.md` - a
lumped UI-polish pass (Phase 2.5), implemented ahead of the Project
Lifecycle/Create Project Wizard/Loop Mode/Layers Panel milestones between
it and Landing Page, per explicit go-ahead. The legacy Studio's icon and
brand palette now carry into this rewrite, across the app itself and its
generated code documentation.

### Added

- **`sound_mind::studio::theme` (`theme.h`)**: `studioStyleSheet()` - the
  app-wide Qt stylesheet (deep orange `#DD4B00` to amber gold `#FEC100`
  gradient accents on a dark ground, matching `docs/stylesheets/extra.css`
  in the legacy repo) applied once, at the `QApplication` level in
  `main.cpp` - and `studioWindowIcon()`, the Studio's window/taskbar icon,
  loaded from the newly-carried-over `ChooseAgainLarge.png`.
- **`sound-mind-studio/assets/`**: `ChooseAgainIcon.ico` and
  `ChooseAgainLarge.png`, copied over from the legacy Studio. The `.ico`
  is used only via the new `resources/app.rc` - the executable's native
  Win32 icon resource (Explorer/taskbar/Alt+Tab), compiled in by the RC
  compiler; the PNG is embedded through Qt's own resource system
  (`assets/app.qrc`) and covers both the runtime window icon and the
  Landing Page's header logo. Splitting the two this way avoids pulling in
  Qt's `qico` imageformat plugin as a runtime dependency for no benefit.
- **Doxygen theme** (`docs/doxygen/sound-mind-theme.css`), wired via
  `HTML_EXTRA_STYLESHEET`: overrides the same brand palette onto modern
  Doxygen's generated CSS custom properties, for both the default/light
  and OS-dark-preference cases, so the code documentation reads as the
  same product rather than a stock Doxygen theme.
- **`LandingPage`'s header** now shows the logo pixmap next to the title,
  matching the legacy `WelcomePanel`'s own header layout now that the
  asset exists.
- **`MainWindow`** sets its own window icon in the constructor (in
  addition to the `QApplication`-level default), so it's real and
  testable independent of `main.cpp`'s own wiring.

### Fixed (during development, not a regression)

- **A real Qt static-library gotcha**: the embedded logo silently resolved
  to a null `QPixmap`/`QIcon` at runtime - `rcc`'s generated resource-
  registration object file, compiled into `sound-mind-studio-lib` (a
  static library), was dropped by the linker because nothing in either
  final executable referenced it directly. Fixed with an explicit
  `Q_INIT_RESOURCE(app)` call in both `main()` entry points (the real app
  and the test binary), per Qt's own documented pattern for this exact
  situation - see `docs/sound-mind-architecture.md`'s Decisions Made #16.

### Notes

- **Deliberately one fixed, dark-first theme - not a light/dark toggle.**
  `docs/sound-mind-design.md`'s Export section already names a future
  Studio-wide light/dark setting; building that toggle is separate,
  not-yet-scheduled work, out of scope for this milestone per
  `docs/sound-mind-roadmap.md`'s own wording.
- **No Y bump.** Purely visual - no project file, public API, or codec
  format is touched.

Full regression suite: 92/92 ctest entries passing (codec, core, studio) -
`sound-mind-studio-tests` now runs 50 QTest functions across five classes
(up from 45), including new coverage for `theme.h` and the logo/window-icon
wiring.

## [0.0.9.1] - 2026-09-08

The "Landing Page" milestone from `docs/sound-mind-roadmap.md` - the first
of Phase 2.5 (UI Foundations). The Studio no longer silently creates an
in-memory project at startup: a persistent start screen is shown until a
project is actually created or opened, informed by (but narrower than) the
legacy Studio's Welcome panel.

### Added

- **`sound_mind::studio::LandingPage`**: the new start screen - a title,
  "New Project"/"Open Project..." actions, and a Recent Projects list.
  Purely presentational (every action is a Qt signal `MainWindow` connects
  to its own existing handlers), narrower than the legacy `WelcomePanel` it
  was informed by: no standalone-file actions (there's no standalone-TIFF
  concept in this rewrite), and no startup-profile selector or favorite-
  directories list - both deferred until there's a real profile/preferences
  concept, or the added surface is worth it.
- **`sound_mind::studio::RecentProjects`**: persists a most-recently-used
  list of project file paths (up to 10, most recent first, missing files
  silently filtered out on read - matching the legacy Studio's own
  behavior) via a caller-supplied `QSettings`. `MainWindow` backs it with a
  real, ini-format settings file under a fresh "SoundMind"/"SoundMindStudio"
  identity, distinct from the legacy Python Studio's own settings (an
  incompatible project file format either way, so nothing would carry over
  meaningfully). Recorded on every successful open or save.
- **`MainWindow::openProjectAt()`**: the non-prompting, directly-testable
  work behind `openProject()` - mirrors `importAudioFile()`'s existing
  split between an interactive slot and a headless-safe worker.
  `MainWindow::isShowingLandingPage()` is a small new testable accessor
  for which of the central `QStackedWidget`'s two pages (Landing Page or
  Canvas) is currently visible.

### Changed

- **`MainWindow`'s central widget is now a `QStackedWidget`** alternating
  between the Landing Page (index 0, shown first) and the `CanvasWidget`
  (index 1) - `setProject()` switches to the canvas the moment a project
  actually exists. The constructor no longer calls `newProject()` itself.

### Notes

- **No Y bump.** Nothing about the project file format changed - this is
  entirely new UI plus one new, additive settings store.
- **Real dependency confirmed, not carried over as-is:** `CanvasWidget`
  already accepted a `nullptr` project gracefully (built that way from its
  own first milestone), so the only real gap this milestone had to close
  was `MainWindow` itself no longer assuming a project always exists -
  every action already guarded on `!project_` defensively, which turned
  out to already be correct, not just forward-compatible scaffolding.

Full regression suite: 92/92 ctest entries passing (codec, core, studio) -
`sound-mind-studio-tests` itself now runs 45 QTest functions across four
classes (up from 29), including new coverage for `LandingPage` and
`RecentProjects` in isolation.

## [0.0.8.1] - 2026-09-08

The "Record" milestone from `docs/sound-mind-roadmap.md` - the last of
Phase 2 (I/O Infrastructure & Performance Baseline): one-shot capture from
an input device into a new layer, encoded exactly as an imported file would
be. Deliberately much simpler than Live Mode (`v0.0.7.1`): no incremental
encoder, no streaming output, no background worker thread.

### Added

- **`sound_mind::core::RecordEngine`**: opens an input-only audio device;
  the real-time callback (`processBlock()`) copies captured samples into a
  `juce::AbstractFifo`-backed lock-free ring buffer (no allocation, no
  locking); `drainAvailable()` (called from a UI-thread timer, no dedicated
  background thread needed - there's no per-block work to do during
  capture, just accumulating raw samples) moves them into a growing
  `AudioBuffer`. `stop()` drains any final samples so nothing captured
  right before stopping is lost. `processBlock()` is independently,
  deterministically testable (mirroring `PlaybackEngine::renderBlock()`/
  `LiveEngine::processBlock()`) - no real device needed to test the
  capture pipeline.
- **"Record" toolbar toggle** in `sound-mind-studio`: starts/stops
  capture; on stop, the captured audio is encoded via the same whole-buffer
  `sound_mind::codec::encode()` any import already uses (per the design
  doc's "encoded ... exactly as any other imported audio would be" - not
  `StreamIncrementalEncoder`'s slightly different framing) and added as a
  new "Recording" layer, mirroring `importAudioFile()`'s own shape. Stops
  Playback first, and refuses to start against a running Live Mode session
  (and vice versa) - see Notes.

### Notes

- **Confirmed scope, mirroring Playback's and Live Mode's own precedent:**
  the design doc's "choose the input device (with rescan)" and "set an
  input gain" are both deferred as UI affordances layered on top of a
  working capture pipeline - Playback deferred an output-device picker the
  same way, and Live Mode an input-device picker.
- **Playback, Live Mode, and Recording are now mutually exclusive** (each
  owns an independent `juce::AudioDeviceManager`, so any two running at
  once risk device contention with no guaranteed cross-platform behavior).
  Starting Live Mode or Recording stops Playback outright; Live Mode and
  Recording instead refuse to start against each other, rather than
  surprise-stopping an in-progress capture. Simultaneous playback-while-
  recording (monitoring a backing track while capturing a take) is
  deliberately out of scope for this first pass.
- No Y bump: `RecordEngine` is a new, additive capability - no project
  file format or existing public API changed. Stayed `v0.0.8.1`, not
  `v0.1.0.1`.
- **Phase 2 complete.** Pool, Export, Live Mode, and Record all now exist;
  Phase 3 (Painting & Editing) is next, per the roadmap.

Full regression suite: 92/92 tests passing (codec, core, studio).

## [0.0.7.1] - 2026-09-08

The "Live Mode" milestone from `docs/sound-mind-roadmap.md`: continuous
real-time capture from an input device, encoded into a growing layer and
streamed back out through the Stream codec - the first genuinely continuous,
real-time-safe audio pipeline in the codebase, and the first real use of a
lock-free ring buffer for the audio-thread handoff
`docs/sound-mind-architecture.md`'s GPU/Audio-Thread Handoff section had
long left as "a plausible direction, not yet a decision."

Confirmed scope, per the two decisions asked before implementing:
- Real multi-layer audio mixing doesn't exist anywhere yet (not even
  Playback does it) - building it for Live Mode alone was declined; this
  first pass's output is the live input alone, round-tripped through the
  Stream codec, not composited with the rest of the project.
- The captured layer's spectrogram *does* visibly grow on the canvas in
  real time, via a UI-thread timer.

### Added

- **`sound_mind::codec::StreamIncrementalEncoder`**: the live-capture
  counterpart to `encode()` - encodes a growing audio stream frame-by-frame
  as enough newly-captured audio makes each frame's analysis window
  available, never zero-padding a frame with fake future audio the way
  `encode()`'s whole-buffer approach does at a clip's real end. Shares its
  per-frame STFT primitives with `encode()` via a new private
  `stream_frame_codec.h` header rather than duplicating that logic.
  Bit-exact with `encode()` on every frame both agree on, and identical
  regardless of how audio is chunked across `pushSamples()` calls
  (verified: matters for real audio callbacks, which never deliver a
  clip's audio in one piece).
- **`sound_mind::core::LiveEngine`**: opens a duplex (input+output) audio
  device and runs the real-time pipeline - the audio callback
  (`processBlock()`) only ever copies fixed-size blocks into/out of two
  `juce::AbstractFifo`-backed lock-free ring buffers (no allocation, no
  locking); a background `juce::Thread` drains the capture ring into
  `StreamIncrementalEncoder`, re-decodes the accumulated image each pass,
  and publishes only the newly-*stable* prefix of the result to the
  playback ring (the last `fftSize - hopLength` samples of any decode are
  still subject to change as more frames arrive, so they're recomputed,
  not published early) - bounding round-trip latency to about one analysis
  window (~40 ms at the default config). `processBlock()`/
  `processPendingAudio()` are both exposed as independently, deterministic-
  ly testable methods (mirroring `PlaybackEngine::renderBlock()`), so the
  whole pipeline is tested without a real audio device or real thread
  timing.
- **"Live" toolbar toggle** in `sound-mind-studio`: starts/stops capture
  into a new "Live Input" layer, mutually exclusive with Playback (both
  would otherwise try to open the default output device through two
  independent JUCE device managers at once) - starting Live Mode stops
  Playback first, and Play is a no-op while Live Mode is running. A 33 ms
  (~30 fps) timer refreshes the captured layer's content from
  `LiveEngine::currentImage()` and repaints the canvas while running.

### Notes

- **Known limitation: decoding is not incremental.** The worker thread
  re-decodes the *entire* accumulated history every pass rather than
  extending a previous decode - correct (each pass's stable prefix is
  identical to what an incremental decoder would produce, since decode()'s
  overlap-add only resolves a sample once every frame overlapping it
  exists), but cost grows with how long a Live session has run. Fine for
  the durations this milestone's demo needs; a real problem for an
  extended session. Revisit with an actual incremental decoder if that
  turns out to matter in practice.
- No Y bump: `StreamIncrementalEncoder` and `LiveEngine` are new, additive
  capabilities - no project file format or existing public API changed.
  Stayed `v0.0.7.1`, not `v0.1.0.1`.

Full regression suite: 86/86 tests passing (codec, core, studio).

## [0.0.6.2] - 2026-09-08

Maintenance pass, ahead of Live Mode: a non-modal status bar reporting
progress/completion for imports, exports, and pooling in
`sound-mind-studio`. A second planned item - porting the legacy codec's
edge-artifact suppression (reflection-padding a clip before its whole-signal
FFT) into the Pool codec - was investigated but not shipped; see Notes.

### Added

- **`sound-mind-studio` status bar**: `importAudio()`/`importImage()`/
  `exportAudio()`/`exportVideo()`/`poolTopmostLayer()` now show an
  in-progress message (e.g. "Importing audio...") before their (still
  synchronous - see Notes) work starts, and a completion message
  afterward, via `QMainWindow::statusBar()`. Per the confirmed scope:
  failures still show a modal (`QMessageBox::critical`) - a missed error is
  worse than an intrusive one - but `poolTopmostLayer()`'s success
  confirmation, previously a `QMessageBox::information` modal, is now a
  status bar message instead.

### Notes

- **Pool codec edge-artifact suppression: investigated, not shipped.**
  The legacy Python codec (`sound_mind_codec/encoder.py`) reflection-pads a
  clip by one period of its lowest analyzed frequency before the transform,
  trimming the padding back out afterward, to give its widest analysis
  window a smooth boundary at the clip's true start/end instead of the
  discontinuity a whole-signal FFT's implicit periodicity otherwise creates
  there. Porting this into `sound-mind-codec`'s Pool codec turned out to
  need more than padding the input: because Pool directly interpolates each
  bin's native-rate sequence onto the output frame grid (rather than
  legacy's "transform the padded signal, then trim frames" pipeline),
  padding also shifts `fftSize` and (per bin) `windowLength`, and every
  attempt at compensating the frame-to-native-index mapping for that shift
  - verified algebraically self-consistent between encode and decode each
  time - still produced far worse round-trip fidelity than doing nothing,
  for reasons not yet root-caused (a genuine, reproducible spurious-energy
  effect in low-frequency bins was observed, but shown not to be the actual
  cause of the regression). Reverted cleanly rather than ship something
  broken or half-verified; the Pool codec is unchanged from `v0.0.5.1`.
  Worth a fresh, dedicated pass rather than folding into other work.
- No Y bump, no file-format change: this is UI plus an abandoned
  investigation, nothing in any on-disk format changed. Stayed `v0.0.6.2`.

## [0.0.6.1] - 2026-09-08

The "Export" milestone from `docs/sound-mind-roadmap.md`: real compressed
audio export (FLAC/Ogg via JUCE, MP3 via ffmpeg) and real MP4 video export
(the spectrogram canvas animated with a playhead, synced to its audio),
plus a File > Export UI and the layer-level "Pool-primary, Stream-fallback"
export policy the design doc calls for.

### Added

- **`sound_mind::codec::exportCompressedAudio()`**: writes FLAC and Ogg
  Vorbis via JUCE's own bundled encoders (`juce_audio_formats`, already a
  linked dependency); writes MP3 via ffmpeg/libmp3lame, since JUCE's own
  `MP3AudioFormat` writer turned out to be an explicit unimplemented stub
  (confirmed by reading its actual source) - JUCE can only *read* MP3, not
  write it.
- **`sound_mind::codec::exportVideo()`**: writes an MP4 (via ffmpeg) of a
  spectrogram canvas animated with a moving vertical playhead line, synced
  to its audio - MPEG-4 Part 2 video (LGPL-compatible, unlike H.264's GPL
  `libx264`) and AAC audio, muxed together. Canvas dimensions are padded to
  the nearest even size for YUV420P's chroma planes if needed.
- **ffmpeg** joins the dependency set (`avcodec`/`avformat`/`swresample`/
  `swscale`/`libmp3lame`, vcpkg `ffmpeg` port), linked dynamically (SHARED)
  - the one deliberate exception to every other dependency here being
    statically linked, since ffmpeg's default LGPL build requires dynamic
    linking to stay compliant with this project's closed-source
    distribution. The only practical vcpkg-available MP4-muxing option.
- **`sound_mind::core::decodeLayerForExport()`/`exportLayerAudio()`/
  `exportLayerVideo()`**: per the design doc's "Pool-mode primary, a quick
  Stream-mode bounce for scratch use" - exporting a layer decodes its Pool
  content (near-lossless) when it's been Pooled, falling back to its
  Stream content (a faster, only approximately phase-accurate bounce)
  otherwise.
- **"Export Audio.../Export Video..." File menu actions** in
  `sound-mind-studio`: export the topmost layer with content, with the
  audio format inferred from the chosen file's extension
  (`.flac`/`.ogg`/`.mp3`).

### Notes

- MP3 export has a real, standard encoder delay (libmp3lame's filter-bank
  priming) before the decoded audio lines back up with the source - an
  inherent MP3 property, not a defect; its round-trip test verifies best-
  alignment correlation rather than assuming zero delay, unlike the
  lossless/JUCE-backed formats.
- A real bug surfaced and was fixed in vcpkg's packaged `JUCEConfig.cmake`
  (its re-entry guard compares against a garbled expected-target list, so
  a second `find_package(JUCE CONFIG REQUIRED)` call in the same CMake
  configure always fails) - resolved by calling it exactly once, at the
  top-level `CMakeLists.txt`, rather than separately in `sound-mind-codec`
  and `sound-mind-core`.
- No Y bump: additive to `sound-mind-codec`'s and `sound-mind-core`'s
  public API, nothing about the project file format changes. Stayed
  `v0.0.6.1`, not `v0.1.0.1`.
- Video export is synchronous and can take a real amount of time for
  longer content (per-frame RGB->YUV conversion plus ffmpeg encoding) -
  no progress indicator or background-thread dispatch yet; noted as
  future work, not in this milestone's scope.

## [0.0.5.1] - 2026-09-08

The "Pool Codec" milestone from `docs/sound-mind-roadmap.md`: a real,
near-lossless NSGT-based Pool codec, a "Pool Layer" action, and the
architecture doc's long-open NSGT library question actually resolved.

### Added

- **`sound_mind::codec::poolEncode()`/`poolDecode()`**: a real
  Non-Stationary Gabor Transform, implemented from scratch on PocketFFT -
  the standard frequency-domain-windowing technique (one global FFT per
  channel; each log-spaced bin's own native-rate coefficient sequence
  comes directly from a Hann-windowed spectral slice via the DFT
  filter-bank identity, no separate demodulation step needed).
  Constant-Q: deep frequency resolution at bass, fine time resolution at
  treble, unlike Stream's fixed-window STFT. Independent left/right
  phase (unlike Stream's shared-phase approximation) - measured
  round-trip correlation: 0.999984, for both mono and genuinely-stereo
  content, on the first working implementation.
- **`sound_mind::codec::writePoolFile()`/`readPoolFile()`**: the Pool
  file format - a standard TIFF 6.0 container (via libtiff), four 16-bit
  grayscale pages (left/right amplitude, left/right phase), LZW-
  compressed, informed by (not bound to) `docs/legacy/SOUND_MIND_TIFF_SPEC.md`;
  a fresh `SoundMindPool:key=value` metadata block, not the legacy key
  set. A-weighting and the tiled storage layout are deferred (see Notes).
- **`toRgbImage(const PoolImage&)`**: renders a Pool image using the same
  red/green/blue convention as the Stream overload (red = left amplitude,
  green = right amplitude, blue = a phase channel - left phase, since
  Pool has no single shared one) - directly enabling pixel-for-pixel
  comparison between a layer's Stream and Pool renders.
- **`sound_mind::core::poolLayer()`**: pools a layer in place - Pool
  content stored on `Layer::poolContent()` (mirroring `content()`,
  persisted the same way under the project's new `pool/` folder), with a
  fresh Stream copy re-derived from the pooled result, per the design
  doc's "resulting Pool file converted to a light-weight Stream copy."
- **"Pool Layer" toolbar action** in `sound-mind-studio`: pools the
  topmost layer with content, then writes both its Stream and Pool
  renders as PNG files (to the system temp directory) for side-by-side
  comparison in any image viewer.
- `Project::layers()` gained a non-const overload, for in-place layer
  mutation (Pooling now, opacity/renaming/painting later).

### Fixed

- **A real, if rare, race condition in `PlaybackEngine`**, found while
  running the full regression suite for this milestone (unrelated to Pool
  Codec itself, but not deferred, per the project's regression-testing
  discipline): when a real audio device exists, the constructor
  unconditionally registered a background callback that also calls
  `renderBlock()` - racing against a test's own direct calls to that
  method on the same shared atomics. Added `AudioDeviceMode::None` so
  tests can construct a fully hermetic engine that never touches real
  hardware; `PlaybackEngine`'s own tests now also run roughly 10x faster
  (no real device-enumeration overhead) as a side benefit.

### Notes

- **Confirmed scope, per the four decisions asked before implementing**:
  NSGT implemented from scratch rather than porting `libnsgt` (its actual
  ~710-line FFTW-coupled source wasn't reliably obtainable through
  available tooling for a faithful port - only a summarized view of it,
  confirmed this session); libtiff over `tinytiff` (permissive license,
  complete native LZW/multi-page support); Pooling as a plain in-place
  mutation rather than building the first `Operation` subtype early; no Y
  bump (the roadmap's own "likely" prediction turned out wrong - the
  `pool/` addition is additive, same as `media/` was in `v0.0.3.1`).
- "Lossless" is a practical claim, not an absolute one, matching the wider
  NSGT literature and the legacy codec's own usage of the term: the
  interpolate-onto-a-common-pixel-grid step every practical NSGT
  implementation needs for rectangular storage is the only source of
  reconstruction error in an otherwise perfectly-invertible transform.
  Energy outside `[minFrequencyHz, maxFrequencyHz]` (DC and Nyquist, in
  particular) is a deliberate, accepted loss, not a bug.
- A-weighting (a cosmetic, decode-reversible dB pre-emphasis the legacy
  TIFF format applies) and the tiled/multi-snippet storage layout (for
  efficient partial decode of long compositions) are both deferred -
  neither affects round-trip correctness, and nothing yet needs partial
  decode (Playback pre-decodes a whole layer at once).
- Test coverage: 8 new `sound-mind-codec` tests (NSGT round-trip fidelity
  both mono and stereo, energy-outside-range handling, Pool file I/O,
  Color Mapping), 5 new `sound-mind-core` tests (`Layer.poolContent()`,
  project persistence, the `poolLayer()` action, plus one covering the
  `PlaybackEngine` fix above), 2 new `sound-mind-studio` GUI tests (the
  Pool Layer action, including its comparison-image output) - 75 tests
  total across the project (22 + 39 + 14), all green.

## [0.0.4.1] - 2026-09-07

The "Playback" milestone from `docs/sound-mind-roadmap.md`: transport
controls, real audio output - press Play, hear the imported audio.

### Added

- **JUCE, linked in for real for the first time**, via the vcpkg `juce`
  port - `juce_core`, `juce_audio_basics`, `juce_events`, and
  `juce_audio_devices` only (never JUCE's GUI modules; Qt still owns the
  Studio's windowing and widgets). Under JUCE 8's free Starter license (up
  to $20,000 gross annual revenue) - see `docs/sound-mind-architecture.md`'s
  Decisions Made for the full reasoning and what's still open beyond that
  threshold. The root CMake project now enables the C language too - JUCE's
  CMake package requires it.
- **`sound_mind::core::PlaybackEngine`**: decodes a layer's content once,
  in full, when told to play, then reads sequentially from that fixed
  buffer via an atomic position index in the real-time audio callback - no
  allocation, no locking, satisfying `CLAUDE.md`'s non-negotiable
  real-time constraint. Its actual per-block rendering logic
  (`renderBlock()`) is exposed separately from the `AudioIODeviceCallback`
  interface specifically so it's unit-testable without a real audio
  device; gracefully falls back to a silent no-op (`isDeviceAvailable()`)
  rather than crashing when no output device exists (CI runners, in
  particular).
- **Transport toolbar**: Play/Pause/Stop, wired to `MainWindow::start
  Playback()`/`pausePlayback()`/`stopPlayback()`, which play the topmost
  layer with content - the same layer `CanvasWidget` already shows, so
  what you hear matches what you see. `startPlayback()` only decodes once
  per "session" (tracked by `playbackLoaded_`), so pausing and resuming
  continues from the same position rather than restarting; stopping, or
  importing new content, invalidates that so the next Play re-picks the
  current topmost layer.

### Notes

- **Simpler than the design doc's eventual scope, on purpose**: decode
  happens once, not continuously - nothing generates a live edit to react
  to yet (Painting doesn't exist until Phase 3). The design doc's actual
  always-current, no-separate-render-step model is Live Mode's job
  (`v0.Y.8.1`), which inherently needs continuous incremental decode
  anyway; Playback will pick that up once it exists. See
  `docs/sound-mind-roadmap.md` for the full reasoning.
- Plays only the topmost layer with content - real multi-layer mixing
  doesn't exist yet, matching `CanvasWidget`'s own current simplification.
- No output device selection or volume control yet - the design doc
  describes both, but the roadmap's demo bar ("press play, hear it")
  doesn't need them, so they're deferred rather than built speculatively.
- Fixed a real, pre-existing gap while cross-checking Doxygen against the
  architecture doc for this milestone: `sound-mind-studio` was never in
  Doxygen's `INPUT` list, so `MainWindow`/`CanvasWidget`'s documentation
  had never actually been verified since it was first written. Added it,
  and fixed the several genuinely undocumented members (mostly
  constructors and simple overrides) that surfaced as a result.
- Test coverage: 6 new `sound-mind-core` tests for `PlaybackEngine`
  (playing state, sample-accurate rendering, end-of-buffer handling, none
  of which need real audio hardware); 4 new `sound-mind-studio` GUI tests
  for the transport wiring - 55 tests total across the project, all green.

## [0.0.3.1] - 2026-09-07

The "Import & Display" milestone from `docs/sound-mind-roadmap.md`: real
audio and image import as new layers, using the Stream codec, with a first
Color Mapping rendering them on the canvas - no more placeholder rectangle.

### Added

- **WAV audio import**: `sound_mind::codec::readWavFile()`, a hand-rolled
  RIFF/WAVE parser (16-bit PCM and 32-bit float, mono or stereo) - no new
  dependency. Deliberately scoped to WAV only for this pass: real JUCE
  integration (for AIFF/FLAC/OGG/MP3/etc) is deferred, partly because
  JUCE's own distribution tier depends on Sound Mind's own license, which
  isn't decided yet (`docs/sound-mind-architecture.md`'s Decisions Needed).
- **Full five-format image import**: PNG, JPEG, BMP, TGA, and WebP, via
  Qt's `QImage` plus the `qtimageformats` add-on module (installed
  alongside the base Qt package - see `README.md`, `ci.yml`, `release.yml`)
  for TGA/WebP specifically; PNG/JPEG/BMP are already built into Qt.
- **Color Mapping, both directions**: `sound_mind::codec::toRgbImage()` /
  `toGrayscaleImage()` render a `StreamImage` as pixels (red = left
  amplitude, green = right amplitude, blue = the shared phase channel;
  grayscale = combined amplitude only); `fromRgbImage()` does the reverse -
  an imported image's RGB pixels become amplitude/phase data directly, no
  transform involved, so an imported image is genuinely unified with
  audio-imported content from the start, per the design doc's "sound and
  image are one continuous surface" principle, not a picture bolted on
  separately.
- **A first, minimal Compositor**: `sound_mind::core::renderLayer()`, in
  `sound-mind-core` - single-layer only (no blend modes, opacity, or
  MindWave-bound parameters applied yet), turning a layer's cached content
  into displayable pixels via Color Mapping. `CanvasWidget` now shows the
  topmost layer with content, scaled to fill the widget, instead of always
  drawing the placeholder rectangle (which remains the fallback for a
  project where nothing has been imported yet).
- **`Layer` gained a content cache**: `Layer::content()` /`setContent()`,
  an in-memory `std::optional<sound_mind::codec::StreamImage>` -
  deliberately *not* part of `Layer`'s JSON serialization. Per the
  architecture doc's Project File & Folder layout, `Project::save()` now
  writes each layer's cached content to its own Stream file under
  `<project>/media/layer_<id>.smstream`, and `Project::load()` reads it
  back - a project's imported audio/images survive a save/reload round
  trip, not just its layer metadata.
- **`Project::addLayer()`**: appends a new layer to the stack, assigning it
  a fresh, unique id - the first time a Sound Mind project has more than
  one layer.
- File menu gained **Import Audio...** and **Import Image...**. Both are
  backed by a plain, headless-safe `MainWindow::import{Audio,Image}File()`
  pair (no dialog, no message box) that the interactive menu actions wrap -
  kept deliberately separate so tests can exercise the real import logic,
  including its failure path, without ever triggering `QMessageBox::exec()`
  under the `offscreen` QPA platform, which blocks on a modal event loop
  nothing can dismiss there.
- `sound-mind-core` depends on `sound-mind-codec` for the first time
  (`Layer` now holds a `codec::StreamImage`), per the architecture doc's
  intended dependency direction.

### Notes

- No DirectX 12 GPU dispatch, no real-time audio path involvement - all of
  this runs on the UI thread, same scope boundary as the Stream codec
  milestone it builds on.
- "The topmost layer with content" stands in for a real composite - true
  multi-layer blending (opacity, blend modes) doesn't exist yet.
- Test coverage: 14 new `sound-mind-codec` tests (WAV parsing, Color
  Mapping both directions), 8 new `sound-mind-core` tests (`Layer` content,
  `Project::addLayer()`, media persistence, the new Compositor), and 4 new
  `sound-mind-studio` GUI tests (audio/image import success and failure
  paths, canvas rendering) - 45 tests total across the project, all green.

## [0.0.2.1] - 2026-09-07

The "Stream Codec" milestone from `docs/sound-mind-roadmap.md`: a real,
working audio codec in `sound-mind-codec` - not yet wired into the Studio's
GUI (that's next milestone's job), validated instead by an automated
round-trip test, per this milestone's own "a round-trip test" demo wording.

### Added

- **The Stream codec itself**: `encode()`/`decode()` in `sound-mind-codec`,
  a Short-Time Fourier Transform (Hann window, 75% overlap) with output
  remapped onto log-spaced frequency bins, matching the visual axis Pool
  will eventually share. `AudioBuffer` (planar stereo PCM) and
  `StreamCodecConfig`/`StreamImage` are the new public types - all
  independent of `sound-mind-core`'s project model, per the architecture
  doc's module-layout constraint (`codec` has no project model and stays
  usable standalone).
- **PocketFFT** as the FFT backend (new third-party dependency, confirmed
  before adding it): header-only C++, BSD-3-Clause, vcpkg-packaged, no
  build/link step. Chosen over `KissFFT` (also vcpkg-packaged, also
  BSD-3-Clause, but needs actual linking and has a narrower feature set)
  and over FFTW3 (GPL-encumbered, per `docs/sound-mind-architecture.md`'s
  NSGT Library Candidates discussion) - see that doc's updated *Decisions
  Made* for the full reasoning.
- **A lightweight custom Stream file format** (`writeStreamFile()` /
  `readStreamFile()`): a small fixed binary header followed by three raw
  `float[bin][frame]` planes - left amplitude, right amplitude, and a
  single *shared* phase channel (from the left/right mono downmix, reused
  for both channels on decode), not TIFF-like and not independent
  left/right phase, since Stream's entire reason to exist is encode/decode
  speed rather than generic-viewer compatibility or Pool-level fidelity.
- Catch2 round-trip tests: a mono-in-stereo tone (where the shared-phase
  approximation is exact, isolating the STFT/log-binning/overlap-add
  pipeline's own correctness) and a genuinely stereo tone (where it isn't),
  both checked via normalized cross-correlation against the original
  signal; a file round-trip test; a bad-magic-bytes error case.

### Notes

- CPU-only - no DirectX 12 GPU dispatch yet. Per
  `docs/sound-mind-architecture.md`'s GPU/Audio-Thread Handoff section,
  that needs a working Stream pipeline to prototype *against*, which now
  exists.
- Not real-time-safe as written (allocates throughout) - real-time-safe
  decode is Playback's concern (`v0.Y.4.1`), not this milestone's.
- Not yet wired into the Studio's GUI, `Project`, or `Layer` - that's
  `v0.Y.3.1` (Import & Display), next.
- The NSGT-specific half of the architecture doc's "NSGT library selection"
  decision (whether `libnsgt`'s logic ports onto PocketFFT) is still open -
  Stream mode uses a plain STFT, not NSGT, so this milestone didn't need to
  resolve it. That's Pool's job, in `v0.Y.6.1`.

## [0.0.1.1] - 2026-09-06

The "Project & Canvas" milestone from `docs/sound-mind-roadmap.md`: a real
project file, and a Studio window that actually shows one.

### Added

- **`sound-mind-core` Project model**: `Project`, `ProjectSettings`, and
  `OperationLog`, alongside the existing `Layer`/`Operation` base classes,
  per the architecture doc's Core Data Model and "Project File & Folder".
  `Project::createNew()` seeds a single Background layer; `Project::load()`
  / `save()` round-trip the whole thing as JSON (`nlohmann::json`, via the
  library's ADL `to_json`/`from_json` customization points) to a `.smproj`
  file. `OperationLog` is genuinely empty for now - it serializes as `[]`
  and accepts (but doesn't yet populate) an array on load, since no
  concrete `Operation` subtype exists yet to fill it with.
- **`sound-mind-studio` canvas and File menu**: `MainWindow` now owns a
  real `Project` (starting from a fresh one via `newProject()`) and a
  `CanvasWidget` central widget that renders the project's canvas
  dimensions as a placeholder solid rect - no codec yet, so there's
  nothing real to paint. File > New/Open/Save/Save As exercise
  `Project::createNew()`/`load()`/`save()` through a `QFileDialog`
  filtered to `*.smproj`, with load/save failures reported via
  `QMessageBox` rather than crashing.
- **QTest-based GUI test suite**: `sound-mind-studio-tests`, covering
  `CanvasWidget`'s size-hint behavior and `MainWindow`'s fresh-project
  lifecycle, run with `QT_QPA_PLATFORM=offscreen` (no real display in CI
  or this dev environment) alongside the existing Catch2 suite in
  `sound-mind-core`. Introduced now rather than deferred, since GUI
  behavior worth testing already exists as of this milestone.

### Fixed

- **AUTOMOC never scanned `sound-mind-studio-lib`'s headers**: CMake's
  AUTOMOC only auto-associates a header with a `.cpp` file when they share
  a basename *and* directory; this project's headers live under
  `include/sound_mind/studio/` while their sources live under `src/`, so
  AUTOMOC silently produced an empty `mocs_compilation.cpp` and linking
  failed with unresolved `moc`-generated symbols. Fixed by listing the
  `Q_OBJECT`-bearing headers explicitly alongside their sources in
  `add_library()`.
- **QTest's default logger produced no output once redirected**: on this
  Windows/Arm64 setup, QTest's plain-text logger wrote nothing at all once
  stdout was piped or redirected - exactly what `ctest --output-on-failure`
  always does - leaving a failing GUI test with no diagnosable detail.
  Fixed by registering the test with `-o -,txt`, which forces the logger
  to stdout explicitly regardless of how it's connected.

### Notes

- `sound-mind-studio` now depends on `sound-mind-core` for the first time.
- The Equalizer special layer from `docs/sound-mind-design.md` isn't
  created yet - it needs a working Filter mechanism, which doesn't exist
  until the `Filter Layers` milestone.

## [0.0.0.2] - 2026-09-06

### Added

- **WiX-based MSI installer**, alongside the existing release zip - not
  replacing it. `cmake --install` now runs `windeployqt` automatically
  (an `install(CODE ...)` hook), so both the zip and the installer are
  produced from the same deployment logic. See `docs/sound-mind-roadmap.md`'s
  "Packaging & Installers" for the two-stage plan this is the first half of.

No feature or file-format change - a point release specifically to validate
the installer on real GitHub Actions infrastructure (both x64 and Arm64),
not just this local machine.

## [0.0.0.1] - 2026-09-05

The first milestone that actually builds, tests, and runs something -
proving the whole toolchain (CMake, vcpkg, MSVC, Qt, Catch2, Doxygen,
CI) fits together, not yet any real Studio functionality.

### Added

- **Toolchain scaffold**: top-level CMake project with vcpkg manifest
  dependencies, `CMakePresets.json` (architecture comes from whichever
  Developer Command Prompt / CMake Tools kit is active - the same presets
  serve native Arm64 dev and native x64/Arm64 CI, no cross-compilation).
- **`sound-mind-codec`**: a toolchain-validation stub (no real codec logic
  yet - see `docs/sound-mind-architecture.md`).
- **`sound-mind-core`**: the `Operation` and `Layer` base classes from the
  architecture doc's Core Data Model - `Operation`'s `bounds()` and
  `supersedes()` in particular, per the "Composer Mode Fit" section.
- **`sound-mind-studio`**: a minimal Qt Widgets application that opens a
  single empty window titled with its own version. Not the real Studio UI
  yet - just proof that Qt links and runs on this stack.
- **Doxygen code documentation**: every public class/function above is
  fully documented; generated via the `docs` CMake target into
  `docs/generated/` (HTML + XML). See `CLAUDE.md`'s "Code Documentation
  (Doxygen)" section for how this is meant to be used - cross-checked
  against `docs/sound-mind-architecture.md` and against the test suite,
  not just generated and left unread.
- **CI**: GitHub Actions builds and tests natively on both Windows x64
  (`windows-latest`) and Windows Arm64 (`windows-11-arm`), per
  `docs/tech-stack-decisions.md`'s Arm64-primary / x64-validated split.

### Notes

- Not yet scaffolded: `sound-mind-gpu`, `sound-mind-vst` (deferred until
  each has real content).
- `sound-mind-studio` does not yet depend on `sound-mind-core` - there is
  no UI logic yet that needs it. Expect that dependency to appear once the
  Studio has anything real to show.
- Qt is distributed via official prebuilt binaries (`aqtinstall`), not
  built from source via vcpkg - vastly faster, and Qt now publishes
  Windows Arm64 binaries directly (LTS since Qt 6.8). Each developer/CI
  job points `QT_ROOT_DIR` (and, for the cross-compiled Arm64 build,
  `QT_HOST_PATH_DIR`) at their own architecture's install, the same
  pattern `VCPKG_ROOT` already uses.
