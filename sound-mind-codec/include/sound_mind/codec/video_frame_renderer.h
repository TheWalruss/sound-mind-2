#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/video_options.h"

namespace sound_mind::codec {

/**
 * @brief Turns a canvas image and a playhead position into one video frame
 *        (`v0.1.45.1`) - the legacy Studio's `render_frame()`, which the
 *        video export is modelled on.
 *
 * - **Whole canvas:** the canvas scaled to fit the video's size (keeping
 *   its proportions, centred on black), with the playhead line sweeping
 *   across it.
 * - **Scrolling:** a `windowSeconds` window of the canvas pans from start
 *   to end as the playhead moves, the line crossing the window meanwhile.
 * - **Polar, whole canvas:** the Sound Flower, with a ray turning clockwise
 *   from twelve o'clock.
 * - **Polar, scrolling:** the flower turns counter-clockwise under a ray
 *   fixed at twelve o'clock. rectToPolar()'s mapping is computed once and
 *   each frame resamples the canvas through it (bilinear, wrapping round),
 *   in parallel over rows - the slowest mode, but not a per-frame
 *   rectToPolar().
 *
 * The playhead is a bright line with a dark shadow on each side, so it
 * shows over any colour; it thickens with the video's height.
 *
 * Scaling uses swscale (area averaging when shrinking, bicubic when
 * enlarging). Work that only depends on the canvas is done once per
 * setCanvas(), so a canvas that changes rarely (a whole-project export, a
 * Macro segment) costs little per frame.
 *
 * Not thread-safe; one per export.
 */
class VideoFrameRenderer {
public:
    /// @brief The playhead's bright colour (legacy Studio yellow).
    static constexpr std::uint8_t kPlayheadRgb[3] = {255, 220, 60};

    /**
     * @param options How to render - size, animation, playhead, polar.
     * @param canvasWidth The canvas images' width.
     * @param canvasHeight Their height.
     * @param projectSeconds How long the canvas lasts - what a scrolling
     *        window is a part of.
     * @throws std::invalid_argument for an empty canvas size.
     */
    VideoFrameRenderer(const VideoExportOptions& options, std::uint32_t canvasWidth, std::uint32_t canvasHeight,
                       double projectSeconds);
    ~VideoFrameRenderer();

    VideoFrameRenderer(const VideoFrameRenderer&) = delete;
    VideoFrameRenderer& operator=(const VideoFrameRenderer&) = delete;

    /// @brief The frames' width (even).
    /// @return Pixels.
    [[nodiscard]] std::uint32_t width() const noexcept;

    /// @brief The frames' height (even).
    /// @return Pixels.
    [[nodiscard]] std::uint32_t height() const noexcept;

    /**
     * @brief Sets the image to render frames of.
     * @param canvas The canvas, the size given to the constructor.
     * @throws std::invalid_argument for a different size.
     */
    void setCanvas(const RgbImage& canvas);

    /**
     * @brief Renders one frame.
     * @param playheadFraction Where the playhead is, as a fraction of the
     *        project (clamped to `[0, 1]`); `std::nullopt` draws none and
     *        shows a scrolling window at its start.
     * @return A width() x height() frame.
     * @throws std::logic_error before setCanvas().
     */
    [[nodiscard]] RgbImage render(std::optional<double> playheadFraction);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sound_mind::codec
