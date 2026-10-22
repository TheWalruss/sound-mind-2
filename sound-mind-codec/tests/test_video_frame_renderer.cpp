#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/video_frame_renderer.h"

using sound_mind::codec::RgbImage;
using sound_mind::codec::VideoAnimation;
using sound_mind::codec::VideoExportOptions;
using sound_mind::codec::VideoFrameRenderer;

namespace {

using Rgb = std::array<std::uint8_t, 3>;
constexpr Rgb kRed{200, 0, 0};
constexpr Rgb kBlue{0, 0, 200};
constexpr Rgb kBlack{0, 0, 0};
constexpr Rgb kYellow{255, 220, 60};

/// Left half red, right half blue.
RgbImage halves(std::uint32_t width, std::uint32_t height) {
    RgbImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(std::size_t{width} * height * 3);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const Rgb& c = x < width / 2 ? kRed : kBlue;
            std::copy(c.begin(), c.end(), image.pixels.begin() + static_cast<std::ptrdiff_t>((y * width + x) * 3));
        }
    }
    return image;
}

Rgb at(const RgbImage& image, std::uint32_t x, std::uint32_t y) {
    const auto* p = image.pixels.data() + (std::size_t{y} * image.width + x) * 3;
    return {p[0], p[1], p[2]};
}

bool close(const Rgb& a, const Rgb& b) {
    for (int i = 0; i < 3; ++i) {
        if (std::abs(int{a[i]} - int{b[i]}) > 12) {
            return false;
        }
    }
    return true;
}

}  // namespace

// v0.1.45.1 - the legacy Studio's frame rendering, rebuilt.
TEST_CASE("Frames default to the canvas size, rounded up to even", "[video_frames]") {
    VideoFrameRenderer renderer(VideoExportOptions{}, 101, 51, 10.0);
    CHECK(renderer.width() == 102);
    CHECK(renderer.height() == 52);
}

TEST_CASE("The canvas is letterboxed into the chosen size, keeping its proportions", "[video_frames]") {
    VideoExportOptions options;
    options.width = 1920;
    options.height = 1080;
    VideoFrameRenderer renderer(options, 400, 100, 10.0);  // 4:1 into 16:9: bars above and below.
    renderer.setCanvas(halves(400, 100));
    const RgbImage frame = renderer.render(std::nullopt);
    REQUIRE(frame.width == 1920);
    REQUIRE(frame.height == 1080);
    CHECK(at(frame, 960, 10) == kBlack);             // bar.
    CHECK(close(at(frame, 400, 540), kRed));         // picture, left half.
    CHECK(close(at(frame, 1500, 540), kBlue));       // picture, right half.
    CHECK(at(frame, 960, 1070) == kBlack);           // bar.
}

TEST_CASE("The whole-canvas playhead sweeps across, shadowed, and only when asked", "[video_frames]") {
    VideoExportOptions options;
    options.width = 200;
    options.height = 100;
    VideoFrameRenderer renderer(options, 200, 100, 10.0);
    renderer.setCanvas(halves(200, 100));
    const RgbImage middle = renderer.render(0.5);
    const std::uint32_t x = 100;  // round(0.5 * 199) = 100.
    CHECK(at(middle, x, 50) == kYellow);
    CHECK(at(middle, x - 1, 50) == kBlack);  // shadow.
    CHECK(at(middle, x + 1, 50) == kBlack);
    CHECK(at(renderer.render(0.0), 0, 50) == kYellow);

    VideoExportOptions hidden = options;
    hidden.showPlayhead = false;
    VideoFrameRenderer plain(hidden, 200, 100, 10.0);
    plain.setCanvas(halves(200, 100));
    CHECK(close(at(plain.render(0.5), x, 50), kBlue));
}

TEST_CASE("The playhead thickens with the video's height", "[video_frames]") {
    VideoExportOptions options;
    options.width = 1920;
    options.height = 1080;
    VideoFrameRenderer renderer(options, 1920, 1080, 10.0);
    renderer.setCanvas(halves(1920, 1080));
    const RgbImage frame = renderer.render(0.25);
    int yellow = 0;
    for (std::uint32_t x = 470; x < 490; ++x) {
        yellow += at(frame, x, 500) == kYellow ? 1 : 0;
    }
    CHECK(yellow == 2);  // 1080 / 540.
}

TEST_CASE("A scrolling window pans from the start of the canvas to its end", "[video_frames]") {
    VideoExportOptions options;
    options.width = 100;
    options.height = 100;
    options.animation = VideoAnimation::Scrolling;
    options.windowSeconds = 2.5;  // a quarter of the canvas.
    VideoFrameRenderer renderer(options, 400, 100, 10.0);
    renderer.setCanvas(halves(400, 100));
    CHECK(close(at(renderer.render(0.0), 60, 50), kRed));   // window over the start.
    CHECK(close(at(renderer.render(1.0), 40, 50), kBlue));  // ...and over the end.
    const RgbImage half = renderer.render(0.5);
    CHECK(at(half, 50, 50) == kYellow);  // the playhead crosses the window as it pans.
}

TEST_CASE("Polar: a ray turning clockwise from twelve o'clock over the flower", "[video_frames]") {
    VideoExportOptions options;
    options.width = 160;
    options.height = 100;
    options.polar = true;
    VideoFrameRenderer renderer(options, 200, 50, 10.0);
    renderer.setCanvas(halves(200, 50));
    const RgbImage start = renderer.render(0.0);
    CHECK(at(start, 2, 50) == kBlack);       // outside the disc.
    CHECK(at(start, 80, 20) == kYellow);     // the ray, straight up from the centre (80, 50).
    const RgbImage quarter = renderer.render(0.25);
    CHECK(at(quarter, 110, 50) == kYellow);  // three o'clock.
}

TEST_CASE("Polar scrolling: the flower turns under a fixed ray", "[video_frames]") {
    VideoExportOptions options;
    options.width = 100;
    options.height = 100;
    options.polar = true;
    options.animation = VideoAnimation::Scrolling;
    options.showPlayhead = false;
    VideoFrameRenderer renderer(options, 200, 50, 10.0);
    renderer.setCanvas(halves(200, 50));
    // Just clockwise of twelve o'clock: the moment being played.
    CHECK(close(at(renderer.render(0.0), 56, 20), kRed));
    CHECK(close(at(renderer.render(0.5), 56, 20), kBlue));
}

TEST_CASE("The renderer refuses a mismatched canvas and rendering before one is set", "[video_frames]") {
    VideoFrameRenderer renderer(VideoExportOptions{}, 20, 10, 1.0);
    CHECK_THROWS_AS(renderer.render(0.0), std::logic_error);
    CHECK_THROWS_AS(renderer.setCanvas(halves(21, 10)), std::invalid_argument);
    CHECK_THROWS_AS(VideoFrameRenderer(VideoExportOptions{}, 0, 10, 1.0), std::invalid_argument);
}
