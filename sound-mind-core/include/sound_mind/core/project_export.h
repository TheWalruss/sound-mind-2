#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include "sound_mind/core/master_limiter.h"
#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/audio_export.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/core/project.h"

namespace sound_mind::core {

/// @brief The whole project, rendered for export: what Playback plays and
///        what the canvas shows.
struct ProjectExportMedia {
    /// @brief The decoded audio of the full composite.
    sound_mind::codec::AudioBuffer audio;
    /// @brief The full composite as an image, for video export.
    sound_mind::codec::RgbImage canvas;
};

/**
 * @brief Renders the whole layer stack for export - the same composite
 *        Playback hears (`compositeProject()` with `respectMute = true`),
 *        decoded to audio, plus its canvas image.
 *
 * Hidden and muted layers are left out, exactly as in Playback; filter
 * layers apply as they do there. The audio is a Stream-mode decode of the
 * composite: compositing works on Stream content, so a Pooled layer's
 * higher-fidelity Pool content isn't used here.
 *
 * @param project The project to render. Read only; safe to call on a copy
 *        from a background thread.
 * @param shouldCancel Forwarded to `compositeProject()`.
 * @return The rendered media, or `std::nullopt` if no layer contributes.
 * @throws CompositeCancelled if `shouldCancel` returns `true`.
 */
[[nodiscard]] std::optional<ProjectExportMedia> renderProjectForExport(
    const Project& project, const std::function<bool()>& shouldCancel = nullptr);

/**
 * @brief Exports the whole project's audio (see renderProjectForExport())
 *        to a compressed audio file.
 *
 * @param project The project to export.
 * @param path Destination path.
 * @param format Which compressed format to write - see
 *        `sound_mind::codec::exportCompressedAudio()`.
 * @param shouldCancel Consulted while compositing and while encoding.
 * @param limiterCeilingDb Runs the audio through the master limiter at this
 *        ceiling before encoding (`applyMasterLimiter()`, Pre-Beta Plan
 *        `v0.Y.80.1`) - on by default, as in playback; `std::nullopt` exports
 *        the mix unlimited.
 * @return `true` if exported; `false` if no layer contributes (nothing
 *         written).
 * @throws std::runtime_error if the underlying codec export fails.
 * @throws CompositeCancelled or sound_mind::codec::ExportCancelled if
 *         `shouldCancel` returns `true` during compositing or encoding
 *         respectively.
 */
bool exportProjectAudio(const Project& project, const std::filesystem::path& path,
                        sound_mind::codec::CompressedAudioFormat format,
                        const std::function<bool()>& shouldCancel = nullptr,
                        std::optional<float> limiterCeilingDb = kDefaultLimiterCeilingDb);

/**
 * @brief Exports the whole project as an MP4 video: the composite canvas
 *        animated with a playhead synced to the composite audio (see
 *        renderProjectForExport()).
 *
 * @param project The project to export.
 * @param path Destination path.
 * @param frameRate Video frame rate, in frames per second - see
 *        `sound_mind::codec::exportVideo()`.
 * @param shouldCancel Consulted while compositing and while encoding.
 * @param limiterCeilingDb Runs the audio through the master limiter at this
 *        ceiling before encoding (`applyMasterLimiter()`, Pre-Beta Plan
 *        `v0.Y.80.1`) - on by default, as in playback; `std::nullopt` exports
 *        the mix unlimited.
 * @return `true` if exported; `false` if no layer contributes (nothing
 *         written).
 * @throws std::runtime_error if the underlying codec export fails.
 * @throws CompositeCancelled or sound_mind::codec::ExportCancelled if
 *         `shouldCancel` returns `true` during compositing or encoding
 *         respectively.
 */
bool exportProjectVideo(const Project& project, const std::filesystem::path& path, int frameRate = 30,
                        const std::function<bool()>& shouldCancel = nullptr,
                        std::optional<float> limiterCeilingDb = kDefaultLimiterCeilingDb);

}  // namespace sound_mind::core
