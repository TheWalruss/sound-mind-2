#include <cstddef>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/warp_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyWarpOperation;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::frameIndexToTime;
using sound_mind::core::LayerId;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::WarpAxis;
using sound_mind::core::WarpMode;
using sound_mind::core::WarpOperation;

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

TimeFrequencyRect makeTestBounds(const StreamCodecConfig& config) {
    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(5.0, config);
    bounds.endTimeSeconds = frameIndexToTime(25.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(90.0f, config);
    return bounds;
}

/// @brief A curve holding a constant +10-bin deflection across frames
/// [5, 25]: flat at bin 20 (the baseline) up to frame 4, then flat at bin
/// 30 from frame 4 through frame 30 - deliberately chosen so every frame
/// in the test's own selected range sees the exact same raw deflection
/// (10 bins), isolating Stretch mode's own per-column ramp (based on the
/// column's own position in the range) from the curve's own raw
/// deflection value (which stays constant here) - see this test file's
/// own test cases for how that distinguishes Displace from Stretch
/// cleanly, with no fractional/bilinear blending to reason about.
Path makeConstantDeflectionCurve(const StreamCodecConfig& config) {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{frameIndexToTime(0.0, config), binIndexToFrequency(20.0f, config)};
    start.type = PathNodeType::Corner;
    PathNode transition;
    transition.anchor = TimeFrequencyPoint{frameIndexToTime(4.0, config), binIndexToFrequency(30.0f, config)};
    transition.type = PathNodeType::Corner;
    PathNode end;
    end.anchor = TimeFrequencyPoint{frameIndexToTime(30.0, config), binIndexToFrequency(30.0f, config)};
    end.type = PathNodeType::Corner;
    path.addNode(start);
    path.addNode(transition);
    path.addNode(end);
    return path;
}

}  // namespace

TEST_CASE("applyWarpOperation Displace mode shifts every covered column by the curve's own full deflection",
          "[core][warp_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 30);
    content.leftMagnitudeDb[pixelIndex(content, 5, 50)] = -10.0f;
    content.leftMagnitudeDb[pixelIndex(content, 15, 50)] = -10.0f;
    content.leftMagnitudeDb[pixelIndex(content, 25, 50)] = -10.0f;

    const WarpOperation op(1, LayerId{1}, makeTestBounds(config), makeConstantDeflectionCurve(config),
                            WarpAxis::Frequency, WarpMode::Displace);
    applyWarpOperation(op, content);

    // Every marker moved by the same +10 bins, regardless of its own
    // column's position within the selection.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 5, 60)] == Catch::Approx(-10.0f));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 15, 60)] == Catch::Approx(-10.0f));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 25, 60)] == Catch::Approx(-10.0f));
    // The marker's own old position (50) isn't necessarily silent - it
    // gets refilled by whatever shifted in from its own new source (bin
    // 40, still blank here). The selection's own low *edge* (bins
    // [10, 19], for a uniform +10 shift over [10, 90]) has no valid
    // in-selection source at all (source would be bins [0, 9], outside
    // the selection) - genuinely, unconditionally silenced.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 5, 10)] == Catch::Approx(-96.0f));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 15, 10)] == Catch::Approx(-96.0f));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 25, 10)] == Catch::Approx(-96.0f));
}

TEST_CASE("applyWarpOperation Stretch mode ramps the deflection by each column's own position across the range",
          "[core][warp_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 30);
    content.leftMagnitudeDb[pixelIndex(content, 5, 50)] = -10.0f;   // first column in range.
    content.leftMagnitudeDb[pixelIndex(content, 15, 50)] = -10.0f;  // midpoint.
    content.leftMagnitudeDb[pixelIndex(content, 25, 50)] = -10.0f;  // last column in range.

    const WarpOperation op(1, LayerId{1}, makeTestBounds(config), makeConstantDeflectionCurve(config),
                            WarpAxis::Frequency, WarpMode::Stretch);
    applyWarpOperation(op, content);

    // First column: scale 0/(25-5) = 0 - unaffected, despite the curve's
    // own raw +10-bin deflection there (unlike Displace, which moved it).
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 5, 50)] == Catch::Approx(-10.0f));
    // Midpoint: scale (15-5)/(25-5) = 0.5 - moved by exactly +5 bins.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 15, 55)] == Catch::Approx(-10.0f));
    // Last column: scale (25-5)/(25-5) = 1 - the full +10 bins, matching
    // Displace's own result there exactly.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 25, 60)] == Catch::Approx(-10.0f));
}

