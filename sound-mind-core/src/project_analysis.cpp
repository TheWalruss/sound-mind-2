#include "sound_mind/core/project_analysis.h"

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

namespace {

/// @brief The mix of `project` (respecting mute), decoded and measured; an
/// empty measurement if nothing is audible. `composite` receives the
/// composited image when there is one.
LoudnessMeasurement measureMix(const Project& project, const std::function<bool()>& shouldCancel,
                               std::optional<sound_mind::codec::StreamImage>* composite = nullptr) {
    auto image = compositeProject(project, shouldCancel, /*respectMute=*/true);
    if (!image.has_value()) {
        return {};
    }
    const LoudnessMeasurement measurement = measureLoudness(sound_mind::codec::decode(*image), shouldCancel);
    if (composite != nullptr) {
        *composite = std::move(image);
    }
    return measurement;
}

}  // namespace

ProjectLoudnessReport analyzeProjectLoudness(const Project& project, const std::function<bool()>& shouldCancel) {
    ProjectLoudnessReport report;
    std::optional<sound_mind::codec::StreamImage> composite;
    report.mix = measureMix(project, shouldCancel, &composite);
    if (composite.has_value()) {
        report.mixBandLevels = bandLevelsOverTime(*composite);
        report.frameCount = composite->frameCount;
        report.secondsPerFrame =
            static_cast<double>(composite->config.hopLength) / static_cast<double>(composite->config.sampleRateHz);
    }

    for (const Layer& layer : project.layers()) {
        if (isFilterLayerType(layer.type()) || layer.type() == LayerType::Background || !layer.visible() ||
            layer.muted() || !layer.content().has_value()) {
            continue;
        }
        // Soloed: every other layer muted, so the composite is exactly this
        // layer's own contribution (opacity, gain, balance, timing).
        Project solo = project;
        for (Layer& other : solo.layers()) {
            other.setMuted(other.id() != layer.id());
        }
        report.layers.push_back(LayerLoudness{layer.id(), layer.name(), measureMix(solo, shouldCancel)});
    }
    return report;
}

}  // namespace sound_mind::core
