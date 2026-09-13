#include "gpu_compute_access.h"

#include <optional>

#include "sound_mind/core/gpu_compute_availability.h"

namespace sound_mind::core {

namespace {

// Test-only override - see setGpuComputeForcedOffForTesting()'s own
// docs. A plain bool, not std::atomic<bool>: every GPU-accelerated call
// site this backs runs on whichever single thread is currently recomputing
// a Layer's cache or recompositing a Project, and tests set this before
// calling into applyFilter()/compositeProject(), not concurrently with a
// GPU dispatch already in flight - matches sound_mind::gpu::ComputeDevice's
// own documented "not thread-safe" contract.
bool gpuComputeForcedOff = false;

}  // namespace

void setGpuComputeForcedOffForTesting(bool forcedOff) { gpuComputeForcedOff = forcedOff; }

namespace detail {

sound_mind::gpu::ComputeDevice* gpuComputeDeviceOrNull() {
    if (gpuComputeForcedOff) {
        return nullptr;
    }
    // Created at most once per process, on whichever thread first asks for
    // it - if that first attempt fails (no D3D12 adapter at all), every
    // later call keeps seeing nullopt too, rather than retrying a failed
    // adapter search on every single filter/composite call.
    static std::optional<sound_mind::gpu::ComputeDevice> device = sound_mind::gpu::ComputeDevice::create();
    return device.has_value() ? &*device : nullptr;
}

}  // namespace detail

}  // namespace sound_mind::core
