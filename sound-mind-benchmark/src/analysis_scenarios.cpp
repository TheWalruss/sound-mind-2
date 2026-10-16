#include "sound_mind/benchmark/scenarios.h"

#include <memory>
#include <string>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_analysis.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/stereo_analysis.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::StreamImage;
using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;

/// @brief The analyzed project's length - long enough that per-frame and
/// per-sample work dominates fixed setup.
constexpr double kProjectSeconds = 30.0;
/// @brief The single-stage measurements' length.
constexpr double kStageSeconds = 60.0;

ProjectSettings settingsFor(double seconds) {
    ProjectSettings settings;
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    settings.canvasWidth =
        static_cast<std::uint32_t>(seconds * config.sampleRateHz / static_cast<double>(config.hopLength));
    return settings;
}

std::shared_ptr<Project> projectWithLayers(int layerCount) {
    const ProjectSettings settings = settingsFor(kProjectSeconds);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    auto project = std::make_shared<Project>(Project::createNew(settings));
    for (int i = 0; i < layerCount; ++i) {
        Layer layer(0, "Layer " + std::to_string(i + 1), LayerType::Normal);
        layer.setContent(sound_mind::codec::encode(
            musicLikeAudio(kProjectSeconds, config.sampleRateHz, static_cast<unsigned>(i)), config));
        project->addLayer(std::move(layer));
    }
    return project;
}

}  // namespace

std::vector<BenchmarkCase> buildAnalysisScenarios() {
    std::vector<BenchmarkCase> cases;

    // The whole Analyze button: composite + decode the mix and every layer
    // soloed, then every measurement.
    for (const int layers : {1, 4, 8}) {
        auto project = projectWithLayers(layers);
        nlohmann::json parameters;
        parameters["layers"] = layers;
        parameters["seconds"] = kProjectSeconds;
        cases.push_back(BenchmarkCase{"analyzeProject,layers=" + std::to_string(layers), "analysis", parameters,
                                       /*gpuEligible=*/true, [project]() {
                                           const auto report = sound_mind::core::analyzeProjectLoudness(*project);
                                           static_cast<void>(report);
                                       }});
    }

    // Its stages on their own, at a fixed length.
    const ProjectSettings settings = settingsFor(kStageSeconds);
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    auto audio = std::make_shared<AudioBuffer>(musicLikeAudio(kStageSeconds, config.sampleRateHz, 7));
    auto image = std::make_shared<StreamImage>(sound_mind::codec::encode(*audio, config));
    nlohmann::json stage;
    stage["seconds"] = kStageSeconds;
    stage["frames"] = image->frameCount;
    cases.push_back(BenchmarkCase{"measureLoudness", "analysis", stage, false, [audio]() {
                                      static_cast<void>(sound_mind::core::measureLoudness(*audio));
                                  }});
    cases.push_back(BenchmarkCase{"measureStereo", "analysis", stage, false, [audio]() {
                                      static_cast<void>(sound_mind::core::measureStereo(*audio));
                                  }});
    cases.push_back(BenchmarkCase{"bandLevelsOverTime", "analysis", stage, false, [image]() {
                                      static_cast<void>(sound_mind::core::bandLevelsOverTime(*image));
                                  }});
    cases.push_back(BenchmarkCase{"bandStereoWidths", "analysis", stage, false, [image]() {
                                      static_cast<void>(sound_mind::core::bandStereoWidths(*image));
                                  }});
    for (const int voices : {1, 3}) {
        nlohmann::json parameters = stage;
        parameters["voices"] = voices;
        cases.push_back(BenchmarkCase{"detectPitches,voices=" + std::to_string(voices), "analysis", parameters,
                                       false, [image, voices]() {
                                           static_cast<void>(sound_mind::core::detectPitches(*image, voices));
                                       }});
    }
    return cases;
}

}  // namespace sound_mind::benchmark
