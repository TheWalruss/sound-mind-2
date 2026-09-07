# Changelog

All notable changes to Sound Mind Studio are recorded here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/); versioning is the
`vX.Y.Z.W` scheme documented in `docs/sound-mind-roadmap.md` (X stays `0`
until `v1.0.0.0`; Y for a breaking file-format change; Z per feature
milestone; W per binary build).

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
