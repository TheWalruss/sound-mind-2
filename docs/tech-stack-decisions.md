# Sound Mind Studio — Tech Stack Decisions

Reference notes from the initial planning conversation. Previous version was Python/PySide6/NumPy — solid for a spectrogram editor, but the GIL and GC make it a poor fit for a real-time DAW. This rewrite targets a professional-grade DAW + graphics studio with responsiveness/performance as a core design constraint.

## Core Language: C++

- Chosen over Rust primarily on developer background (experienced with C/C++ and Qt already).
- Real-time audio thread must never touch Python, GC'd languages, or anything with unpredictable allocation/locking.
- Needs a refresh on modern C++ idioms (C++11 through C++20/23) before deep implementation work.

### Learning resources for modern C++
- *Effective Modern C++* — Scott Meyers (move semantics, smart pointers, lambdas, `auto`)
- *A Tour of C++*, 3rd ed. — Bjarne Stroustrup (fast map of current language)
- *C++ Concurrency in Action*, 2nd ed. — Anthony Williams (memory model, atomics, lock-free — essential for audio thread work)
- *Professional CMake* — Craig Scott (modern target-based build practices)
- C++ Core Guidelines (isocpp.github.io/CppCoreGuidelines) — living style reference
- cppreference.com — day-to-day reference
- CppCon talks (YouTube), especially Herb Sutter and Jason Turner
- Compiler Explorer (godbolt.org) — inspect what modern C++ actually compiles to

## Frameworks
- **Audio/DSP**: JUCE — industry-standard for DAWs and plugins (VST3/AU hosting, lock-free audio callback patterns, real-time-safe design). Its codebase is also a good real-world reference for modern C++ idioms applied to audio constraints.
- **GUI**: Leaning on JUCE's own GUI module (avoids a second toolkit) or Qt, given existing Qt experience. Decision can stay open — both are viable.

## GPU Compute: DirectX 12 Compute
- Chosen for cross-vendor support (Nvidia, AMD, Intel) on Windows, good documentation, and Windows-first target platform.
- Used for spectrogram/DSP transform acceleration (NSGT-style processing) and image/graphics work.
- Avoid CUDA — Nvidia-only, doesn't fit a general desktop audience.
- Vulkan Compute was considered as a cross-platform alternative but DX12 fits better given the Windows-first, C++ direction.

## Development Environment: Snapdragon X laptop (Arm64) + VS Code

- Install **native Arm64 Visual Studio 2022/2026** — native Arm64 MSVC toolset is meaningfully faster than the older x64-emulated compiler.
- VS Code + C++ extension natively support Arm64 with full IntelliSense/build support.
- clang-cl (bundled via LLVM in VS) available as an MSVC-compatible alternative compiler driver if needed.
- vcpkg runs natively on Arm64 for dependency management (600+ libraries buildable natively).
- Cross-compiling for x64/x86 from the Arm64 machine works via standard VS/CMake platform config, but **validate real x64 builds via CI** (e.g., GitHub Actions x64 runners) rather than relying solely on cross-compiled local binaries — some third-party libraries/plugins may lack Arm64 packaging.
- Qualcomm's "Windows on Snapdragon Porter" VS Code extension can help analyze/port x64-specific code (including SIMD intrinsic translation, x86 SSE/AVX → Arm NEON) — useful reference given the audio DSP will lean on SIMD.

## Performance Validation Strategy
- Developing/testing primarily on the Snapdragon X + Adreno laptop: if it performs well there, it should perform even better on more powerful, compatible desktop hardware.
- **Caveat**: Adreno is a mobile-class GPU and not representative of desktop Nvidia/AMD GPU compute performance. Real DX12 compute performance on Nvidia/AMD hardware needs to be validated separately (physical desktop access or cloud GPU instance) — don't assume Adreno benchmarks predict desktop GPU behavior.
- Before committing to specific third-party audio libraries (VST SDK, codec libs, etc.), verify current Arm64 Windows packaging status — coverage was inconsistent as recently as the last year.

## Open Decisions (not yet settled)
- GUI framework: JUCE's built-in GUI vs. Qt
- Final DSP library choices for NSGT-equivalent processing in C++
- Plugin format support scope (VST3/AU) if applicable
