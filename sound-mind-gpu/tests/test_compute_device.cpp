// Defined before any other include (even Catch2's own, which can pull in
// <Windows.h> transitively on this platform) - without it, Windows.h's
// own max/min macros shadow std::max/std::min wherever it happens to get
// included from, producing a confusing "illegal token on right side of
// ::" MSVC parse error at every std::max/std::min call below.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/gpu/compute_device.h"

using sound_mind::gpu::AdapterKind;
using sound_mind::gpu::AmplitudePhaseSignal;
using sound_mind::gpu::ComputeDevice;

namespace {

/// @brief A device shared across this file's own test cases - real
/// device creation (even against WARP) is comparatively expensive, and
/// every test here only ever reads from it (each `ComputeDevice` method
/// creates and tears down its own resources per call - see its own
/// docs), so sharing one is safe and keeps this suite fast.
ComputeDevice& sharedDevice() {
    static ComputeDevice device = [] {
        auto created = ComputeDevice::create();
        REQUIRE(created.has_value());
        return std::move(*created);
    }();
    return device;
}

// ---------------------------------------------------------------------------
// Independent CPU reference implementations - sound-mind-gpu doesn't (and
// per docs/sound-mind-architecture.md's own Build & Module Layout,
// shouldn't) depend on sound-mind-core, so this file can't call the real
// sound_mind::core::gaussianBlur2D()/mixLayerInto() directly. These are
// deliberately independent re-implementations of the exact same
// documented algorithms, for behavioral comparison only - see
// docs/sound-mind-architecture.md's own Decision #4 testing strategy
// (behavioral tests confirming the GPU path matches a CPU reference).
// ---------------------------------------------------------------------------

std::vector<float> referenceGaussianKernel1D(float sigma) {
    const float s = std::max(0.1f, sigma);
    const int radius = std::max(1, static_cast<int>(std::ceil(4.0f * s)));
    std::vector<float> kernel(static_cast<std::size_t>(radius) * 2 + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) {
        const float weight = std::exp(-(static_cast<float>(i) * static_cast<float>(i)) / (2.0f * s * s));
        kernel[static_cast<std::size_t>(i + radius)] = weight;
        sum += weight;
    }
    for (float& weight : kernel) {
        weight /= sum;
    }
    return kernel;
}

int clampIndex(int index, int size) { return std::max(0, std::min(size - 1, index)); }

std::vector<float> referenceGaussianBlur2D(const std::vector<float>& data, int width, int height, float sigma) {
    const auto kernel = referenceGaussianKernel1D(sigma);
    const int radius = static_cast<int>(kernel.size() / 2);

    std::vector<float> horizontal(data.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int sampleX = clampIndex(x + k, width);
                sum += data[static_cast<std::size_t>(y) * width + sampleX] * kernel[static_cast<std::size_t>(k + radius)];
            }
            horizontal[static_cast<std::size_t>(y) * width + x] = sum;
        }
    }

    std::vector<float> result(data.size());
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int sampleY = clampIndex(y + k, height);
                sum += horizontal[static_cast<std::size_t>(sampleY) * width + x] * kernel[static_cast<std::size_t>(k + radius)];
            }
            result[static_cast<std::size_t>(y) * width + x] = sum;
        }
    }
    return result;
}

constexpr float kMinLinearAmplitude = 1e-7f;

float referenceDbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

float referenceLinearToDb(float amplitude) { return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude)); }

/// @brief Uniform (all-1.0) field of `count` entries - the "no MindWave
/// binding" convenience every existing call site here uses, so the
/// pre-Installment-C1 behavior these tests already assert stays provably
/// unchanged now that mixAmplitudePhaseSignal() always takes a per-cell
/// field alongside the scalar gain.
std::vector<float> uniformField(std::size_t count) { return std::vector<float>(count, 1.0f); }

