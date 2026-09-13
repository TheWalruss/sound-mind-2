#pragma once

namespace sound_mind::core {

/**
 * @brief Test-only override forcing `sound-mind-core`'s own GPU-accelerated
 *        call sites (`applyFilter()`'s `UniformBlur` case,
 *        `compositeProject()`'s per-layer mixing) to behave exactly as if
 *        no DirectX 12 device were available, regardless of what
 *        `sound_mind::gpu::ComputeDevice::create()` itself would actually
 *        report on this machine.
 *
 * Exists so the CPU fallback path can be exercised deterministically
 * under test even on a machine where a real GPU (or, at minimum,
 * Microsoft's WARP software adapter) succeeds - see
 * `docs/sound-mind-architecture.md`'s Decision #4 testing strategy for
 * real-time/GPU code (behavioral tests confirming the GPU path *and* the
 * CPU fallback produce equivalent results). Without this override, a
 * machine where `ComputeDevice::create()` always succeeds (WARP, if
 * nothing else) would never actually exercise the CPU fallback branch at
 * this integration point at all.
 *
 * @param forcedOff `true` forces every GPU-accelerated call site in this
 *        module onto its own CPU fallback; `false` (the default) restores
 *        normal "use the GPU when available" behavior.
 * @note Not thread-safe - set this before dispatching work that reads
 *       it, not concurrently with it, matching
 *       `sound_mind::gpu::ComputeDevice`'s own documented threading
 *       contract. Tests using it should reset it back to `false` when
 *       done (an RAII guard is the simplest way), so it doesn't leak
 *       into whichever test runs next.
 */
void setGpuComputeForcedOffForTesting(bool forcedOff);

}  // namespace sound_mind::core
