# Sound Mind Studio

Sound Mind Studio treats sound as a picture. It encodes audio as a
spectrogram — a time/frequency image — that you can import, layer, paint
on, filter and analyze, then decodes it back into audio with no extra
step. Horizontal position is time, vertical position is frequency,
brightness is loudness, and color separates the left and right channels.
Import a recording and you're looking at it; import an image and you're
listening to it.

This is a from-scratch C++/Qt/JUCE rewrite of an earlier Python prototype,
built incrementally and tracked openly - see `CHANGELOG.md` for exactly
what's implemented so far and `docs/sound-mind-roadmap.md` for what's
planned next. The full creative vision is recorded in
`docs/sound-mind-design.md`; parts of it (a VST plugin, among others) are
still ahead of where the Studio actually is today - **the guides below
describe only what you can actually do right now.**

- **New to the Studio?** Start with [`QUICKSTART.md`](QUICKSTART.md) - one
  page, from launching the app to your first saved project.
- **Want the full picture?** [`USER_GUIDE.md`](USER_GUIDE.md) covers every
  screen and control in the current build.
- **Building from source, or curious how it's built?**
  `docs/sound-mind-architecture.md` and `docs/tech-stack-decisions.md`
  cover the technical design; keep reading below for build instructions.

## Licence

Sound Mind Studio is copyright © 2026 Andreas Lundberg. It is **not** open
source: it's free to download and use - for anything, including commercial
work, and what you make with it is yours - but you may not redistribute or
modify it. See [`LICENSE.md`](LICENSE.md) for the full terms. The source code
is visible here, but being able to read it doesn't grant any further rights.

It includes open-source components under their own licences - Qt, FFmpeg,
LAME, JUCE and others - listed with their licence texts in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and [`licenses/`](licenses/),
which also ship with every download. The source code of the LGPL and MPL
components is published in the
[third-party sources release](https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1).
In the app, **Help → About → Licence / Open-source notices** shows both.

Sound Mind Studio is pre-release software. A public beta is being prepared
- see [`BETA.md`](BETA.md).

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

Package the Microsoft Store MSIX from a Release build (needs the Windows
SDK's `makeappx`) - see `docs/store-submission.md` for the identity
settings, local testing, and submitting:

```
cmake --build build/release --target msix
```

A developer-only performance benchmarking tool also lives here
(`sound-mind-benchmark` - not part of the shipped app) - see
`docs/sound-mind-benchmarking.md`.

A second developer-only tool, `sound-mind-codec-eval`, encodes/decodes real
audio files through the Stream codec and reports fidelity,
timing, and memory metrics to a CSV - see `docs/sound-mind-codec-design.md`
for what the codec itself does, and that tool's own Doxygen docs for how to
run it.

See `CHANGELOG.md` for what's actually implemented so far.