AmplitudePhaseSignal referenceMix(const AmplitudePhaseSignal& running, const AmplitudePhaseSignal& layer, float gain,
                                    const std::vector<float>& mindWaveField) {
    AmplitudePhaseSignal result;
    const std::size_t count = running.leftMagnitudeDb.size();
    result.leftMagnitudeDb.resize(count);
    result.rightMagnitudeDb.resize(count);
    result.phaseRadians.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float cellGain = gain * mindWaveField[i];
        const float layerLeftLinear = referenceDbToLinear(layer.leftMagnitudeDb[i]) * cellGain;
        const float layerRightLinear = referenceDbToLinear(layer.rightMagnitudeDb[i]) * cellGain;
        const std::complex<float> layerDirection(std::cos(layer.phaseRadians[i]), std::sin(layer.phaseRadians[i]));

        const float runningLeftLinear = referenceDbToLinear(running.leftMagnitudeDb[i]);
        const float runningRightLinear = referenceDbToLinear(running.rightMagnitudeDb[i]);
        const std::complex<float> runningDirection(std::cos(running.phaseRadians[i]), std::sin(running.phaseRadians[i]));

        const std::complex<float> newLeft = runningLeftLinear * runningDirection + layerLeftLinear * layerDirection;
        const std::complex<float> newRight = runningRightLinear * runningDirection + layerRightLinear * layerDirection;

        result.leftMagnitudeDb[i] = referenceLinearToDb(std::abs(newLeft));
        result.rightMagnitudeDb[i] = referenceLinearToDb(std::abs(newRight));
        const std::complex<float> mid = (newLeft + newRight) / 2.0f;
        result.phaseRadians[i] = (std::abs(mid) > 0.0f) ? std::arg(mid) : 0.0f;
    }
    return result;
}

}  // namespace

TEST_CASE("create() succeeds, on a hardware adapter or WARP", "[gpu][compute_device]") {
    const auto& device = sharedDevice();
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
    std::vector<float> input(200);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<float>(i) * 0.5f;
    }

    const auto result = sharedDevice().multiplyByTwo(input);

    REQUIRE(result.size() == 200);
    for (std::size_t i = 0; i < result.size(); ++i) {
        CHECK(result[i] == static_cast<float>(i));
    }
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
}

// ---------------------------------------------------------------------------
// gaussianBlur2D
// ---------------------------------------------------------------------------

TEST_CASE("gaussianBlur2D returns an empty vector for empty input", "[gpu][compute_device][gaussian_blur]") {
    const auto result = sharedDevice().gaussianBlur2D({}, 0, 0, 2.0f);
    CHECK(result.empty());
}

TEST_CASE("gaussianBlur2D throws if width * height doesn't match data.size()",
          "[gpu][compute_device][gaussian_blur]") {
    CHECK_THROWS_AS(sharedDevice().gaussianBlur2D({1.0f, 2.0f, 3.0f}, 2, 2, 1.0f), std::runtime_error);
}

TEST_CASE("gaussianBlur2D leaves a uniform grid unchanged", "[gpu][compute_device][gaussian_blur]") {
    const std::vector<float> data(16 * 16, -40.0f);

    const auto result = sharedDevice().gaussianBlur2D(data, 16, 16, 3.0f);

    REQUIRE(result.size() == data.size());
    for (const float value : result) {
        CHECK(value == Catch::Approx(-40.0f).margin(0.01));
    }
}

TEST_CASE("gaussianBlur2D matches an independent CPU reference implementation, on a real 2D impulse",
          "[gpu][compute_device][gaussian_blur]") {
    constexpr int width = 32;
    constexpr int height = 24;
    std::vector<float> data(static_cast<std::size_t>(width) * height, -96.0f);
    data[static_cast<std::size_t>(height / 2) * width + width / 2] = 0.0f;  // A single loud impulse in the middle.

    const auto expected = referenceGaussianBlur2D(data, width, height, 2.5f);
    const auto actual = sharedDevice().gaussianBlur2D(data, width, height, 2.5f);

    REQUIRE(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        CHECK(actual[i] == Catch::Approx(expected[i]).margin(0.01));
    }
}

TEST_CASE("gaussianBlur2D matches an independent CPU reference implementation, on random-ish data",
          "[gpu][compute_device][gaussian_blur]") {
    constexpr int width = 40;
    constexpr int height = 30;
    std::vector<float> data(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < data.size(); ++i) {
        // Deterministic, not actually random - a repeatable pseudo-noise
        // pattern is all this needs, spanning most of the -96..0 dB range.
        data[i] = -96.0f + 96.0f * static_cast<float>((i * 37 + 11) % 101) / 100.0f;
    }

    const auto expected = referenceGaussianBlur2D(data, width, height, 4.0f);
    const auto actual = sharedDevice().gaussianBlur2D(data, width, height, 4.0f);

    REQUIRE(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        CHECK(actual[i] == Catch::Approx(expected[i]).margin(0.01));
    }
}

