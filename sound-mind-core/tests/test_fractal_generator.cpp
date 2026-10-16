#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/generator.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::GeneratorConfiguration;
using sound_mind::core::GeneratorFamily;
using sound_mind::core::generateContent;
using sound_mind::core::ProjectSettings;

namespace {

ProjectSettings fractalSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 400;
    settings.binCount = 128;
    settings.minFrequencyHz = 50.0f;
    settings.maxFrequencyHz = 12800.0f;
    return settings;
}

GeneratorConfiguration fractalConfig(double orderChaos, std::uint64_t seed = 21) {
    GeneratorConfiguration config;
    config.family = GeneratorFamily::Fractal;
    config.orderChaos = orderChaos;
    config.seed = seed;
    return config;
}

/// @brief How many bins are ever louder than -40 dB - how many distinct
/// pitches the tree reaches.
int activeBins(const sound_mind::codec::StreamImage& image) {
    int count = 0;
    for (std::uint32_t bin = 0; bin < image.config.binCount; ++bin) {
        for (std::uint32_t frame = 0; frame < image.frameCount; ++frame) {
            if (image.leftMagnitudeDb[sound_mind::core::cellIndex(bin, frame, image.frameCount)] > -40.0f) {
                ++count;
                break;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("The Fractal generator grows audible, seeded content", "[core][generator][fractal]") {
    const auto settings = fractalSettings();
    const auto content = generateContent(fractalConfig(0.0), settings);

    REQUIRE(content.frameCount == settings.canvasWidth);
    CHECK(*std::max_element(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end()) > -20.0f);
    CHECK(activeBins(content) > 1);  // branches, not just the trunk.
    CHECK(generateContent(fractalConfig(0.0), settings).leftMagnitudeDb == content.leftMagnitudeDb);
    CHECK(generateContent(fractalConfig(0.0, 22), settings).leftMagnitudeDb != content.leftMagnitudeDb);
}

TEST_CASE("Toward order the Fractal tree reaches fewer pitches than toward chaos", "[core][generator][fractal]") {
    const auto settings = fractalSettings();
    int ordered = 0;
    int chaotic = 0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        ordered += activeBins(generateContent(fractalConfig(1.0, seed), settings));
        chaotic += activeBins(generateContent(fractalConfig(-1.0, seed), settings));
    }
    CHECK(ordered < chaotic);
}

TEST_CASE("Fractal branches advance their phase like the tones they are, and decode cleanly",
          "[core][generator][fractal]") {
    const auto settings = fractalSettings();
    const auto content = generateContent(fractalConfig(1.0), settings);
    const auto& config = content.config;

    // The loudest cell of frame 50, and the same bin one frame later.
    std::uint32_t loudest = 0;
    for (std::uint32_t bin = 1; bin < config.binCount; ++bin) {
        if (content.leftMagnitudeDb[sound_mind::core::cellIndex(bin, 50u, content.frameCount)] >
            content.leftMagnitudeDb[sound_mind::core::cellIndex(loudest, 50u, content.frameCount)]) {
            loudest = bin;
        }
    }
    const double frequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(loudest), config);
    const double expected = 2.0 * std::numbers::pi * frequencyHz * config.hopLength / config.sampleRateHz;
    const double actual = content.sharedPhaseRadians[sound_mind::core::cellIndex(loudest, 51u, content.frameCount)] -
                          content.sharedPhaseRadians[sound_mind::core::cellIndex(loudest, 50u, content.frameCount)];
    const double wrapped = std::remainder(actual - expected, 2.0 * std::numbers::pi);
    CHECK(std::abs(wrapped) < 0.2);

    const auto audio = sound_mind::codec::decode(content);
    double energy = 0.0;
    for (const float sample : audio.left) {
        REQUIRE(std::isfinite(sample));
        energy += static_cast<double>(sample) * sample;
    }
    CHECK(energy > 0.0);
}

namespace {

/// @brief The amplitude-weighted mean bin of each frame, from frame 20 on.
std::vector<double> centroids(const sound_mind::codec::StreamImage& image) {
    std::vector<double> result;
    for (std::uint32_t frame = 20; frame + 20 < image.frameCount; ++frame) {
        double weighted = 0.0;
        double total = 0.0;
        for (std::uint32_t bin = 0; bin < image.config.binCount; ++bin) {
            const double amplitude =
                std::pow(10.0, image.leftMagnitudeDb[sound_mind::core::cellIndex(bin, frame, image.frameCount)] / 20.0);
            weighted += amplitude * bin;
            total += amplitude;
        }
        result.push_back(weighted / total);
    }
    return result;
}

double meanChange(const std::vector<double>& values) {
    double sum = 0.0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        sum += std::abs(values[i] - values[i - 1]);
    }
    return sum / static_cast<double>(values.size() - 1);
}

GeneratorConfiguration streamingConfig(double orderChaos, std::uint64_t seed = 5) {
    GeneratorConfiguration config;
    config.family = GeneratorFamily::Streaming;
    config.orderChaos = orderChaos;
    config.seed = seed;
    return config;
}

}  // namespace

TEST_CASE("The Streaming generator produces audible, seeded, decodable content", "[core][generator][streaming]") {
    const auto settings = fractalSettings();
    const auto content = generateContent(streamingConfig(0.0), settings);

    CHECK(*std::max_element(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end()) > -40.0f);
    CHECK(generateContent(streamingConfig(0.0), settings).leftMagnitudeDb == content.leftMagnitudeDb);
    CHECK(generateContent(streamingConfig(0.0, 6), settings).leftMagnitudeDb != content.leftMagnitudeDb);
    const auto audio = sound_mind::codec::decode(content);
    for (const float sample : audio.left) {
        REQUIRE(std::isfinite(sample));
    }
}

TEST_CASE("Toward order the Streaming signal settles; toward chaos it keeps moving", "[core][generator][streaming]") {
    const auto settings = fractalSettings();
    double ordered = 0.0;
    double chaotic = 0.0;
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        ordered += meanChange(centroids(generateContent(streamingConfig(1.0, seed), settings)));
        chaotic += meanChange(centroids(generateContent(streamingConfig(-1.0, seed), settings)));
    }
    CHECK(ordered * 5.0 < chaotic);
}
