#include "sound_mind/codec/video_frame_renderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "ffmpeg_raii.h"
#include "sound_mind/codec/parallel.h"
#include "sound_mind/codec/polar_projection.h"

namespace sound_mind::codec {

namespace {

using detail::SwsContextPtr;

constexpr std::uint8_t kShadowRgb[3] = {0, 0, 0};

std::uint32_t evenAtLeastTwo(std::uint32_t value) {
    value = std::max<std::uint32_t>(value, 2);
    return value % 2 == 0 ? value : value + 1;
}

/// Where a `width` x `height` image lands when fitted inside `outWidth` x
/// `outHeight`, keeping its proportions and centred.
struct Fit {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 1;
    std::uint32_t height = 1;
};

Fit fitInside(std::uint32_t width, std::uint32_t height, std::uint32_t outWidth, std::uint32_t outHeight) {
    const double scale = std::min(static_cast<double>(outWidth) / std::max<std::uint32_t>(width, 1),
                                  static_cast<double>(outHeight) / std::max<std::uint32_t>(height, 1));
    Fit fit;
    fit.width = std::clamp<std::uint32_t>(static_cast<std::uint32_t>(std::lround(width * scale)), 1, outWidth);
    fit.height = std::clamp<std::uint32_t>(static_cast<std::uint32_t>(std::lround(height * scale)), 1, outHeight);
    fit.x = (outWidth - fit.width) / 2;
    fit.y = (outHeight - fit.height) / 2;
    return fit;
}

/// An RGB24 -> RGB24 resize at fixed sizes - area averaging when shrinking
/// (so a long canvas doesn't alias), bicubic when enlarging.
class Scaler {
public:
    void configure(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth, std::uint32_t dstHeight) {
        if (sws_ && srcWidth == srcWidth_ && srcHeight == srcHeight_ && dstWidth == dstWidth_ &&
            dstHeight == dstHeight_) {
            return;
        }
        const bool shrinking = dstWidth < srcWidth || dstHeight < srcHeight;
        sws_.reset(sws_getContext(static_cast<int>(srcWidth), static_cast<int>(srcHeight), AV_PIX_FMT_RGB24,
                                  static_cast<int>(dstWidth), static_cast<int>(dstHeight), AV_PIX_FMT_RGB24,
                                  (shrinking ? SWS_AREA : SWS_BICUBIC) | SWS_ACCURATE_RND, nullptr, nullptr,
                                  nullptr));
        if (!sws_) {
            throw std::runtime_error("could not create an image scaler");
        }
        srcWidth_ = srcWidth;
        srcHeight_ = srcHeight;
        dstWidth_ = dstWidth;
        dstHeight_ = dstHeight;
    }

