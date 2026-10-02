#include "sound_mind/benchmark/scenarios.h"

#include <array>
#include <memory>
#include <string>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/path.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::evaluateMindWaveField;
using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::SuperpositionBlendMode;
using sound_mind::core::TimeFrequencyPoint;

struct FieldSize {
    const char* label;
    std::uint32_t binCount;
    std::uint32_t canvasWidth;
};

constexpr FieldSize kMediumField{"medium", 512, 2000};
constexpr std::array<FieldSize, 3> kFieldSizeSweep{{
    {"small", 128, 500},
    {"medium", 512, 2000},
    {"large", 1024, 8000},
}};

StreamCodecConfig codecConfigFor(const FieldSize& size) {
    StreamCodecConfig config;
    config.binCount = size.binCount;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

/// @brief A simple captured-shape Path for `GeneratorType::Drawn` - that
/// generator's own field is flat/neutral with no captured shape at all,
/// which would measure nothing real.
Path makeDrawnShapePath() {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{0.0, 0.0};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode mid;
    mid.anchor = TimeFrequencyPoint{0.5, 1.0};
    mid.type = PathNodeType::Corner;
    path.addNode(mid);
    PathNode end;
    end.anchor = TimeFrequencyPoint{1.0, 0.0};
    end.type = PathNodeType::Corner;
    path.addNode(end);
    return path;
}

/// @param gpuEligible Whether this case's own generator type has a real
///        GPU-accelerated field path - `GeneratorType::Fractal` only, as
///        of `v0.1.6.4` (`evaluateMindWaveField()`'s own unwarped,
///        non-superposition-stacked fast path) - see
///        `MindWave::tryEvaluateFractalFieldOnGpu()`'s own docs. Every
///        other generator type stays CPU-only, so defaults to `false`.
BenchmarkCase makeMindWaveCase(std::string name, const MindWave& wave, const FieldSize& size,
                                bool gpuEligible = false) {
    auto waveCopy = std::make_shared<MindWave>(wave);
    auto config = std::make_shared<StreamCodecConfig>(codecConfigFor(size));
    const std::uint32_t canvasWidth = size.canvasWidth;

    nlohmann::json parameters;
    parameters["field"] = size.label;
    parameters["binCount"] = size.binCount;
    parameters["canvasWidth"] = size.canvasWidth;

    return BenchmarkCase{std::move(name), "mindWave", std::move(parameters), gpuEligible,
                          [waveCopy, config, canvasWidth]() {
                              const auto field = evaluateMindWaveField(*waveCopy, *config, canvasWidth);
                              static_cast<void>(field);
                          }};
}

void appendOneBaselineCasePerGeneratorType(std::vector<BenchmarkCase>& cases) {
    constexpr std::array<GeneratorType, 9> kEveryGeneratorType{{
        GeneratorType::Periodic,
        GeneratorType::Envelope,
        GeneratorType::SteppedNoise,
        GeneratorType::Spatial,
        GeneratorType::Fractal,
        GeneratorType::Drawn,
        GeneratorType::StepGrid,
        GeneratorType::Continuous,
        GeneratorType::Resonance,
    }};

    for (const GeneratorType type : kEveryGeneratorType) {
        MindWave wave;
        wave.setType(type);
        if (type == GeneratorType::Drawn) {
            wave.setDrawnPath(makeDrawnShapePath());
        } else if (type == GeneratorType::Resonance) {
            wave.setResonanceSpectrum(std::nullopt, {0.2f, 0.5f, 1.0f, 0.6f, 0.3f, 0.1f});
        }
        cases.push_back(makeMindWaveCase("baseline", wave, kMediumField, /*gpuEligible=*/type == GeneratorType::Fractal));
    }
}

/// @brief Field-size sweep for `Fractal` - the generator whose own cost
/// most obviously scales with per-cell iteration count (`fractalIterations()`),
/// making it the most interesting one to also sweep by output size.
void appendFractalFieldSizeSweep(std::vector<BenchmarkCase>& cases) {
    MindWave wave;
    wave.setType(GeneratorType::Fractal);
    wave.setFractalIterations(6);
    for (const FieldSize& size : kFieldSizeSweep) {
        cases.push_back(makeMindWaveCase("Fractal", wave, size, /*gpuEligible=*/true));
    }
}

/// @brief Superposition stack-depth sweep (1/2/4/8 stacked waves) -
/// answers "does combining several MindWaves meaningfully add up", per
/// `docs/sound-mind-benchmarking.md`'s own "Scenario catalog".
void appendSuperpositionStackDepthSweep(std::vector<BenchmarkCase>& cases) {
    for (const int depth : {1, 2, 4, 8}) {
        MindWave wave;
        wave.setType(GeneratorType::Periodic);
        std::vector<MindWave> stack;
        stack.reserve(static_cast<std::size_t>(depth) - 1);
        for (int i = 1; i < depth; ++i) {
            MindWave member;
            member.setType(GeneratorType::Fractal);
            member.setFractalIterations(4);
            stack.push_back(member);
        }
        wave.setSuperpositionStack(std::move(stack));
        wave.setSuperpositionBlendMode(SuperpositionBlendMode::Add);

        nlohmann::json parameters;
        parameters["stackDepth"] = depth;
        auto waveCopy = std::make_shared<MindWave>(wave);
        auto config = std::make_shared<StreamCodecConfig>(codecConfigFor(kMediumField));
        const std::uint32_t canvasWidth = kMediumField.canvasWidth;
        cases.push_back(BenchmarkCase{"Superposition stackDepth=" + std::to_string(depth), "mindWave", parameters,
                                       false, [waveCopy, config, canvasWidth]() {
                                           const auto field = evaluateMindWaveField(*waveCopy, *config, canvasWidth);
                                           static_cast<void>(field);
                                       }});
    }
}

}  // namespace

std::vector<BenchmarkCase> buildMindWaveScenarios() {
    std::vector<BenchmarkCase> cases;
    appendOneBaselineCasePerGeneratorType(cases);
    appendFractalFieldSizeSweep(cases);
    appendSuperpositionStackDepthSweep(cases);
    return cases;
}

}  // namespace sound_mind::benchmark