TEST_CASE("gaussianBlur2D is measurably faster than the CPU reference on a large grid",
          "[gpu][compute_device][gaussian_blur][performance]") {
    constexpr int width = 512;
    constexpr int height = 512;
    std::vector<float> data(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = -96.0f + 96.0f * static_cast<float>(i % 97) / 96.0f;
    }
    constexpr float sigma = 8.0f;  // A real kernel radius (32), not a trivial one.

    const auto cpuStart = std::chrono::steady_clock::now();
    const auto cpuResult = referenceGaussianBlur2D(data, width, height, sigma);
    const auto cpuDuration = std::chrono::steady_clock::now() - cpuStart;

    const auto gpuStart = std::chrono::steady_clock::now();
    const auto gpuResult = sharedDevice().gaussianBlur2D(data, width, height, sigma);
    const auto gpuDuration = std::chrono::steady_clock::now() - gpuStart;

    REQUIRE(gpuResult.size() == cpuResult.size());
    for (std::size_t i = 0; i < gpuResult.size(); i += 997) {  // Spot-check - correctness is this file's own other tests' job.
        CHECK(gpuResult[i] == Catch::Approx(cpuResult[i]).margin(0.01));
    }

    INFO("CPU: " << std::chrono::duration_cast<std::chrono::microseconds>(cpuDuration).count() << " us, GPU: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(gpuDuration).count() << " us");
    CHECK(gpuDuration < cpuDuration);
}

// ---------------------------------------------------------------------------
// mixAmplitudePhaseSignal
// ---------------------------------------------------------------------------

TEST_CASE("mixAmplitudePhaseSignal returns an empty signal for empty input",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    const AmplitudePhaseSignal empty;
    const auto result = sharedDevice().mixAmplitudePhaseSignal(empty, empty, 1.0f, {});
    CHECK(result.leftMagnitudeDb.empty());
    CHECK(result.rightMagnitudeDb.empty());
    CHECK(result.phaseRadians.empty());
}

TEST_CASE("mixAmplitudePhaseSignal throws if running's and layer's own arrays aren't all the same size",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    AmplitudePhaseSignal running;
    running.leftMagnitudeDb = {-10.0f, -20.0f};
    running.rightMagnitudeDb = {-10.0f, -20.0f};
    running.phaseRadians = {0.0f, 0.0f};
    AmplitudePhaseSignal layer;
    layer.leftMagnitudeDb = {-30.0f};  // Wrong size.
    layer.rightMagnitudeDb = {-30.0f};
    layer.phaseRadians = {0.0f};

    CHECK_THROWS_AS(sharedDevice().mixAmplitudePhaseSignal(running, layer, 1.0f, uniformField(1)), std::runtime_error);
}

TEST_CASE("mixAmplitudePhaseSignal throws if mindWaveField isn't the same size as running/layer",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    AmplitudePhaseSignal running;
    running.leftMagnitudeDb = {-10.0f, -20.0f};
    running.rightMagnitudeDb = {-10.0f, -20.0f};
    running.phaseRadians = {0.0f, 0.0f};
    AmplitudePhaseSignal layer;
    layer.leftMagnitudeDb = {-30.0f, -30.0f};
    layer.rightMagnitudeDb = {-30.0f, -30.0f};
    layer.phaseRadians = {0.0f, 0.0f};

    CHECK_THROWS_AS(sharedDevice().mixAmplitudePhaseSignal(running, layer, 1.0f, uniformField(1)),
                    std::runtime_error);
}

TEST_CASE("mixAmplitudePhaseSignal reproduces a single full-opacity, silent-running layer's own content exactly",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    // Summing one real term against pure silence is the identity - the
    // same property docs/sound-mind-architecture.md's own Decision #59
    // establishes for compositeProject() as a whole.
    AmplitudePhaseSignal silentRunning;
    silentRunning.leftMagnitudeDb = {-96.0f, -96.0f, -96.0f};
    silentRunning.rightMagnitudeDb = {-96.0f, -96.0f, -96.0f};
    silentRunning.phaseRadians = {0.0f, 0.0f, 0.0f};

    AmplitudePhaseSignal layer;
    layer.leftMagnitudeDb = {-10.0f, -20.0f, -5.0f};
    layer.rightMagnitudeDb = {-15.0f, -25.0f, -8.0f};
    layer.phaseRadians = {0.5f, -1.2f, 2.0f};

    const auto result = sharedDevice().mixAmplitudePhaseSignal(silentRunning, layer, 1.0f, uniformField(3));

    REQUIRE(result.leftMagnitudeDb.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(result.leftMagnitudeDb[i] == Catch::Approx(layer.leftMagnitudeDb[i]).margin(0.01));
        CHECK(result.rightMagnitudeDb[i] == Catch::Approx(layer.rightMagnitudeDb[i]).margin(0.01));
        CHECK(result.phaseRadians[i] == Catch::Approx(layer.phaseRadians[i]).margin(0.01));
    }
}

TEST_CASE("mixAmplitudePhaseSignal scales the layer's own contribution by its own gain",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    AmplitudePhaseSignal silentRunning;
    silentRunning.leftMagnitudeDb = {-96.0f};
    silentRunning.rightMagnitudeDb = {-96.0f};
    silentRunning.phaseRadians = {0.0f};

    AmplitudePhaseSignal layer;
    layer.leftMagnitudeDb = {0.0f};  // Full-scale (linear amplitude 1.0).
    layer.rightMagnitudeDb = {0.0f};
    layer.phaseRadians = {0.0f};

    // Half amplitude (linear gain 0.5) is -6.02 dB.
    const auto result = sharedDevice().mixAmplitudePhaseSignal(silentRunning, layer, 0.5f, uniformField(1));

    REQUIRE(result.leftMagnitudeDb.size() == 1);
    CHECK(result.leftMagnitudeDb[0] == Catch::Approx(-6.0206f).margin(0.02));
    CHECK(result.rightMagnitudeDb[0] == Catch::Approx(-6.0206f).margin(0.02));
}

TEST_CASE("mixAmplitudePhaseSignal scales the layer's own contribution by a per-cell MindWave field",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    // Same full-scale layer at three cells, full opacity (gain 1.0), but a
    // MindWave field of {1.0, 0.5, 0.0} (v0.Y.31.1 Installment C1) - full,
    // half, and no contribution respectively, alongside (not replacing)
    // the scalar opacity gain.
    AmplitudePhaseSignal silentRunning;
    silentRunning.leftMagnitudeDb = {-96.0f, -96.0f, -96.0f};
    silentRunning.rightMagnitudeDb = {-96.0f, -96.0f, -96.0f};
    silentRunning.phaseRadians = {0.0f, 0.0f, 0.0f};

    AmplitudePhaseSignal layer;
    layer.leftMagnitudeDb = {0.0f, 0.0f, 0.0f};  // Full-scale (linear amplitude 1.0).
    layer.rightMagnitudeDb = {0.0f, 0.0f, 0.0f};
    layer.phaseRadians = {0.0f, 0.0f, 0.0f};

    const std::vector<float> field = {1.0f, 0.5f, 0.0f};
    const auto result = sharedDevice().mixAmplitudePhaseSignal(silentRunning, layer, 1.0f, field);

    REQUIRE(result.leftMagnitudeDb.size() == 3);
    CHECK(result.leftMagnitudeDb[0] == Catch::Approx(0.0f).margin(0.02));       // Full contribution: 0 dB.
    CHECK(result.leftMagnitudeDb[1] == Catch::Approx(-6.0206f).margin(0.02));   // Half: -6.02 dB.
    CHECK(result.leftMagnitudeDb[2] < -90.0f);                                  // None: back at the silence floor.
}

TEST_CASE("mixAmplitudePhaseSignal matches an independent CPU reference implementation, on real-shaped data",
          "[gpu][compute_device][mix_amplitude_phase_signal]") {
    constexpr std::size_t count = 200;
    AmplitudePhaseSignal running;
    AmplitudePhaseSignal layer;
    running.leftMagnitudeDb.resize(count);
    running.rightMagnitudeDb.resize(count);
    running.phaseRadians.resize(count);
    layer.leftMagnitudeDb.resize(count);
    layer.rightMagnitudeDb.resize(count);
    layer.phaseRadians.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        running.leftMagnitudeDb[i] = -96.0f + 80.0f * static_cast<float>((i * 13 + 3) % 97) / 96.0f;
        running.rightMagnitudeDb[i] = -96.0f + 80.0f * static_cast<float>((i * 19 + 7) % 89) / 88.0f;
        running.phaseRadians[i] = -3.0f + 6.0f * static_cast<float>((i * 5 + 1) % 61) / 60.0f;
        layer.leftMagnitudeDb[i] = -96.0f + 96.0f * static_cast<float>((i * 29 + 2) % 83) / 82.0f;
        layer.rightMagnitudeDb[i] = -96.0f + 96.0f * static_cast<float>((i * 31 + 4) % 79) / 78.0f;
        layer.phaseRadians[i] = -3.0f + 6.0f * static_cast<float>((i * 7 + 9) % 53) / 52.0f;
    }
    constexpr float gain = 0.75f;
    // A non-uniform field too, not just uniformField() - exercises the
    // per-cell multiply itself against the same independent CPU reference,
    // not only the "no binding" case the other tests above already cover.
    std::vector<float> field(count);
    for (std::size_t i = 0; i < count; ++i) {
        field[i] = static_cast<float>((i * 11 + 3) % 100) / 99.0f;
    }

    const auto expected = referenceMix(running, layer, gain, field);
    const auto actual = sharedDevice().mixAmplitudePhaseSignal(running, layer, gain, field);

    REQUIRE(actual.leftMagnitudeDb.size() == expected.leftMagnitudeDb.size());
    for (std::size_t i = 0; i < count; ++i) {
        CHECK(actual.leftMagnitudeDb[i] == Catch::Approx(expected.leftMagnitudeDb[i]).margin(0.02));
        CHECK(actual.rightMagnitudeDb[i] == Catch::Approx(expected.rightMagnitudeDb[i]).margin(0.02));
        CHECK(actual.phaseRadians[i] == Catch::Approx(expected.phaseRadians[i]).margin(0.02));
    }
}

