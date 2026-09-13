#pragma once

// Must precede every include in any translation unit that reaches this
// header, not just this header's own #include below: compute_device.h's
// <wrl/client.h> transitively pulls in <Windows.h>, whose own max/min
// macros silently shadow std::max/std::min (and std::numeric_limits<T>::max()
// et al.) for every line *after* that first transitive include, in
// whichever .cpp file included this header - sound-mind-gpu's own
// compute_device.cpp/test_compute_device.cpp hit this exact bug twice
// (see docs/sound-mind-architecture.md's Decision #70). Defining these
// here, before compute_device.h's own include, protects every consumer
// of this header regardless of where in its own file it places the
// #include "gpu_compute_access.h" line.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "sound_mind/gpu/compute_device.h"

/// @brief Shared access to `sound-mind-core`'s own single, lazily-created
/// `sound_mind::gpu::ComputeDevice` - the thing `filter_application.cpp`'s
/// `UniformBlur` case and `compositor.cpp`'s layer-mixing both dispatch
/// through when a GPU is available. A private, `src/`-only header (the
/// same "duplicated, not shared" module-boundary precedent
/// `docs/sound-mind-architecture.md`'s Decision #12 already establishes
/// for `sound-mind-codec`'s own `stream_frame_codec.h`) rather than a
/// public one: nothing outside these two `.cpp` files needs this -
/// `sound_mind::core::setGpuComputeForcedOffForTesting()` (the public,
/// test-only override declared in
/// `sound_mind/core/gpu_compute_availability.h`) reaches the same shared
/// state from `gpu_compute_access.cpp` directly, not through this header.
namespace sound_mind::core::detail {

/// @brief The process-wide `sound_mind::gpu::ComputeDevice` this module
/// shares between every GPU-accelerated call site, created lazily on
/// first use (function-local static initialization - thread-safe per the
/// standard's "magic statics" guarantee) and reused for the lifetime of
/// the process. Never rebuilt per call, unlike the ephemeral per-call
/// resources (root signature, PSO, buffers) `ComputeDevice`'s own methods
/// each allocate and tear down internally - see its own docs.
///
/// @return A pointer to the shared device, or `nullptr` if no D3D12
///         adapter (hardware or WARP) is available at all, or if
///         `sound_mind::core::setGpuComputeForcedOffForTesting(true)` is
///         currently in effect - callers should fall back to their own
///         existing CPU implementation exactly as if this returned
///         `nullptr` for a real hardware reason, in both cases.
sound_mind::gpu::ComputeDevice* gpuComputeDeviceOrNull();

}  // namespace sound_mind::core::detail
