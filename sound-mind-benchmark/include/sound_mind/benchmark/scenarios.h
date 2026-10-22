#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sound_mind/benchmark/benchmark_case.h"
#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/stream_codec.h"

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
///        count). (A Superposition stack-depth sweep was removed with that
///        feature in `v0.1.32.1`.)
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

/// @brief The Stream codec (`v0.1.41.1`): `encode()` and `decode()` of
///        music-like stereo audio at 10, 60 and 180 s, default settings -
///        the transform under playback preparation, export, analysis and
///        recording.
[[nodiscard]] std::vector<BenchmarkCase> buildCodecScenarios();

/// @brief The Analysis panel (`v0.1.41.1`): the whole
///        `analyzeProjectLoudness()` at 1, 4 and 8 layers of 30 s
///        (GPU-eligible, through compositing), and each stage on its own at
///        60 s - loudness, stereo, per-band levels and widths, and pitch at
///        1 and 3 voices.
[[nodiscard]] std::vector<BenchmarkCase> buildAnalysisScenarios();

/// @brief Generators (`v0.1.41.1`): white and pink noise Generate Layers,
///        the Fractal and Streaming families, and a noise MindWave field, at
///        30 s and 120 s canvases.
[[nodiscard]] std::vector<BenchmarkCase> buildGeneratorScenarios();

/// @brief The cases whose category is one of `categories` - the
///        `--category` command-line filter. An empty list keeps every case.
/// @param cases Every case.
/// @param categories The categories to keep.
/// @return The kept cases, in their original order.
[[nodiscard]] std::vector<BenchmarkCase> casesInCategories(std::vector<BenchmarkCase> cases,
                                                           const std::vector<std::string>& categories);

/// @brief The codec settings the codec and analysis scenarios share - a
///        default project's.
/// @return The configuration.
[[nodiscard]] sound_mind::codec::StreamCodecConfig defaultBenchmarkCodecConfig();

/// @brief Deterministic, music-like stereo audio: a few harmonic voices
///        moving through a short progression, plus a little noise.
/// @param seconds The length.
/// @param sampleRateHz The sample rate.
/// @param seed Varies the noise, so layers built from it differ.
/// @return The audio.
[[nodiscard]] sound_mind::codec::AudioBuffer musicLikeAudio(double seconds, std::uint32_t sampleRateHz,
                                                            unsigned seed);

}  // namespace sound_mind::benchmark
