#pragma once

#include <vector>

#include "sound_mind/benchmark/benchmark_case.h"

namespace sound_mind::benchmark {

/// @brief Every filter-related `BenchmarkCase` - one baseline case per
///        `sound_mind::core::FilterType` at a shared medium canvas size,
///        a kernel-size sweep for every filter with an obvious size-like
///        parameter (`UniformBlur`/`EdgePreservingBlur`/`DirectionalBlur`/
///        `Convolve`/`Downsample`), and a canvas-size sweep for
///        `UniformBlur` specifically (GPU-eligible, so this doubles as
///        the GPU-vs-CPU scaling comparison) - see
///        `docs/sound-mind-benchmarking.md`'s own "Scenario catalog".
[[nodiscard]] std::vector<BenchmarkCase> buildFilterScenarios();

/// @brief Every paint-related `BenchmarkCase` - one baseline case per
///        `sound_mind::core::ToolType`, a "many small strokes vs. few
///        large strokes" comparison at a fixed total stroke length, and a
///        canvas-size sweep for a Procedural stroke.
[[nodiscard]] std::vector<BenchmarkCase> buildPaintScenarios();

/// @brief Every MindWave-related `BenchmarkCase` - one baseline case per
///        `sound_mind::core::GeneratorType` at a shared field size, a
///        field-size sweep for `Fractal` (the generator with the most
///        obviously-scaling-with-detail cost, via its own iteration
///        count), and a Superposition stack-depth sweep (1/2/4/8 stacked
///        waves).
[[nodiscard]] std::vector<BenchmarkCase> buildMindWaveScenarios();

/// @brief Every compositing-related `BenchmarkCase` -
///        `sound_mind::core::compositeProject()` across a layer-count
///        sweep (GPU-eligible, so each count also doubles as a GPU-vs-CPU
///        comparison), a blend-mode sweep at a fixed layer count, and a
///        `sound_mind::core::compositeProjectCached()` sweep at the same
///        layer counts (a warmed cache, re-timing only a single top-layer
///        edit's own recomposite) - directly comparable against the first
///        sweep's own cold numbers at the same `layerCount`.
[[nodiscard]] std::vector<BenchmarkCase> buildCompositingScenarios();

}  // namespace sound_mind::benchmark
