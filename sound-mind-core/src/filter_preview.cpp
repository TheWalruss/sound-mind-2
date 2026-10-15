#include "sound_mind/core/filter_preview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "sound_mind/core/compositor.h"
#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/filter_operation.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

namespace {

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::StreamImage;

/// @brief Frames `[startFrame, startFrame + frames)` of `image`, as a
///        standalone image `decode()` turns into exactly `frames` hops.
StreamImage cropFrames(const StreamImage& image, std::uint32_t startFrame, std::uint32_t frames) {
    StreamImage cropped;
    cropped.config = image.config;
    cropped.frameCount = frames;
    cropped.sampleCount = static_cast<std::uint64_t>(frames) * image.config.hopLength;
    const std::size_t cells = static_cast<std::size_t>(image.config.binCount) * frames;
    cropped.leftMagnitudeDb.resize(cells);
    cropped.rightMagnitudeDb.resize(cells);
    cropped.sharedPhaseRadians.resize(cells);
    for (std::uint32_t bin = 0; bin < image.config.binCount; ++bin) {
        const std::size_t from = static_cast<std::size_t>(bin) * image.frameCount + startFrame;
        const std::size_t to = static_cast<std::size_t>(bin) * frames;
        std::copy_n(image.leftMagnitudeDb.begin() + from, frames, cropped.leftMagnitudeDb.begin() + to);
        std::copy_n(image.rightMagnitudeDb.begin() + from, frames, cropped.rightMagnitudeDb.begin() + to);
        std::copy_n(image.sharedPhaseRadians.begin() + from, frames, cropped.sharedPhaseRadians.begin() + to);
    }
    return cropped;
}

/// @brief Linear fade-in and fade-out over `fadeSamples` at each end, both
///        reaching exactly zero at the outermost sample.
void fadeEnds(AudioBuffer& audio, std::size_t fadeSamples) {
    const std::size_t count = audio.frameCount();
    fadeSamples = std::min(fadeSamples, count / 2);
    for (std::size_t i = 0; i < fadeSamples; ++i) {
        const float gain = static_cast<float>(i) / static_cast<float>(fadeSamples);
        audio.left[i] *= gain;
        audio.right[i] *= gain;
        audio.left[count - 1 - i] *= gain;
        audio.right[count - 1 - i] *= gain;
    }
}

}  // namespace

std::pair<double, double> filterPreviewWindow(double playbackPositionSeconds,
                                              const std::optional<TimeFrequencyRect>& selection,
                                              double canvasSeconds) {
    canvasSeconds = std::max(canvasSeconds, 0.0);
    const double wanted = selection.has_value()
                              ? std::max(selection->endTimeSeconds - std::max(selection->startTimeSeconds, 0.0),
                                         kMinimumFilterPreviewSeconds)
                              : kDefaultFilterPreviewSeconds;
    const double length = std::min(wanted, canvasSeconds);
    const double start = std::clamp(selection.has_value() ? selection->startTimeSeconds : playbackPositionSeconds,
                                    0.0, canvasSeconds - length);
    return {start, start + length};
}

AudioBuffer renderFilterPreview(const Project& project, const FilterPreviewRequest& request,
                                const std::function<bool()>& shouldCancel) {
    const ProjectSettings& settings = project.settings();
    Project copy = project;
    const MindWaveResolver resolveMindWave = [&copy](MindWaveId id) -> const MindWave* {
        const auto* entry = copy.mindWaveById(id);
        return entry != nullptr ? &entry->wave : nullptr;
    };

    // See FilterPreviewRequest's docs for which target applies.
    bool applyToWholeMix = true;
    if (Layer* layer = request.filterLayer.has_value() ? copy.layerById(*request.filterLayer) : nullptr;
        layer != nullptr && isFilterLayerType(layer->type())) {
        layer->setFilterConfiguration(request.config);
        applyToWholeMix = false;
    } else if (request.selection.has_value()) {
        applyToWholeMix = false;
        Layer* target = copy.layerById(request.selection->layer);
        if (target != nullptr && target->content().has_value()) {
            const FilterOperation operation(OperationId{0}, request.selection->layer, request.selection->bounds,
                                            request.config, std::nullopt, request.selection->boundary);
            StreamImage content = *target->content();
            applyFilterOperation(operation, content, settings,
                                 resolveFilterParameterMindWaves(request.config, resolveMindWave));
            target->setContent(std::move(content));
        }
    }

    std::optional<StreamImage> composite = compositeProject(copy, shouldCancel, /*respectMute=*/true);
    if (!composite.has_value() || composite->frameCount == 0) {
        return {};
    }
    if (applyToWholeMix) {
        *composite = applyFilter(*composite, request.config, settings,
                                 resolveFilterParameterMindWaves(request.config, resolveMindWave));
    }

    // The length depends only on the window's duration, never on where it
    // starts, so every render of the same window can be stageAudio()'d.
    const double framesPerSecond =
        static_cast<double>(composite->config.sampleRateHz) / static_cast<double>(composite->config.hopLength);
    const auto wantedFrames = static_cast<std::uint32_t>(
        std::max(1L, std::lround((request.endSeconds - request.startSeconds) * framesPerSecond)));
    const std::uint32_t frames = std::min(wantedFrames, composite->frameCount);
    const auto startFrame = static_cast<std::uint32_t>(
        std::clamp<long>(std::lround(request.startSeconds * framesPerSecond), 0L,
                         static_cast<long>(composite->frameCount - frames)));

    AudioBuffer audio = sound_mind::codec::decode(cropFrames(*composite, startFrame, frames));
    audio.sampleRateHz = composite->config.sampleRateHz;
    const std::size_t samples = static_cast<std::size_t>(frames) * composite->config.hopLength;
    audio.left.resize(samples, 0.0f);
    audio.right.resize(samples, 0.0f);
    fadeEnds(audio, static_cast<std::size_t>(std::lround(kFilterPreviewFadeSeconds * audio.sampleRateHz)));
    return audio;
}

}  // namespace sound_mind::core
