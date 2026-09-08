# Changelog

All notable changes to Sound Mind Studio are recorded here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/); versioning is the
`vX.Y.Z.W` scheme documented in `docs/sound-mind-roadmap.md` (X stays `0`
until `v1.0.0.0`; Y for a breaking file-format change; Z per feature
milestone; W per binary build).

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
  (`v0.Y.7.1`), which inherently needs continuous incremental decode
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
  resolve it. That's Pool's job, in `v0.Y.5.1`.

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
