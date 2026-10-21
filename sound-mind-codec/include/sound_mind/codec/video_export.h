#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <memory>
#include <functional>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/export_cancelled.h"
#include "sound_mind/codec/rgb_image.h"

namespace sound_mind::codec {

/**
 * @brief One span of a segmented video export - `exportSegmentedVideo()`'s
 *        own building block, `docs/sound-mind-roadmap.md`'s `v0.Y.49.1`
 *        (Macro Mode) Installment C.
 *
 * The moving playhead line (see `exportVideo()`'s own docs) sweeps
 * continuously across the *whole* exported video's own duration,
 * unaffected by segment boundaries - only the static canvas image
 * underneath it cuts to a new one at each segment's own `startTimeSeconds`,
 * matching a scripted macro's own "the project's state changed at this
 * exact moment" semantics.
 */
struct VideoSegment {
    /// @brief The canvas this segment's own frames render, from
    ///        `startTimeSeconds` up to the next segment's own (or the
    ///        video's own end, for the last segment).
    RgbImage canvas;
    /// @brief When this segment begins, in the exported video's own
    ///        timeline - the first segment's own must be `0.0`.
    double startTimeSeconds = 0.0;
};

/**
 * @brief Exports an MP4 video of a spectrogram canvas, synced to its audio.
 *
 * Per `docs/sound-mind-design.md`'s Export section: the whole canvas
 * visible for the full duration, with a moving playback indicator (a
 * vertical line sweeping left to right) - the design doc's other option
 * (a scrolling window that pans to keep the indicator in view) is deferred
 * to a later pass, since a single static image plus a moving line is
 * enough to satisfy "animation synced to audio" without also building
 * scroll-window logic.
 *
 * Video: MPEG-4 Part 2 ("mpeg4" in ffmpeg), an LGPL-compatible codec built
 * into ffmpeg itself - not H.264, which needs the GPL-licensed libx264 and
 * so doesn't fit the closed-source distribution posture already committed
 * to via the JUCE Starter tier (see `docs/sound-mind-architecture.md`'s
 * Decisions Made for both). Audio: AAC, ffmpeg's own native encoder (also
 * LGPL-compatible) - MP4's conventional audio codec, independent of the
 * standalone MP3 export `exportCompressedAudio()` provides.
 *
 * `canvas`'s width/height are right/bottom-padded with black to the
 * nearest even dimensions if needed, since the video's pixel format
 * (YUV420P) needs even width/height for its half-resolution chroma planes
 * - transparent to the caller, just a note on why the output frame size
 * can be 1px larger than `canvas` in either dimension.
 *
 * @param path Destination path.
 * @param canvas The (static) spectrogram image to animate a playhead over -
 *        typically `toRgbImage()`'s output for whichever layer/composite
 *        is being exported.
 * @param audio The audio to mux alongside the video, and to derive the
 *        video's total duration and playhead speed from.
 * @param frameRate Video frame rate, in frames per second.
 * @param shouldCancel Consulted once per video frame, right before that
 *        frame's own render+encode work begins, and again (per input
 *        chunk) during the audio track's own encode right after - see
 *        `sound_mind::codec::detail::encodeAudioTrack()`'s own docs -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone. Once it returns
 *        `true`, throws `ExportCancelled` immediately rather than doing
 *        any further work. `nullptr` (the default) never cancels - the
 *        exact prior behavior, unchanged for every existing caller.
 * @throws std::runtime_error if the file can't be written, or ffmpeg
 *         couldn't create/open an encoder or muxer for it.
 * @throws ExportCancelled if `shouldCancel` returns `true` - see its own
 *         docs.
 */
void exportVideo(const std::filesystem::path& path, const RgbImage& canvas, const AudioBuffer& audio,
                  int frameRate = 30, const std::function<bool()>& shouldCancel = nullptr);

/**
 * @brief Exports an MP4 video whose own canvas image cuts to a new one at
 *        each `VideoSegment`'s own `startTimeSeconds`, synced to one
 *        continuous audio track spanning the whole thing -
 *        `docs/sound-mind-roadmap.md`'s `v0.Y.49.1` (Macro Mode)
 *        Installment C, exporting a recorded macro as a video.
 *        `exportVideo()` itself is exactly the single-segment case of
 *        this (and is implemented as a thin wrapper over it).
 *
 * @param path Destination path.
 * @param segments Every canvas segment, in order - `segments.front()`'s
 *        own `startTimeSeconds` must be `0.0`; every other segment's own
 *        must be strictly increasing. All segments must share the same
 *        `canvas.width`/`canvas.height` (the same project's own canvas,
 *        re-rendered at each state change - not independently sized
 *        images).
 * @param audio The full, already-concatenated audio to mux alongside the
 *        video, and to derive the video's total duration and playhead
 *        speed from - see `exportVideo()`'s own docs.
 * @param frameRate Video frame rate, in frames per second.
 * @param shouldCancel See `exportVideo()`'s own docs.
 * @throws std::runtime_error if `segments` is empty, its own canvas
 *         dimensions are inconsistent, the file can't be written, or
 *         ffmpeg couldn't create/open an encoder or muxer for it.
 * @throws ExportCancelled if `shouldCancel` returns `true`.
 */
void exportSegmentedVideo(const std::filesystem::path& path, const std::vector<VideoSegment>& segments,
                          const AudioBuffer& audio, int frameRate = 30,
                          const std::function<bool()>& shouldCancel = nullptr);

/**
 * @brief Writes an MP4 piece by piece (`v0.1.44.2`): canvas images and
 *        audio are added as they're rendered, so a long Macro video never
 *        holds more than one canvas in memory - unlike
 *        exportSegmentedVideo(), which needs every segment and the whole
 *        audio up front.
 *
 * Same formats as exportVideo() (MPEG-4 Part 2 video, AAC audio). Add video
 * and audio for the same stretch of time alternately (each a segment
 * long); the muxer interleaves them.
 *
 * Not thread-safe; one instance per file.
 */
class StreamedVideoWriter {
public:
    /// @brief Where the playhead line goes at a moment of the video: a
    ///        fraction of the width, or `std::nullopt` for none.
    using PlayheadAt = std::function<std::optional<double>(double videoSeconds)>;