TEST_CASE("mixAmplitudePhaseSignal is measurably faster than the CPU reference on a large signal",
          "[gpu][compute_device][mix_amplitude_phase_signal][performance]") {
    constexpr std::size_t count = 500000;
    AmplitudePhaseSignal running;
    AmplitudePhaseSignal layer;
    running.leftMagnitudeDb.resize(count);
    running.rightMagnitudeDb.resize(count);
    running.phaseRadians.resize(count);
    layer.leftMagnitudeDb.resize(count);
    layer.rightMagnitudeDb.resize(count);
    layer.phaseRadians.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        running.leftMagnitudeDb[i] = -96.0f + 80.0f * static_cast<float>(i % 97) / 96.0f;
        running.rightMagnitudeDb[i] = -96.0f + 80.0f * static_cast<float>(i % 89) / 88.0f;
        running.phaseRadians[i] = -3.0f + 6.0f * static_cast<float>(i % 61) / 60.0f;
        layer.leftMagnitudeDb[i] = -96.0f + 96.0f * static_cast<float>(i % 83) / 82.0f;
        layer.rightMagnitudeDb[i] = -96.0f + 96.0f * static_cast<float>(i % 79) / 78.0f;
        layer.phaseRadians[i] = -3.0f + 6.0f * static_cast<float>(i % 53) / 52.0f;
    }

    const std::vector<float> field = uniformField(count);

    const auto cpuStart = std::chrono::steady_clock::now();
    const auto cpuResult = referenceMix(running, layer, 0.8f, field);
    const auto cpuDuration = std::chrono::steady_clock::now() - cpuStart;

    const auto gpuStart = std::chrono::steady_clock::now();
    const auto gpuResult = sharedDevice().mixAmplitudePhaseSignal(running, layer, 0.8f, field);
    const auto gpuDuration = std::chrono::steady_clock::now() - gpuStart;

    REQUIRE(gpuResult.leftMagnitudeDb.size() == cpuResult.leftMagnitudeDb.size());
    for (std::size_t i = 0; i < count; i += 4999) {  // Spot-check - correctness is this file's own other tests' job.
        CHECK(gpuResult.leftMagnitudeDb[i] == Catch::Approx(cpuResult.leftMagnitudeDb[i]).margin(0.02));
    }

    INFO("CPU: " << std::chrono::duration_cast<std::chrono::microseconds>(cpuDuration).count() << " us, GPU: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(gpuDuration).count() << " us");
    CHECK(gpuDuration < cpuDuration);
}
