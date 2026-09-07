#include "sound_mind/core/compositor.h"

#include "sound_mind/codec/color_mapping.h"

namespace sound_mind::core {

std::optional<sound_mind::codec::RgbImage> renderLayer(const Layer& layer) {
    if (!layer.content().has_value()) {
        return std::nullopt;
    }
    return sound_mind::codec::toRgbImage(*layer.content());
}

}  // namespace sound_mind::core
