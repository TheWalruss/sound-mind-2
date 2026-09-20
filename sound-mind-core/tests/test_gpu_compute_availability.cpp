#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/gpu_compute_availability.h"

using sound_mind::core::hardwareAccelerationEnabled;
using sound_mind::core::setGpuComputeForcedOffForTesting;
using sound_mind::core::setHardwareAccelerationEnabled;

namespace {

/// @brief Restores hardwareAccelerationEnabled() to `true` (its own
/// documented default) when it goes out of scope - even if a test fails
/// partway through - so this file's own manipulation of the shared,
/// process-wide flag never leaks into whichever test Catch2 runs next
/// (matching test_compositor.cpp's/test_filter_application.cpp's own
/// GpuComputeForcedOffGuard precedent).
struct HardwareAccelerationRestoreGuard {
    ~HardwareAccelerationRestoreGuard() { setHardwareAccelerationEnabled(true); }
};

}  // namespace

TEST_CASE("hardwareAccelerationEnabled defaults to true", "[core][gpu_compute_availability]") {
    CHECK(hardwareAccelerationEnabled());
}

TEST_CASE("setHardwareAccelerationEnabled toggles hardwareAccelerationEnabled", "[core][gpu_compute_availability]") {
    HardwareAccelerationRestoreGuard guard;

    setHardwareAccelerationEnabled(false);
    CHECK_FALSE(hardwareAccelerationEnabled());

    setHardwareAccelerationEnabled(true);
    CHECK(hardwareAccelerationEnabled());
}

TEST_CASE("setGpuComputeForcedOffForTesting and hardwareAccelerationEnabled share one underlying flag",
          "[core][gpu_compute_availability]") {
    HardwareAccelerationRestoreGuard guard;

    setGpuComputeForcedOffForTesting(true);
    CHECK_FALSE(hardwareAccelerationEnabled());

    setGpuComputeForcedOffForTesting(false);
    CHECK(hardwareAccelerationEnabled());

    setHardwareAccelerationEnabled(false);
    setGpuComputeForcedOffForTesting(false);  // the "restore normal behavior" call.
    CHECK(hardwareAccelerationEnabled());
}
