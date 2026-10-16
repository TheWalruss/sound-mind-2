#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/project_analysis.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;

namespace {

ProjectSettings analysisSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 300;
    return settings;
}

LayerId addTone(Project& project, double frequencyHz, double amplitude, const std::string& name) {
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = config.sampleRateHz;
    const auto count = static_cast<std::size_t>(project.settings().canvasWidth) * config.hopLength;
    audio.left.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        audio.left[i] = static_cast<float>(
            amplitude * std::sin(2.0 * std::numbers::pi * frequencyHz * static_cast<double>(i) / config.sampleRateHz));
    }
    audio.right = audio.left;
    const LayerId id = project.addLayer(Layer(0, name, LayerType::Normal));
    project.layerById(id)->setContent(sound_mind::codec::encode(audio, config));
    return id;
}

}  // namespace

TEST_CASE("analyzeProjectLoudness measures the mix and each layer on its own", "[core][project_analysis]") {
    Project project = Project::createNew(analysisSettings());
    const LayerId loud = addTone(project, 1000.0, 0.4, "Loud");
    const LayerId quiet = addTone(project, 500.0, 0.04, "Quiet");

    const auto report = sound_mind::core::analyzeProjectLoudness(project);

    REQUIRE(report.layers.size() == 2);
    CHECK(report.layers[0].layer == loud);
    CHECK(report.layers[0].name == "Loud");
    CHECK(report.layers[1].layer == quiet);
    // The quiet layer is 20 dB down (K-weighting barely differs at 500 Hz).
    CHECK(report.layers[0].measurement.integratedLufs - report.layers[1].measurement.integratedLufs ==
          Catch::Approx(20.0).margin(1.0));
    // The mix is the loud layer, barely raised by the quiet one.
    CHECK(report.mix.integratedLufs == Catch::Approx(report.layers[0].measurement.integratedLufs).margin(0.5));
    CHECK(report.frameCount == 300);
    CHECK(report.mixBandLevels.size() == sound_mind::core::loudnessBands().size() * 300);
    CHECK(report.secondsPerFrame > 0.0);
    // Both tones are identical in each channel: a mono mix.
    REQUIRE(report.mixAudio != nullptr);
    CHECK(report.mixAudio->frameCount() > 0);
    CHECK(report.mixStereo.correlation == Catch::Approx(1.0).margin(0.01));
    CHECK(report.mixBandWidths.size() == sound_mind::core::loudnessBands().size());
}

TEST_CASE("analyzeProjectLoudness leaves out muted, hidden and filter layers", "[core][project_analysis]") {
    Project project = Project::createNew(analysisSettings());
    addTone(project, 1000.0, 0.4, "Heard");
    const LayerId muted = addTone(project, 1000.0, 0.4, "Muted");
    project.layerById(muted)->setMuted(true);
    const LayerId hidden = addTone(project, 1000.0, 0.4, "Hidden");
    project.layerById(hidden)->setVisible(false);
    project.addLayer(Layer(0, "Filter", LayerType::Filter));

    const auto report = sound_mind::core::analyzeProjectLoudness(project);

    REQUIRE(report.layers.size() == 1);
    CHECK(report.layers[0].name == "Heard");
    // The mix doesn't hear the muted or hidden layers either.
    CHECK(report.mix.integratedLufs == Catch::Approx(report.layers[0].measurement.integratedLufs).margin(0.3));
}

TEST_CASE("analyzeProjectLoudness reports nothing audible for an empty project", "[core][project_analysis]") {
    const Project project = Project::createNew(analysisSettings());
    const auto report = sound_mind::core::analyzeProjectLoudness(project);
    CHECK(report.layers.empty());
    CHECK(report.mix.integratedLufs == sound_mind::core::kLoudnessFloorLufs);
    CHECK(report.mixAudio == nullptr);
}
