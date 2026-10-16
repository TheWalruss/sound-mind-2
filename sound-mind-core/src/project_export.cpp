#include "sound_mind/core/project_export.h"

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"

namespace sound_mind::core {

std::optional<ProjectExportMedia> renderProjectForExport(const Project& project,
                                                         const std::function<bool()>& shouldCancel) {
    const auto composite = compositeProject(project, shouldCancel, /*respectMute=*/true);
    if (!composite.has_value()) {
        return std::nullopt;
    }
    return ProjectExportMedia{sound_mind::codec::decode(*composite), sound_mind::codec::toRgbImage(*composite)};
}

bool exportProjectAudio(const Project& project, const std::filesystem::path& path,
                        sound_mind::codec::CompressedAudioFormat format, const std::function<bool()>& shouldCancel,
                        std::optional<float> limiterCeilingDb) {
    auto media = renderProjectForExport(project, shouldCancel);
    if (!media.has_value()) {
        return false;
    }
    if (limiterCeilingDb.has_value()) {
        applyMasterLimiter(media->audio, *limiterCeilingDb);
    }
    sound_mind::codec::exportCompressedAudio(path, media->audio, format, shouldCancel);
    return true;
}

bool exportProjectVideo(const Project& project, const std::filesystem::path& path, int frameRate,
                        const std::function<bool()>& shouldCancel, std::optional<float> limiterCeilingDb) {
    auto media = renderProjectForExport(project, shouldCancel);
    if (!media.has_value()) {
        return false;
    }
    if (limiterCeilingDb.has_value()) {
        applyMasterLimiter(media->audio, *limiterCeilingDb);
    }
    sound_mind::codec::exportVideo(path, media->canvas, media->audio, frameRate, shouldCancel);
    return true;
}

}  // namespace sound_mind::core
