#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/generator.h"
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

TEST_CASE("generateContent returns silent content for the not-yet-implemented Fractal/Streaming families",
          "[core][generator]") {
    const ProjectSettings settings = makeTestSettings();
    GeneratorConfiguration fractalConfig;
    fractalConfig.family = GeneratorFamily::Fractal;
    GeneratorConfiguration streamingConfig;
    streamingConfig.family = GeneratorFamily::Streaming;

    const auto fractalContent = generateContent(fractalConfig, settings);
    const auto streamingContent = generateContent(streamingConfig, settings);

    for (const float value : fractalContent.leftMagnitudeDb) {
        REQUIRE(value <= -90.0f);
    }
    for (const float value : streamingContent.leftMagnitudeDb) {
        REQUIRE(value <= -90.0f);
    }
}
