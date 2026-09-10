#include "sound_mind/core/layer_export.h"

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"

namespace sound_mind::core {

std::optional<sound_mind::codec::AudioBuffer> decodeLayerForExport(const Layer& layer) {
    if (layer.poolContent().has_value()) {
        return sound_mind::codec::poolDecode(*layer.poolContent());
    }
    if (layer.content().has_value()) {
        return sound_mind::codec::decode(*layer.content());
    }
    return std::nullopt;
}

bool exportLayerAudio(const Layer& layer, const std::filesystem::path& path,
                       sound_mind::codec::CompressedAudioFormat format) {
    const auto audio = decodeLayerForExport(layer);
    if (!audio.has_value()) {
        return false;
    }
    sound_mind::codec::exportCompressedAudio(path, *audio, format);
    return true;
}

bool exportLayerVideo(const Layer& layer, const std::filesystem::path& path, std::uint32_t canvasWidth,
                       int frameRate) {
    const auto audio = decodeLayerForExport(layer);
    if (!audio.has_value()) {
        return false;
    }
    const auto canvas = renderLayer(layer, canvasWidth);
    if (!canvas.has_value()) {
        // decodeLayerForExport() found content but renderLayer() didn't -
        // shouldn't happen given both read from the same layer state, but
        // treated as "nothing to export" rather than an assertion, matching
        // this function's own "false means nothing to export" contract.
        return false;
    }
    sound_mind::codec::exportVideo(path, *canvas, *audio, frameRate);
    return true;
}

}  // namespace sound_mind::core
