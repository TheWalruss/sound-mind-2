#pragma once

#include <filesystem>
#include <optional>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/audio_export.h"
#include "sound_mind/core/layer.h"

namespace sound_mind::core {

/**
 * @brief Decodes the best available audio for exporting `layer`.
 *
 * Per `docs/sound-mind-design.md`'s Export section - "Pool-mode primary, a
 * quick Stream-mode bounce for scratch use" - this prefers `layer`'s Pool
 * content (the full phase-aware, near-lossless round trip) when the layer
 * has been Pooled (see `poolLayer()`), and falls back to decoding its
 * Stream content otherwise (a faster but only approximately phase-accurate
 * bounce - see `docs/sound-mind-architecture.md`'s Stream File Format).
 *
 * @param layer The layer to export audio from.
 * @return The decoded audio, or `std::nullopt` if the layer has no content
 *         at all yet (neither Pool nor Stream).
 */
[[nodiscard]] std::optional<sound_mind::codec::AudioBuffer> decodeLayerForExport(const Layer& layer);

/**
 * @brief Exports `layer`'s audio (see `decodeLayerForExport()`) to a
 *        compressed audio file.
 *
 * @param layer The layer to export.
 * @param path Destination path.
 * @param format Which compressed format to write - see
 *        `sound_mind::codec::exportCompressedAudio()`.
 * @return `true` if exported; `false` if the layer had no content to
 *         export (nothing written).
 * @throws std::runtime_error if the underlying codec export fails (see
 *         `sound_mind::codec::exportCompressedAudio()`'s docs).
 */
bool exportLayerAudio(const Layer& layer, const std::filesystem::path& path,
                       sound_mind::codec::CompressedAudioFormat format);

/**
 * @brief Exports `layer` as an MP4 video: its rendered canvas (see
 *        `renderLayer()`) animated with a playhead synced to its audio
 *        (see `decodeLayerForExport()`).
 *
 * @param layer The layer to export.
 * @param path Destination path.
 * @param frameRate Video frame rate, in frames per second - see
 *        `sound_mind::codec::exportVideo()`.
 * @return `true` if exported; `false` if the layer had no content to
 *         export (nothing written).
 * @throws std::runtime_error if the underlying codec export fails (see
 *         `sound_mind::codec::exportVideo()`'s docs).
 */
bool exportLayerVideo(const Layer& layer, const std::filesystem::path& path, int frameRate = 30);

}  // namespace sound_mind::core
