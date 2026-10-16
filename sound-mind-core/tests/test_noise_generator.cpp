#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>

#include "sound_mind/core/generator.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::GeneratorConfiguration;
using sound_mind::core::GeneratorFamily;
using sound_mind::core::generateContent;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::NoiseColor;
using sound_mind::core::ProjectSettings;

namespace {

ProjectSettings noiseSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 600;
    settings.binCount = 64;
    settings.minFrequencyHz = 50.0f;
    settings.maxFrequencyHz = 12800.0f;  // 8 octaves -> 8 bins per octave.
    return settings;
}

GeneratorConfiguration noiseConfig(NoiseColor color) {
    GeneratorConfiguration config;
    config.family = GeneratorFamily::Noise;
    config.seed = 7;
    config.noise.color = color;
    config.noise.levelDb = -30.0f;
    return config;
}

/// @brief The mean stored dB in `bin` across every frame, minus the codec's
/// A-weighting there - the noise's own spectral shape.
double unweightedMeanDb(const sound_mind::codec::StreamImage& image, std::uint32_t bin) {
    double sum = 0.0;
    for (std::uint32_t frame = 0; frame < image.frameCount; ++frame) {
        sum += image.leftMagnitudeDb[sound_mind::core::cellIndex(bin, frame, image.frameCount)];
    }
    const float frequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(bin), image.config);
    return sum / image.frameCount - sound_mind::codec::aWeightingOffsetDb(frequencyHz);
}

double storedMeanDb(const sound_mind::codec::StreamImage& image, std::uint32_t bin) {
    const float frequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(bin), image.config);
    return unweightedMeanDb(image, bin) + sound_mind::codec::aWeightingOffsetDb(frequencyHz);
}

/// @brief A MindWave reading a constant `value` (0 or 1) everywhere.
MindWave constantWave(bool one) {
    MindWave wave;
    wave.setType(sound_mind::core::GeneratorType::Periodic);
    wave.setPeriodicWaveform(sound_mind::core::PeriodicWaveform::Pulse);
    wave.setDutyCycle(one ? 1.0 : 0.0);
    return wave;
}

}  // namespace

TEST_CASE("White noise is flat and pink falls 3 dB per octave, once A-weighting is removed", "[core][noise]") {
    const auto settings = noiseSettings();
    const auto white = generateContent(noiseConfig(NoiseColor::White), settings);
    CHECK(unweightedMeanDb(white, 16) == Catch::Approx(unweightedMeanDb(white, 48)).margin(1.0));

    const auto pink = generateContent(noiseConfig(NoiseColor::Pink), settings);
    // Bins 16 and 48 are four octaves apart: -3.01 dB each.
    CHECK(unweightedMeanDb(pink, 48) - unweightedMeanDb(pink, 16) == Catch::Approx(-12.04).margin(1.2));

    const auto red = generateContent(noiseConfig(NoiseColor::Red), settings);
    CHECK(unweightedMeanDb(red, 48) - unweightedMeanDb(red, 16) == Catch::Approx(-24.08).margin(1.2));
    const auto purple = generateContent(noiseConfig(NoiseColor::Purple), settings);
    CHECK(unweightedMeanDb(purple, 48) - unweightedMeanDb(purple, 16) == Catch::Approx(24.08).margin(1.2));
}

TEST_CASE("Gray noise reads flat on the A-weighted canvas", "[core][noise]") {
    const auto gray = generateContent(noiseConfig(NoiseColor::Gray), noiseSettings());
    CHECK(storedMeanDb(gray, 4) == Catch::Approx(storedMeanDb(gray, 60)).margin(1.0));
    // Rayleigh amplitudes with unit mean power average about -2.5 dB.
    CHECK(storedMeanDb(gray, 30) == Catch::Approx(-30.0 - 2.5).margin(1.0));
}

