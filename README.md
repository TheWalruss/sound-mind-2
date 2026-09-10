# Sound Mind Studio

Sound Mind Studio treats sound as a picture. It encodes audio as a
spectrogram — a time/frequency image — that you can import, layer, and
(eventually) paint on directly, then decodes it back into audio with no
extra step. Horizontal position is time, vertical position is frequency,
brightness is loudness, and color separates the left and right channels.
Import a recording and you're looking at it; import an image and you're
listening to it.

This is a from-scratch C++/Qt/JUCE rewrite of an earlier Python prototype,
built incrementally and tracked openly - see `CHANGELOG.md` for exactly
what's implemented so far and `docs/sound-mind-roadmap.md` for what's
planned next. The full creative vision (painting, filters, MindWaves,
generators, a VST plugin, and more) is recorded in
`docs/sound-mind-design.md`, but most of it is still ahead of where the
Studio actually is today - **the guides below describe only what you can
actually do right now.**

- **New to the Studio?** Start with [`QUICKSTART.md`](QUICKSTART.md) - one
  page, from launching the app to your first saved project.
- **Want the full picture?** [`USER_GUIDE.md`](USER_GUIDE.md) covers every
  screen and control in the current build.
- **Building from source, or curious how it's built?**
  `docs/sound-mind-architecture.md` and `docs/tech-stack-decisions.md`
  cover the technical design; keep reading below for build instructions.

## Building

Prerequisites: a native Arm64 or x64 MSVC toolset (Visual Studio 2022/2026
or Build Tools, with the matching architecture's C++ workload), CMake,
Ninja, [vcpkg](https://github.com/microsoft/vcpkg) bootstrapped somewhere
with `VCPKG_ROOT` set, and Qt 6.8+ installed via
[aqtinstall](https://github.com/miurahr/aqtinstall) (include the
`qtimageformats` module - `--modules qtimageformats` - for full image
import format coverage: TGA and WebP load through it, PNG/JPEG/BMP are
already built into Qt) with `QT_ROOT_DIR` pointing at it (and, only for a
cross-compiled Arm64 Qt build, `QT_HOST_PATH_DIR` pointing at the matching
x64 Qt install alongside it).

From a Developer Command Prompt for your target architecture:

```
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Regenerate the Doxygen code documentation (requires Doxygen on `PATH`):

```
cmake --build build/debug --target docs
```

See `CHANGELOG.md` for what's actually implemented so far.
