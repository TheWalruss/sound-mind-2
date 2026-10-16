#include "sound_mind/core/project_analysis.h"

#include "sound_mind/codec/parallel.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

namespace {

/// @brief The mix of `project` (respecting mute), decoded and measured; an
/// empty measurement if nothing is audible. `composite` and `audio` receive
/// the composited image and the decoded mix when there is one.
LoudnessMeasurement measureMix(const Project& project, const std::function<bool()>& shouldCancel,
                               std::optional<sound_mind::codec::StreamImage>* composite = nullptr,
                               std::shared_ptr<const sound_mind::codec::AudioBuffer>* audio = nullptr) {
    auto image = compositeProject(project, shouldCancel, /*respectMute=*/true);
    if (!image.has_value()) {
        return {};
    }
    auto decoded = std::make_shared<const sound_mind::codec::AudioBuffer>(sound_mind::codec::decode(*image));
    const LoudnessMeasurement measurement = measureLoudness(*decoded, shouldCancel);
    if (composite != nullptr) {
        *composite = std::move(image);
    }
    if (audio != nullptr) {
        *audio = std::move(decoded);
    }
    return measurement;
}

}  // namespace

ProjectLoudnessReport analyzeProjectLoudness(const Project& project, const std::function<bool()>& shouldCancel,
                                             int pitchVoices) {
    std::vector<const Layer*> audible;
    for (const Layer& layer : project.layers()) {
        if (isFilterLayerType(layer.type()) || layer.type() == LayerType::Background || !layer.visible() ||
            layer.muted() || !layer.content().has_value()) {
            continue;
        }
        audible.push_back(&layer);
    }

    // The mix (item 0) and each layer soloed (items 1..) are independent:
    // each composites, decodes and measures on its own, so they run as
    // parallel tasks (v0.1.41.2). Results land in their own slots, so the
    // report's order never depends on which finished first.
    ProjectLoudnessReport report;
    report.layers.resize(audible.size());
    sound_mind::codec::parallelFor(audible.size() + 1, 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t item = begin; item < end; ++item) {
            if (item == 0) {
                std::optional<sound_mind::codec::StreamImage> composite;
                report.mix = measureMix(project, shouldCancel, &composite, &report.mixAudio);
                if (report.mixAudio != nullptr) {
                    report.mixStereo = measureStereo(*report.mixAudio);
                }
                if (composite.has_value()) {
                    report.mixBandLevels = bandLevelsOverTime(*composite);
                    report.mixBandWidths = bandStereoWidths(*composite);
                    report.mixPitchTracks = detectPitches(*composite, pitchVoices);
                    report.frameCount = composite->frameCount;
                    report.secondsPerFrame = static_cast<double>(composite->config.hopLength) /
                                             static_cast<double>(composite->config.sampleRateHz);
                }
                continue;
            }
            const Layer& layer = *audible[item - 1];
            // Soloed: every other layer muted, so the composite is exactly
            // this layer's own contribution (opacity, gain, balance, timing).
            Project solo = project;
            for (Layer& other : solo.layers()) {
                other.setMuted(other.id() != layer.id());
            }
            report.layers[item - 1] = LayerLoudness{layer.id(), layer.name(), measureMix(solo, shouldCancel)};
        }
    });
    return report;
}

}  // namespace sound_mind::core
