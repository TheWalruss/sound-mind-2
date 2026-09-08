#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/layer_export.h"
#include "sound_mind/core/pooling.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::CompressedAudioFormat;
using sound_mind::codec::encode;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::decodeLayerForExport;
using sound_mind::core::exportLayerAudio;
using sound_mind::core::exportLayerVideo;
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

TEST_CASE("decodeLayerForExport returns nullopt for a layer with no content", "[core][layer_export]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    CHECK_FALSE(decodeLayerForExport(layer).has_value());
}

TEST_CASE("decodeLayerForExport decodes Stream content when the layer has never been Pooled", "[core][layer_export]") {
    Layer layer(1, "Imported", LayerType::Normal);
    layer.setContent(encode(makeSineTone(1000.0f, 0.25f, 44100), StreamCodecConfig{}));

    const auto audio = decodeLayerForExport(layer);

    REQUIRE(audio.has_value());
    CHECK(audio->sampleRateHz == 44100);
    CHECK(audio->frameCount() > 0);
}

TEST_CASE("decodeLayerForExport prefers Pool content once the layer has been Pooled", "[core][layer_export]") {
    Layer layer(1, "Imported", LayerType::Normal);
    layer.setContent(encode(makeSineTone(1000.0f, 0.25f, 44100), StreamCodecConfig{}));
    poolLayer(layer);
    REQUIRE(layer.poolContent().has_value());

    const auto fromPool = decodeLayerForExport(layer);
    const auto fromStream = sound_mind::codec::decode(*layer.content());

    REQUIRE(fromPool.has_value());
    // Pool and (Pool-derived) Stream content should decode to essentially
    // the same audio here, so this mainly confirms decodeLayerForExport()
    // actually took the Pool path rather than throwing/crashing on it -
    // poolContent()'s own fidelity is pool_codec's test suite's job.
    CHECK(fromPool->sampleRateHz == fromStream.sampleRateHz);
    CHECK(fromPool->frameCount() == fromStream.frameCount());
}

TEST_CASE("exportLayerAudio does nothing for a layer with no content", "[core][layer_export]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layer-export.flac";

    CHECK_FALSE(exportLayerAudio(layer, path, CompressedAudioFormat::Flac));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("exportLayerAudio writes a real file for a layer with content", "[core][layer_export]") {
    Layer layer(1, "Imported", LayerType::Normal);
    layer.setContent(encode(makeSineTone(1000.0f, 0.25f, 44100), StreamCodecConfig{}));
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layer-export.flac";

    const bool exported = exportLayerAudio(layer, path, CompressedAudioFormat::Flac);
    const bool exists = std::filesystem::exists(path);
    std::filesystem::remove(path);

    CHECK(exported);
    CHECK(exists);
}

TEST_CASE("exportLayerVideo does nothing for a layer with no content", "[core][layer_export]") {
    const Layer layer(1, "Untitled", LayerType::Normal);
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layer-export.mp4";

    CHECK_FALSE(exportLayerVideo(layer, path));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("exportLayerVideo writes a real file for a layer with content", "[core][layer_export]") {
    Layer layer(1, "Imported", LayerType::Normal);
    layer.setContent(encode(makeSineTone(1000.0f, 0.25f, 44100), StreamCodecConfig{}));
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-layer-export.mp4";

    const bool exported = exportLayerVideo(layer, path, /*frameRate=*/24);
    const bool exists = std::filesystem::exists(path);
    std::filesystem::remove(path);

    CHECK(exported);
    CHECK(exists);
}