TEST_CASE("Noise is deterministic from its seed", "[core][noise]") {
    const auto settings = noiseSettings();
    auto config = noiseConfig(NoiseColor::Pink);
    const auto first = generateContent(config, settings);
    const auto again = generateContent(config, settings);
    CHECK(first.leftMagnitudeDb == again.leftMagnitudeDb);
    CHECK(first.sharedPhaseRadians == again.sharedPhaseRadians);
    CHECK(first.leftMagnitudeDb != first.rightMagnitudeDb);  // the channels are independent.

    config.seed = 8;
    CHECK(generateContent(config, settings).leftMagnitudeDb != first.leftMagnitudeDb);
}

TEST_CASE("Noise resolution makes cells span several frames and bins", "[core][noise]") {
    const auto settings = noiseSettings();
    auto config = noiseConfig(NoiseColor::Gray);
    config.noise.horizontalResolution = 4.0;
    config.noise.verticalResolution = 2.0;
    const auto image = generateContent(config, settings);
    const auto at = [&](std::uint32_t bin, std::uint32_t frame) {
        return image.leftMagnitudeDb[sound_mind::core::cellIndex(bin, frame, image.frameCount)];
    };
    CHECK(at(10, 8) == at(10, 11));  // frames 8-11 share a cell...
    CHECK(at(10, 8) == at(11, 8));   // ...as do bins 10-11.
    CHECK(at(10, 8) != at(10, 12));  // the next cell is a fresh draw.
}

TEST_CASE("Custom noise takes each channel's level from the spectrum gradient", "[core][noise]") {
    auto config = noiseConfig(NoiseColor::Custom);
    auto stop = config.noise.customSpectrum.stops().front();
    stop.leftIntensity = -20.0f;
    stop.rightIntensity = -50.0f;
    config.noise.customSpectrum.setStopValues(0, stop);
    config.noise.customSpectrum.setStopValues(1, stop);
    const auto image = generateContent(config, noiseSettings());

    CHECK(storedMeanDb(image, 32) == Catch::Approx(-22.5).margin(1.0));
    double right = 0.0;
    for (std::uint32_t frame = 0; frame < image.frameCount; ++frame) {
        right += image.rightMagnitudeDb[sound_mind::core::cellIndex(32u, frame, image.frameCount)];
    }
    CHECK(right / image.frameCount == Catch::Approx(-52.5).margin(1.0));
}

TEST_CASE("MindWave bindings blend noise parameters from their baseline", "[core][noise]") {
    const auto settings = noiseSettings();
    const MindWave zero = constantWave(false);
    const MindWave one = constantWave(true);
    const auto resolve = [&](MindWaveId id) -> const MindWave* { return id == 1 ? &zero : &one; };

    // A spectrum bound to a MindWave reading 0 everywhere is silent...
    auto custom = noiseConfig(NoiseColor::Custom);
    auto stop = custom.noise.customSpectrum.stops().front();
    stop.leftIntensity = -20.0f;
    stop.rightIntensity = -20.0f;
    custom.noise.customSpectrum.setStopValues(0, stop);
    custom.noise.customSpectrum.setStopValues(1, stop);
    custom.noise.customSpectrumMindWave = MindWaveId{1};
    const auto silent = generateContent(custom, settings, resolve);
    CHECK(storedMeanDb(silent, 32) == Catch::Approx(-96.0).margin(0.5));
    // ...and one reading 1 leaves it at the gradient's level.
    custom.noise.customSpectrumMindWave = MindWaveId{2};
    CHECK(storedMeanDb(generateContent(custom, settings, resolve), 32) == Catch::Approx(-22.5).margin(1.0));

    // A resolution bound to 0 falls back to one cell, whatever its value.
    auto coarse = noiseConfig(NoiseColor::Gray);
    coarse.noise.horizontalResolution = 8.0;
    coarse.noise.horizontalResolutionMindWave = MindWaveId{1};
    const auto fine = generateContent(coarse, settings, resolve);
    const auto at = [&](std::uint32_t frame) {
        return fine.leftMagnitudeDb[sound_mind::core::cellIndex(10u, frame, fine.frameCount)];
    };
    CHECK(at(16) != at(17));
}