    /**
     * @brief Opens `path` and writes the header.
     * @param path Destination path.
     * @param width Canvas width (padded to even).
     * @param height Canvas height (padded to even).
     * @param sampleRateHz The audio's sample rate.
     * @param frameRate Video frame rate.
     * @throws std::runtime_error if the file or encoders can't be set up.
     */
    StreamedVideoWriter(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
                        std::uint32_t sampleRateHz, int frameRate = 30);

    /// @brief Closes the file, finishing it if finish() wasn't called.
    ~StreamedVideoWriter();

    StreamedVideoWriter(const StreamedVideoWriter&) = delete;
    StreamedVideoWriter& operator=(const StreamedVideoWriter&) = delete;

    /**
     * @brief Adds `seconds` of video showing `canvas` (same size as given
     *        to the constructor), from where the video so far ends.
     * @param canvas The image.
     * @param seconds How long it shows.
     * @param playheadAt Where the playhead line goes; empty for none.
     */
    void addVideo(const RgbImage& canvas, double seconds, const PlayheadAt& playheadAt);

    /// @brief Adds audio after what was added so far.
    /// @param audio The samples, at the constructor's sample rate.
    void addAudio(const AudioBuffer& audio);

    /// @brief How long the video added so far is.
    /// @return Seconds.
    [[nodiscard]] double videoSeconds() const noexcept;

    /// @brief Flushes both encoders and writes the trailer. Further calls
    ///        do nothing.
    void finish();

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace sound_mind::codec
