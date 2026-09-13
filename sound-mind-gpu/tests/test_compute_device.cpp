#include <cstddef>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/gpu/compute_device.h"

using sound_mind::gpu::AdapterKind;
using sound_mind::gpu::ComputeDevice;

namespace {

/// @brief A device shared across this file's own test cases - real
/// device creation (even against WARP) is comparatively expensive, and
/// every test here only ever reads from it (`multiplyByTwo()` creates
/// and tears down its own resources per call - see its own docs), so
/// sharing one is safe and keeps this suite fast.
ComputeDevice& sharedDevice() {
    static ComputeDevice device = [] {
        auto created = ComputeDevice::create();
        REQUIRE(created.has_value());
        return std::move(*created);
    }();
    return device;
}

}  // namespace

TEST_CASE("create() succeeds, on a hardware adapter or WARP", "[gpu][compute_device]") {
    const auto& device = sharedDevice();
    // Not asserting which one specifically - that depends on this
    // machine - only that it's a valid, real outcome either way. Logged
    // (not asserted) since it's genuinely useful, machine-specific
    // information: which fallback tier a real run actually landed on.
    switch (device.adapterKind()) {
        case AdapterKind::Hardware:
            WARN("Adapter kind: Hardware");
            break;
        case AdapterKind::Warp:
            WARN("Adapter kind: WARP (software) - no hardware DX12 adapter was available on this run.");
            break;
    }
}

TEST_CASE("multiplyByTwo returns an empty vector for empty input, without dispatching anything",
          "[gpu][compute_device]") {
    const auto result = sharedDevice().multiplyByTwo({});
    CHECK(result.empty());
}

TEST_CASE("multiplyByTwo doubles a small input that fits in one thread group", "[gpu][compute_device]") {
    const auto result = sharedDevice().multiplyByTwo({1.0f, 2.0f, 3.0f});

    REQUIRE(result.size() == 3);
    CHECK(result[0] == 2.0f);
    CHECK(result[1] == 4.0f);
    CHECK(result[2] == 6.0f);
}

TEST_CASE("multiplyByTwo handles negative, zero, and fractional values correctly", "[gpu][compute_device]") {
    const auto result = sharedDevice().multiplyByTwo({-4.5f, 0.0f, 0.25f, -1.0f});

    REQUIRE(result.size() == 4);
    CHECK(result[0] == -9.0f);
    CHECK(result[1] == 0.0f);
    CHECK(result[2] == 0.5f);
    CHECK(result[3] == -2.0f);
}

TEST_CASE("multiplyByTwo is correct exactly at a thread-group boundary (64 elements)", "[gpu][compute_device]") {
    std::vector<float> input(64);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<float>(i);
    }

    const auto result = sharedDevice().multiplyByTwo(input);

    REQUIRE(result.size() == 64);
    for (std::size_t i = 0; i < result.size(); ++i) {
        CHECK(result[i] == static_cast<float>(i) * 2.0f);
    }
}

TEST_CASE("multiplyByTwo is correct across multiple thread groups, including a partial last one",
          "[gpu][compute_device]") {
    // 200 elements over [numthreads(64,1,1)] dispatches 4 groups (256
    // threads total) - the last group's own final 56 threads must not
    // read/write past either buffer's own end (the shader's own bounds
    // check - see multiply_by_two.hlsl).
    std::vector<float> input(200);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<float>(i) * 0.5f;
    }

    const auto result = sharedDevice().multiplyByTwo(input);

    REQUIRE(result.size() == 200);
    for (std::size_t i = 0; i < result.size(); ++i) {
        CHECK(result[i] == static_cast<float>(i));
    }
    // The boundary cells specifically - the ones most likely to be wrong
    // if the dispatch/bounds-check math were off by one.
    CHECK(result[63] == 63.0f);
    CHECK(result[64] == 64.0f);
    CHECK(result[199] == 199.0f);
}

TEST_CASE("a moved-to ComputeDevice remains usable, and the moved-from one destructs safely",
          "[gpu][compute_device]") {
    auto created = ComputeDevice::create();
    REQUIRE(created.has_value());

    ComputeDevice moved = std::move(*created);
    const auto result = moved.multiplyByTwo({10.0f, 20.0f});

    REQUIRE(result.size() == 2);
    CHECK(result[0] == 20.0f);
    CHECK(result[1] == 40.0f);
    // *created (moved-from) goes out of scope right after this test,
    // alongside `moved` - both destructing without a double-close on the
    // shared fence event is the actual thing under test here.
}
