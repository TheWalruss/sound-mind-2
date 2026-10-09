#include "sound_mind/core/mind_shot_preview.h"

namespace sound_mind::core {

sound_mind::codec::StreamImage streamImageFromClip(const Clip& clip,
                                                     const sound_mind::codec::StreamCodecConfig& config) {
    sound_mind::codec::StreamImage image;
    image.config = config;
    image.config.binCount = clip.binCount;
    image.frameCount = clip.frameCount;
    image.sampleCount = static_cast<std::uint64_t>(clip.frameCount) * config.hopLength;
    image.leftMagnitudeDb = clip.leftMagnitudeDb;
    image.rightMagnitudeDb = clip.rightMagnitudeDb;
    image.sharedPhaseRadians = clip.sharedPhaseRadians;
    return image;
}

}  // namespace sound_mind::core
