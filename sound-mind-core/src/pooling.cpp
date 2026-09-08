#include "sound_mind/core/pooling.h"

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"

namespace sound_mind::core {

bool poolLayer(Layer& layer) {
    if (!layer.content().has_value()) {
        return false;
    }

    const sound_mind::codec::StreamCodecConfig config = layer.content()->config;

    const sound_mind::codec::AudioBuffer audio = sound_mind::codec::decode(*layer.content());
    sound_mind::codec::PoolImage poolImage = sound_mind::codec::poolEncode(audio, config);

    const sound_mind::codec::AudioBuffer pooledAudio = sound_mind::codec::poolDecode(poolImage);
    sound_mind::codec::StreamImage freshStreamContent = sound_mind::codec::encode(pooledAudio, config);

    layer.setPoolContent(std::move(poolImage));
    layer.setContent(std::move(freshStreamContent));
    return true;
}

}  // namespace sound_mind::core
