#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paste_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyPasteOperation;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::captureClip;
using sound_mind::core::Clip;
using sound_mind::core::frameIndexToTime;
using sound_mind::core::LayerId;
using sound_mind::core::PasteOperation;
using sound_mind::core::TimeFrequencyRect;

namespace {

StreamCodecConfig makeTestConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;  // ~10ms/frame.
    config.binCount = 100;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

StreamImage makeBlankContent(const StreamCodecConfig& config, std::uint32_t frameCount) {
    StreamImage content;
    content.config = config;
    content.frameCount = frameCount;
    content.leftMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    content.rightMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    content.sharedPhaseRadians.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    return content;
}

std::size_t pixelIndex(const StreamImage& content, int frame, int bin) {
    return static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
}

}  // namespace

TEST_CASE("captureClip copies exactly the cells within bounds, with the right dimensions",
          "[core][paste_application]") {
    const auto config = makeTestConfig();
    StreamImage source = makeBlankContent(config, 100);
    source.leftMagnitudeDb[pixelIndex(source, 20, 10)] = -1.0f;
    source.rightMagnitudeDb[pixelIndex(source, 20, 10)] = -2.0f;
    source.sharedPhaseRadians[pixelIndex(source, 20, 10)] = 0.5f;
    source.leftMagnitudeDb[pixelIndex(source, 21, 11)] = -3.0f;

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(20.0, config);
    bounds.endTimeSeconds = frameIndexToTime(21.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(11.0f, config);

    const Clip clip = captureClip(source, bounds);

    REQUIRE(clip.frameCount == 2);
    REQUIRE(clip.binCount == 2);
    // clip[bin=0][frame=0] is source's (frame 20, bin 10) - the bounds' own low corner.
    REQUIRE(clip.leftMagnitudeDb[0] == -1.0f);
    REQUIRE(clip.rightMagnitudeDb[0] == -2.0f);
    REQUIRE(clip.sharedPhaseRadians[0] == 0.5f);
    // clip[bin=1][frame=1] is source's (frame 21, bin 11).
    REQUIRE(clip.leftMagnitudeDb[3] == -3.0f);
}

TEST_CASE("captureClip returns an empty clip for a degenerate (zero-sized) source buffer",
          "[core][paste_application]") {
    StreamImage source;
    source.config = makeTestConfig();
    source.frameCount = 0;

    const Clip clip = captureClip(source, TimeFrequencyRect{});

    REQUIRE(clip.frameCount == 0);
    REQUIRE(clip.binCount == 0);
    REQUIRE(clip.leftMagnitudeDb.empty());
}

TEST_CASE("applyPasteOperation overwrites destination cells directly from the clip, leaving cells outside its "
          "placement unchanged",
          "[core][paste_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    content.leftMagnitudeDb[pixelIndex(content, 60, 10)] = -99.0f;  // will be overwritten.
    content.leftMagnitudeDb[pixelIndex(content, 0, 0)] = -42.0f;    // outside placement - must survive.

    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-5.0f, -6.0f, -7.0f, -8.0f};
    clip.sharedPhaseRadians = {0.1f, 0.2f, 0.3f, 0.4f};

    TimeFrequencyRect placement;
    placement.startTimeSeconds = frameIndexToTime(60.0, config);
    placement.endTimeSeconds = frameIndexToTime(61.0, config);
    placement.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    placement.highFrequencyHz = binIndexToFrequency(11.0f, config);
    const PasteOperation op(1, LayerId{1}, placement, clip);

    applyPasteOperation(op, content);

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 60, 10)] == -1.0f);
    REQUIRE(content.rightMagnitudeDb[pixelIndex(content, 60, 10)] == -5.0f);
    REQUIRE(content.sharedPhaseRadians[pixelIndex(content, 60, 10)] == Catch::Approx(0.1f));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 61, 11)] == -4.0f);

    // Outside the placement - untouched.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 0, 0)] == -42.0f);
}

TEST_CASE("applyPasteOperation silently clips whatever part of the clip falls outside the destination's own range",
          "[core][paste_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);

    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.sharedPhaseRadians = {0.0f, 0.0f, 0.0f, 0.0f};

    // A placement whose own low corner sits at the destination's last
    // valid index (99), so the clip's own second frame/bin (index 1) would
    // land one past it - out of the destination's own valid range.
    TimeFrequencyRect placement;
    placement.startTimeSeconds = frameIndexToTime(99.0, config);
    placement.endTimeSeconds = frameIndexToTime(99.0, config);
    placement.lowFrequencyHz = binIndexToFrequency(99.0f, config);
    placement.highFrequencyHz = binIndexToFrequency(99.0f, config);
    const PasteOperation op(1, LayerId{1}, placement, clip);

    applyPasteOperation(op, content);  // must not crash.

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 99, 99)] == -1.0f);
}

TEST_CASE("applyPasteOperation does nothing for a degenerate (zero-sized) content buffer or clip",
          "[core][paste_application]") {
    StreamImage content;
    content.config = makeTestConfig();
    content.frameCount = 0;

    const PasteOperation op(1, LayerId{1}, TimeFrequencyRect{}, Clip{});

    applyPasteOperation(op, content);  // must not crash.

    REQUIRE(content.leftMagnitudeDb.empty());
}

TEST_CASE("captureClip followed by applyPasteOperation at the same bounds reproduces the original content",
          "[core][paste_application]") {
    const auto config = makeTestConfig();
    StreamImage source = makeBlankContent(config, 100);
    for (int frame = 20; frame <= 25; ++frame) {
        for (int bin = 5; bin <= 15; ++bin) {
            source.leftMagnitudeDb[pixelIndex(source, frame, bin)] = static_cast<float>(frame + bin);
        }
    }

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(20.0, config);
    bounds.endTimeSeconds = frameIndexToTime(25.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(5.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(15.0f, config);

    const Clip clip = captureClip(source, bounds);

    StreamImage destination = makeBlankContent(config, 100);
    const PasteOperation op(1, LayerId{2}, bounds, clip);
    applyPasteOperation(op, destination);

    for (int frame = 20; frame <= 25; ++frame) {
        for (int bin = 5; bin <= 15; ++bin) {
            REQUIRE(destination.leftMagnitudeDb[pixelIndex(destination, frame, bin)] ==
                    Catch::Approx(static_cast<float>(frame + bin)));
        }
    }
}
