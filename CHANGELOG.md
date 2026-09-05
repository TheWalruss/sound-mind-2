# Changelog

All notable changes to Sound Mind Studio are recorded here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/); versioning follows
`docs/tech-stack-decisions.md` and `CLAUDE.md`'s semantic-versioning policy,
with an extra leading `0` component during this pre-alpha bring-up phase
(`0.0.0.x`) before the project starts counting up through `0.x.y` proper.

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
