#include "sound_mind/benchmark/scenarios.h"

#include <array>
#include <memory>
#include <random>
#include <string>

#include "sound_mind/core/compositor.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::codec::StreamImage;
using sound_mind::core::BlendMode;
using sound_mind::core::compositeProject;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;

constexpr std::uint32_t kBinCount = 512;
constexpr std::uint32_t kCanvasWidth = 2000;

ProjectSettings makeSettings() {
    ProjectSettings settings;
    settings.binCount = kBinCount;
    settings.canvasHeight = kBinCount;
    settings.canvasWidth = kCanvasWidth;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 20000.0f;
    return settings;
}

/// @brief Deterministic pseudo-random content, seeded differently per
/// layer so compositing doesn't degenerate into "every layer is
/// identical" (which some blend modes - `Difference`, in particular -
/// would make trivially cheap/branch-predictable in a way real layered
/// content never is).
StreamImage randomLayerContent(const ProjectSettings& settings, unsigned seed) {
    StreamImage image;
    image.config.binCount = settings.binCount;
    image.config.minFrequencyHz = settings.minFrequencyHz;
    image.config.maxFrequencyHz = settings.maxFrequencyHz;
    image.frameCount = settings.canvasWidth;
    const auto cellCount = static_cast<std::size_t>(settings.binCount) * settings.canvasWidth;
    image.leftMagnitudeDb.resize(cellCount);
    image.rightMagnitudeDb.resize(cellCount);
    image.sharedPhaseRadians.assign(cellCount, 0.0f);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> db(-60.0f, 0.0f);
    for (std::size_t i = 0; i < cellCount; ++i) {
        image.leftMagnitudeDb[i] = db(rng);
        image.rightMagnitudeDb[i] = db(rng);
    }
    return image;
}

std::shared_ptr<Project> makeProjectWithLayers(int layerCount, BlendMode blendMode) {
    auto settings = makeSettings();
    auto project = std::make_shared<Project>(Project::createNew(settings));
    for (int i = 0; i < layerCount; ++i) {
        Layer layer(static_cast<LayerId>(i + 1), "Layer " + std::to_string(i + 1), LayerType::Normal);
        layer.setContent(randomLayerContent(settings, static_cast<unsigned>(i + 1)));
        layer.setBlendMode(blendMode);
        layer.setOpacity(0.8f);
        project->addLayer(std::move(layer));
    }
    return project;
}

BenchmarkCase makeCompositingCase(std::string name, int layerCount, BlendMode blendMode, bool gpuEligible) {
    auto project = makeProjectWithLayers(layerCount, blendMode);

    nlohmann::json parameters;
    parameters["layerCount"] = layerCount;
    parameters["blendMode"] = blendMode;

    return BenchmarkCase{std::move(name), "compositing", std::move(parameters), gpuEligible, [project]() {
                              const auto result = compositeProject(*project);
                              static_cast<void>(result);
                          }};
}

void appendLayerCountSweep(std::vector<BenchmarkCase>& cases) {
    for (const int layerCount : {1, 5, 20, 50}) {
        cases.push_back(makeCompositingCase("layerCount=" + std::to_string(layerCount), layerCount, BlendMode::Normal,
                                             /*gpuEligible=*/true));
    }
}

/// @brief Every `BlendMode`, at a fixed, moderate layer count - answers
/// "did the user choose an expensive blend mode", per
/// `docs/sound-mind-benchmarking.md`'s own "Scenario catalog".
void appendBlendModeSweep(std::vector<BenchmarkCase>& cases) {
    constexpr std::array<BlendMode, 7> kEveryBlendMode{{
        BlendMode::Normal, BlendMode::Overwrite, BlendMode::Multiply, BlendMode::Screen,
        BlendMode::Overlay, BlendMode::Difference, BlendMode::Add,
    }};
    constexpr int kLayerCount = 10;
    for (const BlendMode blendMode : kEveryBlendMode) {
        const nlohmann::json blendModeJson = blendMode;
        cases.push_back(makeCompositingCase("blendMode=" + blendModeJson.get<std::string>(), kLayerCount, blendMode,
                                             /*gpuEligible=*/true));
    }
}

}  // namespace

std::vector<BenchmarkCase> buildCompositingScenarios() {
    std::vector<BenchmarkCase> cases;
    appendLayerCountSweep(cases);
    appendBlendModeSweep(cases);
    return cases;
}

}  // namespace sound_mind::benchmark