TEST_CASE("applyWarpOperation Time axis shifts every covered row horizontally instead of vertically",
          "[core][warp_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 40);
    content.leftMagnitudeDb[pixelIndex(content, 15, 50)] = -10.0f;

    // A curve whose *bin* axis plays the "column" role for Time-axis
    // warping - flat at frame 5 up to bin 45, then flat at frame 15 from
    // bin 49 onward, giving a constant +10-frame deflection across the
    // bin range this test's own marker sits within.
    Path curve;
    PathNode start;
    start.anchor = TimeFrequencyPoint{frameIndexToTime(5.0, config), binIndexToFrequency(0.0f, config)};
    start.type = PathNodeType::Corner;
    PathNode transition;
    transition.anchor = TimeFrequencyPoint{frameIndexToTime(15.0, config), binIndexToFrequency(45.0f, config)};
    transition.type = PathNodeType::Corner;
    PathNode end;
    end.anchor = TimeFrequencyPoint{frameIndexToTime(15.0, config), binIndexToFrequency(99.0f, config)};
    end.type = PathNodeType::Corner;
    curve.addNode(start);
    curve.addNode(transition);
    curve.addNode(end);

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(0.0, config);
    bounds.endTimeSeconds = frameIndexToTime(39.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(0.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(99.0f, config);

    const WarpOperation op(1, LayerId{1}, bounds, curve, WarpAxis::Time, WarpMode::Displace);
    applyWarpOperation(op, content);

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 25, 50)] == Catch::Approx(-10.0f));
    // The selection's own low edge (frames [0, 9], for a uniform
    // +10-frame shift over [0, 39]) has no valid in-selection source
    // (source would be frames [-10, -1]) - genuinely silenced.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 0, 50)] == Catch::Approx(-96.0f));
}

TEST_CASE("applyWarpOperation leaves a column the curve never reaches untouched", "[core][warp_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 30);
    content.leftMagnitudeDb[pixelIndex(content, 0, 50)] = -10.0f;   // outside the curve's own span.
    content.leftMagnitudeDb[pixelIndex(content, 25, 50)] = -20.0f;  // inside it.

    Path curve;  // Only spans frames [4, 30] - never reaches frame 0.
    PathNode start;
    start.anchor = TimeFrequencyPoint{frameIndexToTime(4.0, config), binIndexToFrequency(20.0f, config)};
    start.type = PathNodeType::Corner;
    PathNode end;
    end.anchor = TimeFrequencyPoint{frameIndexToTime(30.0, config), binIndexToFrequency(30.0f, config)};
    end.type = PathNodeType::Corner;
    curve.addNode(start);
    curve.addNode(end);

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(0.0, config);  // Includes frames 0-3, outside the curve's own span.
    bounds.endTimeSeconds = frameIndexToTime(29.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(90.0f, config);
    const WarpOperation op(1, LayerId{1}, bounds, curve, WarpAxis::Frequency, WarpMode::Displace);

    applyWarpOperation(op, content);

    // Frame 0 is outside the curve's own [4, 30] span - "zero deflection",
    // left completely untouched (not even silenced).
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 0, 50)] == Catch::Approx(-10.0f));
    // Frame 25 is covered by the curve, so its own marker actually moved
    // away from its original -20.0f value, proving the operation ran
    // there at all (the curve's own fractional deflection at this frame
    // isn't a clean bin count, so this only checks "changed", not an
    // exact destination).
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 25, 50)] != Catch::Approx(-20.0f));
}

TEST_CASE("applyWarpOperation does nothing for a curve with fewer than 2 nodes", "[core][warp_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 30);
    content.leftMagnitudeDb[pixelIndex(content, 5, 50)] = -10.0f;

    Path curve;
    PathNode onlyNode;
    onlyNode.anchor = TimeFrequencyPoint{frameIndexToTime(5.0, config), binIndexToFrequency(50.0f, config)};
    curve.addNode(onlyNode);

    const WarpOperation op(1, LayerId{1}, makeTestBounds(config), curve, WarpAxis::Frequency, WarpMode::Displace);
    applyWarpOperation(op, content);

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 5, 50)] == Catch::Approx(-10.0f));  // untouched.
}

TEST_CASE("applyWarpOperation does nothing for a degenerate (zero-sized) content buffer",
          "[core][warp_application]") {
    StreamImage content;
    content.config = makeTestConfig();
    content.frameCount = 0;

    const WarpOperation op(1, LayerId{1}, TimeFrequencyRect{}, makeConstantDeflectionCurve(makeTestConfig()),
                            WarpAxis::Frequency, WarpMode::Displace);
    applyWarpOperation(op, content);  // must not crash.

    REQUIRE(content.leftMagnitudeDb.empty());
}
