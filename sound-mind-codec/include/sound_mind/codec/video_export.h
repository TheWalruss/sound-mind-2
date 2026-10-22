#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/export_cancelled.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/video_options.h"

namespace sound_mind::codec {

/**
 * @brief Writes a video piece by piece (`v0.1.44.2`; formats, sizes and
 *        animation since `v0.1.45.1`): canvas images and audio are added as
 *        they're rendered, so even a long Macro video holds one canvas in
 *        memory.
 *
 * Every frame goes through a `VideoFrameRenderer` - the canvas scaled to
 * the chosen size, whole or scrolling, flat or polar, with the playhead -
 * and is encoded in the chosen format (`VideoExportOptions`): H.264 or HEVC
 * in MP4 through Windows' Media Foundation encoders, ProRes 422 in MOV, or
 * MPEG-4 Part 2 in MP4. Audio is AAC (PCM in a ProRes MOV, which editors
 * prefer).
 *
 * Add video and audio for the same stretch of time alternately (each a
 * second or a segment long); the muxer interleaves them.
 *
 * Not thread-safe; one instance per file. Media Foundation encoders need a
 * thread of their own (not the UI thread): run exports on a worker.
 */
class StreamedVideoWriter {
public:
    /// @brief Where the playhead goes at a moment of the video: a fraction
    ///        of the project, or `std::nullopt` for none.
    using PlayheadAt = std::function<std::optional<double>(double videoSeconds)>;

    /**
     * @brief Opens `path` and writes the header.
     * @param path Destination path (its extension should match the
     *        codec's - see `videoFileExtension()`).
     * @param canvasWidth The canvas images' width.
     * @param canvasHeight Their height.
     * @param projectSeconds How long the canvas lasts (a scrolling window is
     *        a part of it).
     * @param sampleRateHz The audio's sample rate.
     * @param options Format, size, frame rate, animation, playhead, polar.
     * @throws std::runtime_error if the format isn't available here, or the
     *         file or encoders can't be set up.
     */
    StreamedVideoWriter(const std::filesystem::path& path, std::uint32_t canvasWidth, std::uint32_t canvasHeight,
                        double projectSeconds, std::uint32_t sampleRateHz, const VideoExportOptions& options);

    /// @brief Closes the file, finishing it if finish() wasn't called.
    ~StreamedVideoWriter();

    StreamedVideoWriter(const StreamedVideoWriter&) = delete;
    StreamedVideoWriter& operator=(const StreamedVideoWriter&) = delete;

    /**
     * @brief Adds `seconds` of video showing `canvas`, from where the video
     *        so far ends.
     * @param canvas The image (the constructor's size).
     * @param seconds How long it shows.
     * @param playheadAt Where the playhead goes; empty for none.
     * @throws ExportCancelled if the cancel check (setShouldCancel()) says so.
     */
    void addVideo(const RgbImage& canvas, double seconds, const PlayheadAt& playheadAt);

    /// @brief Adds audio after what was added so far.
    /// @param audio The samples, at the constructor's sample rate.
    void addAudio(const AudioBuffer& audio);

    /// @brief How long the video added so far is.
    /// @return Seconds.
    [[nodiscard]] double videoSeconds() const noexcept;

    /// @brief The frames' size, after scaling and rounding to even.
    /// @return Width in pixels.
    [[nodiscard]] std::uint32_t frameWidth() const noexcept;

    /// @brief See frameWidth().
    /// @return Height in pixels.
    [[nodiscard]] std::uint32_t frameHeight() const noexcept;

    /// @brief Checked before every frame; `true` throws `ExportCancelled`.
    /// @param shouldCancel The check.
    void setShouldCancel(std::function<bool()> shouldCancel);

    /// @brief Flushes both encoders and writes the trailer. Further calls
    ///        do nothing.
    void finish();

private:
    struct State;
    std::unique_ptr<State> state_;
};

/**
 * @brief Writes a video of one canvas with its audio (options
 *        since `v0.1.45.1`): the playhead travels the canvas over the
 *        audio's duration.
 * @param path Destination path.
 * @param canvas The canvas.
 * @param audio The whole audio; the video lasts as long.
 * @param options Format, size, frame rate, animation, playhead, polar.
 * @param shouldCancel Polled before every frame; `true` stops with
 *        `ExportCancelled` (leaving a partial file for the caller to
 *        remove).
 * @param progress Told the fraction done, `0` to `1`; may be empty.
 * @throws std::runtime_error for an empty canvas, a non-positive frame rate,
 *         an unavailable format or an unwritable file.
 * @throws ExportCancelled if cancelled.
 */
void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                 const VideoExportOptions& options, const std::function<bool()>& shouldCancel = nullptr,
                 const std::function<void(double)>& progress = nullptr);

/**
 * @brief exportVideo() with default options at `frameRate` - MPEG-4 Part 2
 *        at the canvas's own size, the whole canvas with its playhead.
 * @param path Destination path.
 * @param canvas The canvas.
 * @param audio The audio.
 * @param frameRate Frames per second.
 * @param shouldCancel See the other overload.
 */
void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                 int frameRate = 30, const std::function<bool()>& shouldCancel = nullptr);

}  // namespace sound_mind::codec