    /// Scales from `src` (rows `srcStride` bytes apart) into `dst` at
    /// (`dstX`, `dstY`) inside a frame `dstFrameWidth` pixels wide.
    void scale(const std::uint8_t* src, int srcStride, std::uint8_t* dstFrame, std::uint32_t dstFrameWidth,
               std::uint32_t dstX, std::uint32_t dstY) {
        const std::uint8_t* srcSlices[1] = {src};
        const int srcStrides[1] = {srcStride};
        std::uint8_t* dstSlices[1] = {dstFrame + (std::size_t{dstY} * dstFrameWidth + dstX) * 3};
        const int dstStrides[1] = {static_cast<int>(dstFrameWidth * 3)};
        sws_scale(sws_.get(), srcSlices, srcStrides, 0, static_cast<int>(srcHeight_), dstSlices, dstStrides);
    }

private:
    SwsContextPtr sws_;
    std::uint32_t srcWidth_ = 0;
    std::uint32_t srcHeight_ = 0;
    std::uint32_t dstWidth_ = 0;
    std::uint32_t dstHeight_ = 0;
};

void fillSquare(RgbImage& frame, double cx, double cy, int size, const std::uint8_t (&rgb)[3]) {
    const int half = size / 2;
    const int x0 = static_cast<int>(std::floor(cx)) - half;
    const int y0 = static_cast<int>(std::floor(cy)) - half;
    for (int y = std::max(0, y0); y < std::min<int>(static_cast<int>(frame.height), y0 + size); ++y) {
        for (int x = std::max(0, x0); x < std::min<int>(static_cast<int>(frame.width), x0 + size); ++x) {
            std::uint8_t* pixel = frame.pixels.data() + (static_cast<std::size_t>(y) * frame.width + x) * 3;
            pixel[0] = rgb[0];
            pixel[1] = rgb[1];
            pixel[2] = rgb[2];
        }
    }
}

/// The playhead as a vertical line at `x`, rows `top` to `bottom`: a
/// `thickness`-wide bright core with a one-pixel dark shadow either side.
void drawVerticalPlayhead(RgbImage& frame, std::uint32_t x, std::uint32_t top, std::uint32_t bottom,
                          int thickness) {
    const int core0 = static_cast<int>(x) - (thickness - 1) / 2;
    for (int column = core0 - 1; column <= core0 + thickness; ++column) {
        if (column < 0 || column >= static_cast<int>(frame.width)) {
            continue;
        }
        const bool core = column >= core0 && column < core0 + thickness;
        const auto& rgb = core ? VideoFrameRenderer::kPlayheadRgb : kShadowRgb;
        for (std::uint32_t y = top; y < bottom; ++y) {
            std::uint8_t* pixel = frame.pixels.data() + (std::size_t{y} * frame.width + column) * 3;
            pixel[0] = rgb[0];
            pixel[1] = rgb[1];
            pixel[2] = rgb[2];
        }
    }
}

/// The playhead as a ray from (`cx`, `cy`), `length` long, at `theta`
/// (0 = twelve o'clock, clockwise): shadow pass, then core pass.
void drawRay(RgbImage& frame, double cx, double cy, double length, double theta, int thickness) {
    const double dx = std::sin(theta);
    const double dy = -std::cos(theta);
    for (int pass = 0; pass < 2; ++pass) {
        const int size = pass == 0 ? thickness + 2 : thickness;
        const auto& rgb = pass == 0 ? kShadowRgb : VideoFrameRenderer::kPlayheadRgb;
        for (double step = 0.0; step <= length; step += 0.5) {
            fillSquare(frame, cx + dx * step, cy + dy * step, size, rgb);
        }
    }
}

/// Where a polar frame pixel samples the canvas, before rotation; `y < 0`
/// marks a pixel outside the disc.
struct PolarSample {
    float x = 0.0f;
    float y = -1.0f;
};

}  // namespace

/// @cond INTERNAL
// The renderer's private state; documented on VideoFrameRenderer itself.
struct VideoFrameRenderer::Impl {
    VideoExportOptions options;
    std::uint32_t canvasWidth = 0;
    std::uint32_t canvasHeight = 0;
    double projectSeconds = 0.0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    int thickness = 1;

    bool hasCanvas = false;
    RgbImage canvas;
    /// Whole canvas (or a polar whole canvas): the frame without playhead.
    RgbImage base;
    /// Scrolling: the window's width in canvas columns, and where it lands.
    std::uint32_t windowColumns = 0;
    Fit windowFit;
    Fit wholeFit;
    std::uint32_t diameter = 0;
    Scaler scaler;
    /// Scrolling: the window's pixels, copied out of the canvas.
    std::vector<std::uint8_t> window;

    /// Polar scrolling: rectToPolar()'s mapping for every frame pixel.
    std::vector<PolarSample> polarMap;

    void buildPolarMap() {
        if (!polarMap.empty()) {
            return;
        }
        polarMap.resize(std::size_t{diameter} * diameter);
        const double half = diameter / 2.0;
        for (std::uint32_t py = 0; py < diameter; ++py) {
            const double dy = py - half + 0.5;
            for (std::uint32_t px = 0; px < diameter; ++px) {
                const double dx = px - half + 0.5;
                const double r = std::sqrt(dx * dx + dy * dy);
                if (r > half) {
                    continue;
                }
                double theta = std::atan2(dx, -dy);
                if (theta < 0.0) {
                    theta += 2.0 * std::numbers::pi;
                }
                PolarSample& sample = polarMap[std::size_t{py} * diameter + px];
                sample.x = static_cast<float>(canvasWidth * theta / (2.0 * std::numbers::pi));
                sample.y = static_cast<float>(std::clamp(canvasHeight * (1.0 - r / half), 0.0, canvasHeight - 1.0));
            }
        }
    }

