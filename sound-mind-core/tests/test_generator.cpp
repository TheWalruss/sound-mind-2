#include <algorithm>
#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/generator.h"
#include "sound_mind/core/gpu_compute_availability.h"
#include "sound_mind/core/generator_configuration.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/codec/parallel.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::GeneratorConfiguration;
using sound_mind::core::GeneratorFamily;
using sound_mind::core::generateContent;
using sound_mind::core::ProjectSettings;

namespace {

ProjectSettings makeTestSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 64;
    settings.binCount = 32;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 20000.0f;
    return settings;
}

}  // namespace

TEST_CASE("generateContent produces content sized to the project's own canvas", "[core][generator]") {
    const ProjectSettings settings = makeTestSettings();
    GeneratorConfiguration config;
    config.family = GeneratorFamily::Lattice;
    config.seed = 42;

    const auto content = generateContent(config, settings);

    REQUIRE(content.frameCount == settings.canvasWidth);
    REQUIRE(content.config.binCount == settings.binCount);
    REQUIRE(content.leftMagnitudeDb.size() == std::size_t{settings.canvasWidth} * settings.binCount);
    REQUIRE(content.rightMagnitudeDb.size() == std::size_t{settings.canvasWidth} * settings.binCount);
    REQUIRE(content.sharedPhaseRadians.size() == std::size_t{settings.canvasWidth} * settings.binCount);
}

TEST_CASE("generateContent is deterministic from its own seed", "[core][generator]") {
    const ProjectSettings settings = makeTestSettings();
    GeneratorConfiguration config;
    config.family = GeneratorFamily::Lattice;
    config.orderChaos = 0.3;
    config.seed = 12345;

    const auto first = generateContent(config, settings);
    const auto second = generateContent(config, settings);

    REQUIRE(first.leftMagnitudeDb == second.leftMagnitudeDb);
    REQUIRE(first.rightMagnitudeDb == second.rightMagnitudeDb);
    REQUIRE(first.sharedPhaseRadians == second.sharedPhaseRadians);
}

TEST_CASE("generateContent produces different content for different seeds", "[core][generator]") {
    const ProjectSettings settings = makeTestSettings();
    GeneratorConfiguration configA;
    configA.family = GeneratorFamily::Lattice;
    configA.seed = 1;
    GeneratorConfiguration configB = configA;
    configB.seed = 2;

    const auto contentA = generateContent(configA, settings);
    const auto contentB = generateContent(configB, settings);

    REQUIRE(contentA.leftMagnitudeDb != contentB.leftMagnitudeDb);
}

TEST_CASE("generateContent's own left and right channels match (a mono-ish lattice texture)", "[core][generator]") {
    const ProjectSettings settings = makeTestSettings();
    GeneratorConfiguration config;
    config.family = GeneratorFamily::Lattice;
    config.seed = 7;

    const auto content = generateContent(config, settings);

    REQUIRE(content.leftMagnitudeDb == content.rightMagnitudeDb);
}

TEST_CASE("generateContent at full chaos (-1) varies more between neighboring cells than at full order (+1)",
          "[core][generator]") {
    const ProjectSettings settings = makeTestSettings();
    GeneratorConfiguration chaosConfig;
    chaosConfig.family = GeneratorFamily::Lattice;
    chaosConfig.orderChaos = -1.0;
    chaosConfig.seed = 99;
    GeneratorConfiguration orderConfig = chaosConfig;
    orderConfig.orderChaos = 1.0;

    const auto chaosContent = generateContent(chaosConfig, settings);
    const auto orderContent = generateContent(orderConfig, settings);

    // Sum of absolute differences between horizontally-adjacent cells in
    // the same row - a cheap "how jagged is this texture" measure. Full
    // order applies many smoothing passes (see generateContent()'s own
    // docs), so it should read as measurably smoother than full chaos's
    // raw, unsmoothed noise.
    const auto roughness = [&](const sound_mind::codec::StreamImage& content) {
        double total = 0.0;
        const std::uint32_t bin = content.config.binCount / 2;
        for (std::uint32_t frame = 1; frame < content.frameCount; ++frame) {
            const std::size_t index = std::size_t{bin} * content.frameCount + frame;
            const std::size_t previousIndex = std::size_t{bin} * content.frameCount + frame - 1;
            total += std::abs(content.leftMagnitudeDb[index] - content.leftMagnitudeDb[previousIndex]);
        }
        return total;
    };

    CHECK(roughness(chaosContent) > roughness(orderContent));
}

