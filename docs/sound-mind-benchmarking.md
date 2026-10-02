# Sound Mind - the Benchmarking Suite

Status: **first installment, `v0.1.6.1`.** This document describes `sound-mind-benchmark`, a developer-only measurement tool added to start the performance-hardening pass `docs/sound-mind-roadmap.md`'s `v0.Y.60.1` (Performance Validation & Hardening) calls for. It does not replace that milestone - it's the instrument the milestone (and the Studio-side "Performance Hints" feature described in `docs/sound-mind-design.md`) will eventually read from.

This document assumes familiarity with `sound-mind-architecture.md` (see its Decision #195 for this suite's own architectural rationale) and doesn't repeat what's already in the Doxygen comments on the classes themselves - it's the narrative connecting them, not a field-by-field restatement.

## Purpose

Two purposes, in order:

1. **Find things to optimize.** Running the suite surfaces real cost data - which filters are slow, how badly a given operation scales with canvas size/kernel size/layer count, and whether GPU acceleration is actually paying for itself on a given machine - to guide the recomputation/precomputation/GPU-offload work the roadmap's performance-hardening milestone describes.
2. **Feed future Studio-side performance hints.** Not built yet (see `docs/sound-mind-design.md`'s "Performance Hints" section) - the long-term idea is that a user runs this suite on their own machine once, and the Studio then uses that machine-specific report to badge slow operations, let a project hide them, and offer tuning advice. `BenchmarkReport` (`benchmark_report.h`) is deliberately write-only today (no `fromJson()`) because nothing consumes a report yet; this is purpose 2's first dependency, not its implementation.

## What this is not

- **Not part of the shipped app.** `sound-mind-benchmark` has no `install()` rule and isn't linked into any packaged artifact - it's a developer tool, run manually, same spirit as the test suites.
- **Not a correctness check.** Every scenario only measures wall-clock time; none of them assert on the numeric result of the operation they run (Core's own test suites already cover correctness). A benchmark case's content (random seeds, canvas sizes, strokes) is chosen to produce realistic *cost*, not a meaningful visual/audio result.
- **Not a substitute for real desktop GPU validation.** This laptop's Adreno GPU is mobile-class, not representative of the desktop Nvidia/AMD hardware `v0.Y.60.1` ultimately needs to validate against (see `tech-stack-decisions.md`) - a GPU-vs-CPU comparison run here tells you GPU dispatch is *working* and roughly how much it helps on *this* machine, not what it'll do on a user's desktop card.

## Building and running it

```
./_build_release.bat --target sound-mind-benchmark
./build/release/sound-mind-benchmark/sound-mind-benchmark.exe
```

**Must be a Release build.** Debug's lack of optimization and iterator/bounds-checking overhead produce timings with no relationship to what a real user experiences - `main.cpp` checks `SOUND_MIND_BENCHMARK_BUILD_CONFIG` at startup and prints a loud warning (not a hard failure, so a quick Debug smoke test to confirm the suite itself still runs is still possible) if it wasn't built as Release.

Running it prints one line per case as it completes, then writes the full report to `benchmark-results/report-<UTC timestamp>.json` (the directory is created if it doesn't exist, and is `.gitignore`d - reports are per-machine data, not something to commit). A full run currently takes a few minutes; see "A note on runtime" below for why the kernel-size sweeps were deliberately kept smaller than a naive doubling pattern would suggest.

## How a case is measured

Every `BenchmarkCase` (`benchmark_case.h`) pairs a name/category/JSON parameter bag with a no-argument `run()` closure that performs exactly one operation, safe to call repeatedly with no state carried over between calls (each case owns its own input data, constructed once when the case itself is built).

`measure()` (`benchmark_timer.h`) runs `run()` a fixed number of warmup calls (discarded, to avoid cold-cache/branch-prediction skew) followed by a fixed number of timed sample calls, using `std::chrono::steady_clock`, and reports mean/min/max/sample standard deviation (n-1 denominator). The defaults (2 warmup, 5 sample) are a plain engineering judgment call: enough samples for a stddev to mean something without materially lengthening an already multi-minute suite run.

**GPU-eligible cases get measured twice.** `BenchmarkCase::gpuEligible` marks a case whose underlying call site actually branches on `sound_mind::core::hardwareAccelerationEnabled()` (as of this writing, that's only `applyFilter()`'s `UniformBlur` case and `compositeProject()`'s per-layer mixing - see `gpu_compute_availability.h`). For those, `main.cpp` measures once normally, then calls `setHardwareAccelerationEnabled(false)`, measures again, and restores it - producing two `BenchmarkResult` entries with identical `name`/`category`/`parameters` and opposite `gpuEnabled`, letting a later analysis pass pair them up directly. Every other case only ever reflects whatever the hardware-acceleration setting already was (since it has no GPU path to disable).

## Scenario catalog

Four scenario files, one per Core subsystem, each exposed as a `build*Scenarios()` function declared in `scenarios.h` and concatenated by `main.cpp`. The general methodology is **one-at-a-time parameter sweeps against a documented baseline**, not a full combinatorial cross-product - sweeping every parameter against every other would make the suite's own runtime impractical for routine use, and most of the interesting questions ("does this scale with X?") only need X varied in isolation.

### Filters (`filter_scenarios.cpp`)

- **One baseline case per `FilterType`** (all 21), at a fixed "medium" canvas (512 bins x 2000 frames) - establishes a per-filter-type cost floor. Two filter types need a non-default nudge to measure real work instead of a near-no-op: `FrequencyAxisGradient` (whose default gradient is fully transparent) gets an explicit opaque ramp, and `Convolve` (whose default kernel is identity-like) gets a 5x5 averaging kernel.
- **Kernel/parameter-size sweeps**: `UniformBlur` sigma `{1, 8, 32}` (GPU-eligible), `EdgePreservingBlur` medianSize `{3, 7, 11}`, `DirectionalBlur` length `{5, 15, 35}`, `Convolve` kernelSize `{3, 7, 11}`, `Downsample` blockSize `{2, 8, 32}`.
- **Canvas-size sweep**: `UniformBlur` (GPU-eligible) across five named sizes - small, medium, large, and two deliberately extreme aspect ratios (wide/short, tall/narrow), since a filter's own per-axis algorithm can behave very differently at an extreme aspect ratio than a square-ish one.

#### A note on runtime: why the kernel-size sweeps are smaller than they look

An initial version of the `EdgePreservingBlur`/`Convolve` sweeps used `{3, 9, 25}`/`{3, 9, 21}`, following what seemed like a reasonable doubling-ish progression. A smoke test showed `EdgePreservingBlur medianSize=9` already taking ~3.3 seconds (mean of 5) at the medium canvas size, and `medianSize=25` didn't complete in any reasonable time at all (projected 20+ minutes for its own 5 sample runs) - both filters cost roughly `O(size^2)` per cell (a windowed median, and a general 2D convolution, both scan a `size x size` neighborhood per output cell), so doubling the window size roughly quadruples the cost. The sweeps were reduced to `{3, 7, 11}` for both - still enough spread to show the scaling trend clearly (and that trend is itself a real finding worth acting on in the performance-hardening pass) without making a routine suite run impractical.

### Paint operations (`paint_scenarios.cpp`)

- **One baseline case per `ToolType`** (9 of 10 - `Clone` excluded, since it has no standalone single-stroke benchmark shape distinct from `Smudge`/`Heal`'s own already-covered sampling pattern), each a single diagonal stroke on a medium canvas.
- **Stroke-count sweep**: `{1, 10, 100}` strokes of a `Procedural` brush at a fixed *total* path length (so stroke count is varied, not total ink) - directly answers "is painting dominated by per-stroke overhead or by total ink."
- **Canvas-size sweep**: `Procedural` across the same three core sizes (small/medium/large) the other scenario files use.

### MindWaves (`mind_wave_scenarios.cpp`)

- **One baseline case per `GeneratorType`** (all 9), evaluated as a full field at a medium canvas width.
- **Fractal field-size sweep**: `Fractal` (the generator whose cost most directly scales with per-cell iteration count) across small/medium/large field sizes.
- **Superposition stack-depth sweep**: `{1, 2, 4, 8}` stacked waves - answers "does combining several MindWaves meaningfully add up."

### Compositing (`compositing_scenarios.cpp`)

- **Layer-count sweep**: `{1, 5, 20, 50}` layers, `Normal` blend mode, GPU-eligible (`compositeProject()`'s per-layer mixing is one of the two GPU-eligible call sites in the whole codebase).
- **Blend-mode sweep**: all 7 `BlendMode`s at a fixed, moderate layer count (10) - answers "did the user choose an expensive blend mode," GPU-eligible.

Every compositing case uses deterministic pseudo-random per-layer content (seeded differently per layer) rather than identical layers, since some blend modes (`Difference`, in particular) would otherwise degenerate into trivially cheap, branch-predictable work no real layered project would ever actually produce.

## JSON schema

`BenchmarkReport::kSchemaVersion` is currently `1`. Top-level shape (see `benchmark_report.h`'s Doxygen for the authoritative field docs):

```json
{
  "schemaVersion": 1,
  "generatedAtUtc": "2026-10-02T18:30:00Z",
  "gitCommit": "a1b2c3d",
  "buildConfiguration": "Release",
  "results": [
    {
      "name": "UniformBlur sigma=32",
      "category": "filter",
      "parameters": { "canvas": "medium", "binCount": 512, "frameCount": 2000 },
      "gpuEnabled": true,
      "timing": { "meanMs": 93.3, "minMs": 90.1, "maxMs": 97.8, "stddevMs": 2.9, "sampleCount": 5 }
    }
  ]
}
```

- `schemaVersion` is bumped whenever a field is added, removed, or changes meaning - a future consumer can use it to detect (and reject or migrate) an incompatible report rather than silently misreading one.
- `parameters` is a free-form JSON object specific to that case's own scenario file - there's no fixed schema across categories, since a filter's parameters (canvas/kernel size) and a compositing case's (layer count/blend mode) have nothing in common.
- A GPU-eligible case appears as exactly two `results` entries sharing the same `name`/`category`/`parameters`, differing only in `gpuEnabled` and `timing` - see "How a case is measured" above.

## Adding a new scenario

Add cases to whichever of the four `build*Scenarios()` functions fits, or add a new scenario file (register it in `CMakeLists.txt`'s `sound-mind-benchmark-lib` sources, declare its `build*Scenarios()` in `scenarios.h`, and call it from `main.cpp`'s `appendAllScenarios()`) if it covers a Core subsystem none of the four already represents. Keep a new sweep's own parameter range informed by an actual smoke-test run before committing to it - see "A note on runtime" above for why that matters.
