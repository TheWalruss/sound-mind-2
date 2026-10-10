#include "sound_mind/core/tool_configuration_preview.h"

#include <cmath>
#include <cstdint>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/path.h"

namespace sound_mind::core {

sound_mind::codec::StreamImage toolConfigurationPreviewStreamImage(const ToolConfiguration& config,
                                                                      const Project& project) {
    const auto codecConfig = streamCodecConfigFor(project.settings());
    const double scale = frequencyToTimeScaleFor(project.settings());

    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{0.0, 3000.0};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{3.0, 5000.0};
    end.type = PathNodeType::Corner;
    path.addNode(end);
    GradientStop stop = path.gradient().stops().front();
    stop.leftIntensity = 0.0f;
    stop.rightIntensity = 0.0f;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    path.gradient().setStopValues(0, stop);
    path.gradient().setStopValues(1, stop);

    const auto frameCount =
        static_cast<std::uint32_t>(std::lround(3.0 * codecConfig.sampleRateHz / codecConfig.hopLength));
    sound_mind::codec::StreamImage content;
    content.config = codecConfig;
    content.frameCount = frameCount;
    content.sampleCount = static_cast<std::uint64_t>(frameCount) * codecConfig.hopLength;
    // -96 dB, this codebase's established silence floor (see Project::
    // createNew()'s own Equalizer default) - a quiet, not literally
    // empty, backdrop the stroke itself stands out starkly against.
    content.leftMagnitudeDb.assign(std::size_t{codecConfig.binCount} * frameCount, -96.0f);
    content.rightMagnitudeDb.assign(std::size_t{codecConfig.binCount} * frameCount, -96.0f);
    content.sharedPhaseRadians.assign(std::size_t{codecConfig.binCount} * frameCount, 0.0f);

    const PaintOperation op(OperationId{0}, LayerId{0}, path, config.clone());
    const MindWaveResolver resolveMindWave = [&project](MindWaveId mindWaveId) -> const MindWave* {
        const auto* entry = project.mindWaveById(mindWaveId);
        return entry != nullptr ? &entry->wave : nullptr;
    };
    applyPaintOperation(op, scale, content, /*resolveLayerContent=*/{}, resolveMindWave);
    return content;
}

}  // namespace sound_mind::core