namespace {

MindWave noiseWave(NoiseColor color, std::uint32_t seed = 3) {
    MindWave wave;
    wave.setType(sound_mind::core::GeneratorType::Noise);
    wave.setNoiseColor(color);
    wave.setNoiseCellWidth(0.01);
    wave.setNoiseCellHeight(1.0);
    wave.setSeed(seed);
    return wave;
}

/// @brief The mean absolute change between neighboring cells along time.
double meanStep(const MindWave& wave, const sound_mind::codec::StreamCodecConfig& config) {
    double sum = 0.0;
    double previous = wave.evaluate({0.005, 1000.0}, config);
    for (int cell = 1; cell < 2000; ++cell) {
        const double value = wave.evaluate({0.01 * cell + 0.005, 1000.0}, config);
        sum += std::abs(value - previous);
        previous = value;
    }
    return sum / 1999.0;
}

}  // namespace

TEST_CASE("A noise MindWave holds one value per cell and is deterministic", "[core][noise][mind_wave]") {
    const auto config = sound_mind::core::streamCodecConfigFor(noiseSettings());
    MindWave wave = noiseWave(NoiseColor::White);
    wave.setNoiseCellWidth(0.1);

    const float a = wave.evaluate({0.12, 1000.0}, config);
    CHECK(a == wave.evaluate({0.18, 1000.0}, config));  // the same 0.1 s cell.
    CHECK(a != wave.evaluate({0.22, 1000.0}, config));  // the next one.

    MindWave same = wave;
    CHECK(same.evaluate({0.12, 1000.0}, config) == a);
    same.setSeed(99);
    CHECK(same.evaluate({0.12, 1000.0}, config) != a);
    for (int cell = 0; cell < 200; ++cell) {
        const float value = wave.evaluate({0.1 * cell, 500.0}, config);
        REQUIRE(value >= 0.0f);
        REQUIRE(value <= 1.0f);
    }
}

TEST_CASE("A noise MindWave's color sets how smoothly it wanders", "[core][noise][mind_wave]") {
    const auto config = sound_mind::core::streamCodecConfigFor(noiseSettings());
    const double white = meanStep(noiseWave(NoiseColor::White), config);
    const double pink = meanStep(noiseWave(NoiseColor::Pink), config);
    const double red = meanStep(noiseWave(NoiseColor::Red), config);
    const double purple = meanStep(noiseWave(NoiseColor::Purple), config);
    CHECK(red < pink);
    CHECK(pink < white);
    // Purple is anti-correlated: each value tends to swing back from the last.
    const MindWave violet = noiseWave(NoiseColor::Purple);
    double lagProduct = 0.0;
    double previous = violet.evaluate({0.005, 1000.0}, config) - 0.5;
    for (int cell = 1; cell < 2000; ++cell) {
        const double value = violet.evaluate({0.01 * cell + 0.005, 1000.0}, config) - 0.5;
        lagProduct += value * previous;
        previous = value;
    }
    CHECK(lagProduct < 0.0);
    CHECK(purple > 0.0);
    MindWave customRed = noiseWave(NoiseColor::Custom);
    customRed.setNoiseSlope(-2.0);
    CHECK(meanStep(customRed, config) == Catch::Approx(red));
}

TEST_CASE("A noise MindWave's settings round-trip through JSON", "[core][noise][mind_wave]") {
    MindWave wave = noiseWave(NoiseColor::Custom, 11);
    wave.setNoiseSlope(0.5);
    wave.setNoiseCellWidth(0.25);
    wave.setNoiseCellHeight(3.0);

    const nlohmann::json json = wave;
    const MindWave restored = json.get<MindWave>();

    CHECK(restored.type() == sound_mind::core::GeneratorType::Noise);
    CHECK(restored.noiseColor() == NoiseColor::Custom);
    CHECK(restored.noiseSlope() == 0.5);
    CHECK(restored.noiseCellWidth() == 0.25);
    CHECK(restored.noiseCellHeight() == 3.0);
    CHECK(restored.seed() == 11u);
}
