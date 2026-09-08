#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/pooling.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::encode;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::Layer;
using sound_mind::core::LayerType;
using sound_mind::core::poolLayer;

namespace {

AudioBuffer makeSineTone(float frequencyHz, float durationSeconds, std::uint32_t sampleRateHz) {
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto sampleCount = static_cast<std::size_t>(durationSeconds * static_cast<float>(sampleRateHz));
    audio.left.resize(sampleCount);
    audio.right.resize(sampleCount);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float sample =
            std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / static_cast<float>(sampleRateHz));
        audio.left[i] = sample;
        audio.right[i] = sample;
    }
    return audio;
}

}  // namespace

TEST_CASE("poolLayer does nothing to a layer with no content", "[core][pooling]") {
    Layer layer(1, "Untitled", LayerType::Normal);
    CHECK_FALSE(poolLayer(layer));
    CHECK_FALSE(layer.poolContent().has_value());
}

TEST_CASE("poolLayer pools a layer's content", "[core][pooling]") {
    Layer layer(1, "Imported", LayerType::Normal);
    layer.setContent(encode(makeSineTone(1000.0f, 0.5f, 44100), StreamCodecConfig{}));

    const bool pooled = poolLayer(layer);

    CHECK(pooled);
    REQUIRE(layer.poolContent().has_value());
    CHECK(layer.poolContent()->config.sampleRateHz == 44100);
}

TEST_CASE("poolLayer re-derives the layer's Stream content from the pooled result", "[core][pooling]") {
    // Per the design doc's "resulting Pool file converted to a light-weight
    // Stream copy" - the layer keeps showing/playing via its (now
    // Pool-derived) Stream content, not the Pool data directly.
    Layer layer(1, "Imported", LayerType::Normal);
    layer.setContent(encode(makeSineTone(1000.0f, 0.5f, 44100), StreamCodecConfig{}));
    const auto originalFrameCount = layer.content()->frameCount;

    poolLayer(layer);

    REQUIRE(layer.content().has_value());
    CHECK(layer.content()->frameCount == originalFrameCount);
    CHECK(layer.content()->sampleCount == layer.poolContent()->sampleCount);
}
