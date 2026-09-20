#pragma once

namespace sound_mind::core {

/**
 * @brief Whether GPU-accelerated compute is currently enabled - the real,
 *        persisted user preference behind `MainWindow`'s own Hardware
 *        Acceleration menu toggle
 *        (`docs/sound-mind-design.md`'s "Hardware acceleration option",
 *        `v0.0.42.5`, Workflow & Device Polish, Installment E), not a
 *        test-only concept. `true` (GPU allowed, when available) by
 *        default.
 *
 * Backs the exact same underlying flag `setGpuComputeForcedOffForTesting()`
 * does (that function is just this one's own long-standing, test-oriented
 * name and inverted-sense parameter) - `gpuComputeDeviceOrNull()` checks
 * this single flag either way, so a real user toggle and a test override
 * can never disagree about which state is "in effect" at any one time.
 *
 * @return `true` if GPU-accelerated call sites should try the GPU path
 *         (still falling back to CPU if no real device is actually
 *         available); `false` if they should always use their own CPU
 *         implementation instead, regardless of device availability.
 */
[[nodiscard]] bool hardwareAccelerationEnabled();

/**
 * @brief Sets whether GPU-accelerated compute is enabled - see
 *        hardwareAccelerationEnabled()'s own docs.
 * @param enabled `true` to allow the GPU path; `false` to force every
 *        GPU-accelerated call site in this module onto its own CPU
 *        fallback.
 * @note Not thread-safe - see setGpuComputeForcedOffForTesting()'s own
 *       docs on why (the same underlying flag, same threading contract).
 */
void setHardwareAccelerationEnabled(bool enabled);

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
 * A thin, inverted-sense wrapper around setHardwareAccelerationEnabled()
 * (`setGpuComputeForcedOffForTesting(forcedOff)` is exactly
 * `setHardwareAccelerationEnabled(!forcedOff)`) - kept, under its
 * original name, purely so every existing test call site
 * (`test_compositor.cpp`'s/`test_filter_application.cpp`'s own
 * `GpuComputeForcedOffGuard`) keeps working unchanged; new code wanting
 * the real, non-test-specific concept should prefer
 * hardwareAccelerationEnabled()/setHardwareAccelerationEnabled() directly.
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
