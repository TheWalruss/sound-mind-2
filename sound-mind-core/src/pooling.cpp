#include "sound_mind/core/pooling.h"

#include "sound_mind/codec/audio_buffer.h"

namespace sound_mind::core {

PooledContent computePooledContent(const sound_mind::codec::StreamImage& content,
                                    const std::function<bool()>& shouldCancel) {
    const sound_mind::codec::StreamCodecConfig config = content.config;

    if (shouldCancel && shouldCancel()) {
        throw PoolCancelled{};
    }
    const sound_mind::codec::AudioBuffer audio = sound_mind::codec::decode(content);

    if (shouldCancel && shouldCancel()) {
        throw PoolCancelled{};
    }
    sound_mind::codec::PoolImage poolImage = sound_mind::codec::poolEncode(audio, config);

    if (shouldCancel && shouldCancel()) {
        throw PoolCancelled{};
    }
    const sound_mind::codec::AudioBuffer pooledAudio = sound_mind::codec::poolDecode(poolImage);

    if (shouldCancel && shouldCancel()) {
        throw PoolCancelled{};
    }
    sound_mind::codec::StreamImage freshStreamContent = sound_mind::codec::encode(pooledAudio, config);

    return PooledContent{std::move(poolImage), std::move(freshStreamContent)};
}

bool poolLayer(Layer& layer) {
    if (!layer.content().has_value()) {
        return false;
    }

    PooledContent result = computePooledContent(*layer.content());

    layer.setPoolContent(std::move(result.poolImage));
    layer.setContent(std::move(result.streamContent));
    return true;
}

}  // namespace sound_mind::core
