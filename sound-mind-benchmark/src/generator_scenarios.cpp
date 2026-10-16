#include "sound_mind/benchmark/scenarios.h"

#include <memory>
#include <string>

#include "sound_mind/core/generator.h"
#include "sound_mind/core/generator_configuration.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::core::GeneratorConfiguration;
using sound_mind::core::GeneratorFamily;
using sound_mind::core::NoiseColor;
using sound_mind::core::ProjectSettings;

/// @brief Canvas widths swept: 30 s and 120 s at the default 10 ms hop.
constexpr std::uint32_t kCanvasWidths[] = {3000, 12000};

}  // namespace

std::vector<BenchmarkCase> buildGeneratorScenarios() {
    std::vector<BenchmarkCase> cases;
    for (const std::uint32_t width : kCanvasWidths) {
        auto settings = std::make_shared<ProjectSettings>();
        settings->canvasWidth = width;

        // A noise Generate Layer: one hashed random amplitude and phase per
        // cell, shaped by the color's spectrum.
        for (const NoiseColor color : {NoiseColor::White, NoiseColor::Pink}) {
            auto config = std::make_shared<GeneratorConfiguration>();
            config->family = GeneratorFamily::Noise;
            config->noise.color = color;
            config->seed = 42;
            nlohmann::json parameters;
            parameters["canvasWidth"] = width;
            parameters["binCount"] = settings->binCount;
            parameters["color"] = color == NoiseColor::White ? "white" : "pink";
            cases.push_back(BenchmarkCase{
                std::string("noiseLayer,") + (color == NoiseColor::White ? "white" : "pink") +
                    ",canvasWidth=" + std::to_string(width),
                "generator", parameters, /*gpuEligible=*/false, [config, settings]() {
                    static_cast<void>(sound_mind::core::generateContent(*config, *settings));
                }});
        }

        // The other families, for comparison.
        for (const GeneratorFamily family : {GeneratorFamily::Fractal, GeneratorFamily::Streaming}) {
            auto config = std::make_shared<GeneratorConfiguration>();
            config->family = family;
            config->seed = 42;
            const std::string name = family == GeneratorFamily::Fractal ? "fractal" : "streaming";
            nlohmann::json parameters;
            parameters["canvasWidth"] = width;
            parameters["family"] = name;
            cases.push_back(BenchmarkCase{name + "Layer,canvasWidth=" + std::to_string(width), "generator",
                                           parameters, /*gpuEligible=*/false, [config, settings]() {
                                               static_cast<void>(
                                                   sound_mind::core::generateContent(*config, *settings));
                                           }});
        }

        // A noise MindWave evaluated over the whole canvas - what a bound
        // layer or parameter costs on every recomposite.
        auto wave = std::make_shared<sound_mind::core::MindWave>();
        wave->setType(sound_mind::core::GeneratorType::Noise);
        wave->setNoiseColor(NoiseColor::Pink);
        wave->setNoiseCellWidth(0.05);
        wave->setNoiseCellHeight(4.0);
        const auto codecConfig = sound_mind::core::streamCodecConfigFor(*settings);
        nlohmann::json parameters;
        parameters["canvasWidth"] = width;
        cases.push_back(BenchmarkCase{"noiseMindWaveField,canvasWidth=" + std::to_string(width), "generator",
                                       parameters, /*gpuEligible=*/false, [wave, codecConfig, width]() {
                                           static_cast<void>(
                                               sound_mind::core::evaluateMindWaveField(*wave, codecConfig, width));
                                       }});
    }
    return cases;
}

}  // namespace sound_mind::benchmark