    RgbImage blank() const {
        RgbImage frame;
        frame.width = width;
        frame.height = height;
        frame.pixels.assign(std::size_t{width} * height * 3, 0);
        return frame;
    }

    void pastePolar(RgbImage& frame, const RgbImage& source) const {
        const std::uint32_t x0 = (width - diameter) / 2;
        const std::uint32_t y0 = (height - diameter) / 2;
        for (std::uint32_t y = 0; y < diameter; ++y) {
            std::copy_n(source.pixels.data() + std::size_t{y} * diameter * 3, std::size_t{diameter} * 3,
                        frame.pixels.data() + (std::size_t{y0 + y} * width + x0) * 3);
        }
    }
};
/// @endcond

VideoFrameRenderer::VideoFrameRenderer(const VideoExportOptions& options, std::uint32_t canvasWidth,
                                       std::uint32_t canvasHeight, double projectSeconds)
    : impl_(std::make_unique<Impl>()) {
    if (canvasWidth == 0 || canvasHeight == 0) {
        throw std::invalid_argument("cannot render video frames of an empty canvas");
    }
    Impl& im = *impl_;
    im.options = options;
    im.canvasWidth = canvasWidth;
    im.canvasHeight = canvasHeight;
    im.projectSeconds = projectSeconds;
    im.width = evenAtLeastTwo(options.width > 0 ? options.width : canvasWidth);
    im.height = evenAtLeastTwo(options.height > 0 ? options.height : canvasHeight);
    im.thickness = std::max(1, static_cast<int>(std::lround(im.height / 540.0)));
    im.wholeFit = fitInside(canvasWidth, canvasHeight, im.width, im.height);
    im.windowColumns = canvasWidth;
    if (options.animation == VideoAnimation::Scrolling && projectSeconds > 0.0) {
        im.windowColumns = std::clamp<std::uint32_t>(
            static_cast<std::uint32_t>(std::lround(options.windowSeconds / projectSeconds * canvasWidth)), 1,
            canvasWidth);
    }
    im.windowFit = fitInside(im.windowColumns, canvasHeight, im.width, im.height);
    im.diameter = std::min(im.width, im.height) / 2 * 2;
}

VideoFrameRenderer::~VideoFrameRenderer() = default;

std::uint32_t VideoFrameRenderer::width() const noexcept { return impl_->width; }

std::uint32_t VideoFrameRenderer::height() const noexcept { return impl_->height; }

void VideoFrameRenderer::setCanvas(const RgbImage& canvas) {
    Impl& im = *impl_;
    if (canvas.width != im.canvasWidth || canvas.height != im.canvasHeight) {
        throw std::invalid_argument("every canvas of one video must have the same size");
    }
    im.canvas = canvas;
    im.hasCanvas = true;
    const bool scrolling = im.options.animation == VideoAnimation::Scrolling;
    if (im.options.polar) {
        if (!scrolling) {
            im.base = im.blank();
            im.pastePolar(im.base, rectToPolar(canvas, im.diameter));
        }
    } else if (!scrolling) {
        im.base = im.blank();
        im.scaler.configure(canvas.width, canvas.height, im.wholeFit.width, im.wholeFit.height);
        im.scaler.scale(canvas.pixels.data(), static_cast<int>(canvas.width * 3), im.base.pixels.data(), im.width,
                        im.wholeFit.x, im.wholeFit.y);
    }
}

RgbImage VideoFrameRenderer::render(std::optional<double> playheadFraction) {
    Impl& im = *impl_;
    if (!im.hasCanvas) {
        throw std::logic_error("VideoFrameRenderer::render() before setCanvas()");
    }
    const double fraction = std::clamp(playheadFraction.value_or(0.0), 0.0, 1.0);
    const bool scrolling = im.options.animation == VideoAnimation::Scrolling;
    const bool drawPlayhead = im.options.showPlayhead && playheadFraction.has_value();

    if (im.options.polar) {
        const double cx = im.width / 2.0;
        const double cy = im.height / 2.0;
        const double length = im.diameter / 2.0 * 1.03;
        if (!scrolling) {
            RgbImage frame = im.base;
            if (drawPlayhead) {
                drawRay(frame, cx, cy, length, fraction * 2.0 * std::numbers::pi, im.thickness);
            }
            return frame;
        }
        // The flower turned counter-clockwise by the playhead's angle: the
        // moment being played sits under the fixed ray at twelve o'clock.
        // rectToPolar()'s own mapping, computed once; each frame only offsets
        // the column (wrapping round the canvas) and samples.
        im.buildPolarMap();
        const double shift = fraction * im.canvasWidth;
        RgbImage frame = im.blank();
        const std::uint32_t x0 = (im.width - im.diameter) / 2;
        const std::uint32_t y0 = (im.height - im.diameter) / 2;
        const auto width = static_cast<double>(im.canvasWidth);
        sound_mind::codec::parallelFor(im.diameter, 16, [&](std::size_t begin, std::size_t end) {
            for (std::size_t py = begin; py < end; ++py) {
                for (std::uint32_t px = 0; px < im.diameter; ++px) {
                    const PolarSample& sample = im.polarMap[py * im.diameter + px];
                    if (sample.y < 0.0f) {
                        continue;  // outside the disc.
                    }
                    double x = sample.x + shift;
                    x -= std::floor(x / width) * width;
                    const auto xa = static_cast<std::uint32_t>(x) % im.canvasWidth;
                    const std::uint32_t xb = (xa + 1) % im.canvasWidth;
                    const double fx = x - std::floor(x);
                    const auto ya = static_cast<std::uint32_t>(sample.y);
                    const std::uint32_t yb = std::min(ya + 1, im.canvasHeight - 1);
                    const double fy = sample.y - ya;
                    std::uint8_t* out = frame.pixels.data() + ((y0 + py) * im.width + x0 + px) * 3;
                    for (int c = 0; c < 3; ++c) {
                        const auto pick = [&](std::uint32_t cx, std::uint32_t cy) {
                            return static_cast<double>(im.canvas.pixels[(std::size_t{cy} * im.canvasWidth + cx) * 3 + c]);
                        };
                        const double top = pick(xa, ya) * (1.0 - fx) + pick(xb, ya) * fx;
                        const double bottom = pick(xa, yb) * (1.0 - fx) + pick(xb, yb) * fx;
                        out[c] = static_cast<std::uint8_t>(std::clamp(top * (1.0 - fy) + bottom * fy, 0.0, 255.0));
                    }
                }
            }
        });
        if (drawPlayhead) {
            drawRay(frame, cx, cy, length, 0.0, im.thickness);
        }
        return frame;
    }

    if (!scrolling) {
        RgbImage frame = im.base;
        if (drawPlayhead) {
            const auto x = im.wholeFit.x + static_cast<std::uint32_t>(std::lround(fraction * (im.wholeFit.width - 1)));
            drawVerticalPlayhead(frame, x, im.wholeFit.y, im.wholeFit.y + im.wholeFit.height, im.thickness);
        }
        return frame;
    }

    // Scrolling: the window pans from the canvas's start to its end while
    // the playhead crosses it - the legacy Studio's arrangement.
    const std::uint32_t start =
        static_cast<std::uint32_t>(std::lround(fraction * (im.canvasWidth - im.windowColumns)));
    // Copied out first: swscale wants its input rows aligned.
    im.window.resize(std::size_t{im.windowColumns} * im.canvasHeight * 3);
    for (std::uint32_t y = 0; y < im.canvasHeight; ++y) {
        std::copy_n(im.canvas.pixels.data() + (std::size_t{y} * im.canvasWidth + start) * 3,
                    std::size_t{im.windowColumns} * 3, im.window.data() + std::size_t{y} * im.windowColumns * 3);
    }
    RgbImage frame = im.blank();
    im.scaler.configure(im.windowColumns, im.canvasHeight, im.windowFit.width, im.windowFit.height);
    im.scaler.scale(im.window.data(), static_cast<int>(im.windowColumns * 3), frame.pixels.data(), im.width,
                    im.windowFit.x, im.windowFit.y);
    if (drawPlayhead) {
        const auto x = im.windowFit.x + static_cast<std::uint32_t>(std::lround(fraction * (im.windowFit.width - 1)));
        drawVerticalPlayhead(frame, x, im.windowFit.y, im.windowFit.y + im.windowFit.height, im.thickness);
    }
    return frame;
}

}  // namespace sound_mind::codec
