#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace sound_mind::benchmark {

/**
 * @brief One benchmarkable operation - a name, a category, its own
 *        parameters (recorded into the JSON report verbatim, for
 *        whatever a later analysis pass wants to group/filter by), and
 *        the actual callable to time - see `docs/sound-mind-benchmarking.md`.
 *
 * A scenario builder (`scenarios.h`, one function per category) owns
 * capturing whatever fixture data `run` needs (a synthetic `StreamImage`,
 * a `FilterConfiguration`, ...) - typically by `std::shared_ptr` into the
 * lambda, so the same fixture is reused across every timed call rather
 * than rebuilt per call (which would measure fixture construction, not
 * the operation itself).
 *
 * `run` must be safe to call repeatedly without behaving differently the
 * second time - `measure()` (`benchmark_timer.h`) calls it several times
 * in a row (warmup, then timed samples) and assumes each call does the
 * same amount of work. An operation that's naturally idempotent on fixed
 * input (every pure Core function this suite exercises - `applyFilter()`,
 * `evaluateMindWaveField()`, `compositeProject()`) already satisfies this
 * for free; one that *mutates* its own input in place (`applyPaintOperation()`,
 * which paints directly into a `StreamImage`) needs its own scenario to
 * either re-seed that input before each call or confirm the mutation
 * itself is idempotent for the specific operation being measured (a paint
 * stroke blending toward a fixed target converges, rather than drifting
 * further, on repeated application - see `paint_scenarios.cpp`'s own
 * docs on why this holds for every scenario it builds).
 */
struct BenchmarkCase {
    /// @brief Short, human-readable identity - distinct within `category`,
    ///        not necessarily globally (e.g. every category might have its
    ///        own "canvas=large" entry).
    std::string name;

    /// @brief Which broad area this belongs to - "filter", "paint",
    ///        "mindWave", or "compositing" for the four builders this
    ///        suite ships with; a future one is free to introduce its own.
    std::string category;

    /// @brief Whatever parameters distinguish this case from its own
    ///        siblings (canvas dimensions, kernel size, layer count, ...) -
    ///        copied into the JSON report's own entry verbatim, so a
    ///        later analysis pass can group/filter by them without this
    ///        suite needing to know in advance which ones matter.
    nlohmann::json parameters;

    /// @brief Whether this operation has a real GPU-accelerated code path
    ///        (`sound_mind::core::hardwareAccelerationEnabled()` actually
    ///        changes what it does) - see that function's own docs for
    ///        exactly which two call sites currently qualify
    ///        (`applyFilter()`'s `UniformBlur` case, `compositeProject()`'s
    ///        per-layer mixing). `main.cpp` runs a `true` case twice (once
    ///        with the GPU path enabled, once forced onto its own CPU
    ///        fallback) to produce a direct comparison; a `false` case
    ///        (the default) runs once.
    bool gpuEligible = false;

    /// @brief The operation to time - see this struct's own docs on the
    ///        "safe to call repeatedly" contract `measure()` depends on.
    std::function<void()> run;
};

}  // namespace sound_mind::benchmark
