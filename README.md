# sound-mind-2
The official Sound Mind Project

See `docs/sound-mind-design.md` for what this is, and
`docs/sound-mind-architecture.md` / `docs/tech-stack-decisions.md` for how
it's being built.

## Building

Prerequisites: a native Arm64 or x64 MSVC toolset (Visual Studio 2022/2026
or Build Tools, with the matching architecture's C++ workload), CMake,
Ninja, [vcpkg](https://github.com/microsoft/vcpkg) bootstrapped somewhere
with `VCPKG_ROOT` set, and Qt 6.8+ installed via
[aqtinstall](https://github.com/miurahr/aqtinstall) with `QT_ROOT_DIR`
pointing at it (and, only for a cross-compiled Arm64 Qt build,
`QT_HOST_PATH_DIR` pointing at the matching x64 Qt install alongside it).

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