// v0.1.41.5 - noise generation and MindWave fields run in parallel.
TEST_CASE("Noise layers and noise MindWave fields are identical on one thread and on many", "[core][generator]") {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 200;
    sound_mind::core::GeneratorConfiguration config;
    config.family = sound_mind::core::GeneratorFamily::Noise;
    config.noise.color = sound_mind::core::NoiseColor::Pink;
    config.noise.horizontalResolution = 3.0;
    config.seed = 7;
    sound_mind::core::MindWave wave;
    wave.setType(sound_mind::core::GeneratorType::Noise);
    wave.setNoiseColor(sound_mind::core::NoiseColor::Pink);
    wave.setNoiseCellWidth(0.02);
    const auto codecConfig = sound_mind::core::streamCodecConfigFor(settings);

    sound_mind::codec::setMaxWorkerThreads(1);
    const auto serialLayer = sound_mind::core::generateContent(config, settings);
    const auto serialField = sound_mind::core::evaluateMindWaveField(wave, codecConfig, settings.canvasWidth);
    sound_mind::codec::setMaxWorkerThreads(8);
    const auto parallelLayer = sound_mind::core::generateContent(config, settings);
    const auto parallelField = sound_mind::core::evaluateMindWaveField(wave, codecConfig, settings.canvasWidth);
    sound_mind::codec::setMaxWorkerThreads(0);

    CHECK(parallelLayer.leftMagnitudeDb == serialLayer.leftMagnitudeDb);
    CHECK(parallelLayer.rightMagnitudeDb == serialLayer.rightMagnitudeDb);
    CHECK(parallelLayer.sharedPhaseRadians == serialLayer.sharedPhaseRadians);
    CHECK(parallelField == serialField);
}

TEST_CASE("coloredNoiseSample gives the same value whichever color was asked for last", "[core][generator]") {
    // The octave weights are cached per color: switching back and forth must not leak.
    const double pink = sound_mind::core::coloredNoiseSample(3, 1, 40, -1.0);
    static_cast<void>(sound_mind::core::coloredNoiseSample(3, 1, 40, -2.0));
    CHECK(sound_mind::core::coloredNoiseSample(3, 1, 40, -1.0) == pink);
}

TEST_CASE("A noise MindWave field reads the same with and without hardware acceleration", "[core][generator][gpu]") {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 300;
    const auto codecConfig = sound_mind::core::streamCodecConfigFor(settings);
    struct Variant {
        sound_mind::core::NoiseColor color;
        sound_mind::core::MindWaveAxis axis;
        bool reversed;
        bool inverted;
    };
    for (const Variant variant : {Variant{sound_mind::core::NoiseColor::White, sound_mind::core::MindWaveAxis::Time, false, false},
                                  Variant{sound_mind::core::NoiseColor::Pink, sound_mind::core::MindWaveAxis::Time, true, false},
                                  Variant{sound_mind::core::NoiseColor::Red, sound_mind::core::MindWaveAxis::Frequency, false, true},
                                  Variant{sound_mind::core::NoiseColor::Purple, sound_mind::core::MindWaveAxis::Frequency, true, true}}) {
        sound_mind::core::MindWave wave;
        wave.setType(sound_mind::core::GeneratorType::Noise);
        wave.setNoiseColor(variant.color);
        wave.setAxis(variant.axis);
        wave.setReversed(variant.reversed);
        wave.setInverted(variant.inverted);
        wave.setNoiseCellWidth(0.013);
        wave.setNoiseCellHeight(3.0);
        wave.setSeed(1234567u);

        sound_mind::core::setHardwareAccelerationEnabled(false);
        const auto cpu = sound_mind::core::evaluateMindWaveField(wave, codecConfig, settings.canvasWidth);
        sound_mind::core::setHardwareAccelerationEnabled(true);
        const auto gpu = sound_mind::core::evaluateMindWaveField(wave, codecConfig, settings.canvasWidth);
        REQUIRE(gpu.size() == cpu.size());
        float worst = 0.0f;
        for (std::size_t i = 0; i < cpu.size(); ++i) {
            worst = std::max(worst, std::abs(gpu[i] - cpu[i]));
        }
        INFO("color " << static_cast<int>(variant.color) << ", axis " << static_cast<int>(variant.axis));
        CHECK(worst < 1e-5f);  // the GPU's float arithmetic vs the CPU's double.
    }
}

