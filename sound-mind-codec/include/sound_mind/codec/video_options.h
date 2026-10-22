#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sound_mind::codec {

/**
 * @brief The video formats the Studio can write (`v0.1.45.1`).
 *
 * Every encoder is LGPL-compatible: x264/x265 (GPL) are deliberately not
 * used, so H.264 and HEVC come from Windows' own Media Foundation encoders,
 * which FFmpeg drives (`h264_mf`/`hevc_mf`). Whether those exist depends on
 * the PC (an "N" edition of Windows, or no HEVC extension) - see
 * availableVideoCodecs().
 */
enum class VideoCodec {
    H264,    ///< MP4, H.264 via Media Foundation - plays everywhere.
    Hevc,    ///< MP4, H.265/HEVC via Media Foundation - smaller files.
    ProRes,  ///< MOV, Apple ProRes 422 (FFmpeg's own encoder) - for editing.
    Mpeg4,   ///< MP4, MPEG-4 Part 2 (FFmpeg's own encoder) - always available.
};

/// @brief How much picture quality (and file size) to spend.
enum class VideoQuality {
    Low,     ///< Small files.
    Medium,  ///< The default.
    High,    ///< Large files.
};

/**
 * @brief How the canvas moves - the legacy Studio's two animation modes.
 */
enum class VideoAnimation {
    /// The whole canvas stays in view; the playhead sweeps across it (in
    /// polar view: a ray turning clockwise from twelve o'clock).
    WholeCanvas,
    /// A window of the canvas pans along with the playhead (in polar view:
    /// the flower turns counter-clockwise under a ray fixed at twelve
    /// o'clock).
    Scrolling,
};

/**
 * @brief Everything about how a video is rendered and encoded (`v0.1.45.1`).
 */
struct VideoExportOptions {
    /// The format.
    VideoCodec codec = VideoCodec::Mpeg4;
    /// The quality.
    VideoQuality quality = VideoQuality::Medium;
    /// The video's width and height in pixels; `0` uses the canvas's own
    /// size. The canvas is scaled to fit, keeping its proportions, and
    /// centred on black (letterboxed). Rounded up to even.
    std::uint32_t width = 0;
    /// See width.
    std::uint32_t height = 0;
    /// Frames per second.
    int frameRate = 30;
    /// How the canvas moves.
    VideoAnimation animation = VideoAnimation::WholeCanvas;
    /// Scrolling: how many seconds of the canvas are in view at once (no
    /// effect in polar view, where the whole flower always shows).
    double windowSeconds = 30.0;
    /// Whether to draw the playhead.
    bool showPlayhead = true;
    /// Whether to render the canvas as a Sound Flower (polar view).
    bool polar = false;
};

/**
 * @brief The formats this PC can actually write, in VideoCodec order - each
 *        encoder is opened once, on a worker thread (Media Foundation needs
 *        its own COM apartment), and the answer cached.
 * @return The available codecs; always includes `Mpeg4` and `ProRes`.
 */
[[nodiscard]] const std::vector<VideoCodec>& availableVideoCodecs();

/// @brief Whether availableVideoCodecs() includes `codec`.
/// @param codec The codec.
/// @return `true` if it can be written.
[[nodiscard]] bool isVideoCodecAvailable(VideoCodec codec);

/// @brief The file extension a codec's container uses.
/// @param codec The codec.
/// @return `"mp4"` or `"mov"`.
[[nodiscard]] std::string videoFileExtension(VideoCodec codec);

/// @brief The best format available: H.264 if this PC has it, MPEG-4 Part
///        2 otherwise.
/// @return The codec.
[[nodiscard]] VideoCodec preferredVideoCodec();

}  // namespace sound_mind::codec
