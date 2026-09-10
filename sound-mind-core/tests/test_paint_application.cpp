#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/paint_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyPaintOperation;
using sound_mind::core::BrushTipShape;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::LayerId;
using sound_mind::core::Operation;
using sound_mind::core::OperationId;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::rebuildPaintedContent;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ToolConfiguration;
using sound_mind::core::timeToFrameIndex;

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

/// @brief A horizontal straight-line Path (constant frequency) from
/// (startTime, frequencyHz) to (endTime, frequencyHz), with a uniform
/// gradient of the given intensity/opacity (same value at both channels,
/// both endpoints).
Path makeUniformHorizontalPath(double startTime, double endTime, double frequencyHz, float intensity,
                                float opacity) {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{startTime, frequencyHz};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{endTime, frequencyHz};
    end.type = PathNodeType::Corner;
    path.addNode(end);

    auto stop = path.gradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = opacity;
    stop.rightOpacity = opacity;
    path.gradient().setStopValues(0, stop);
    path.gradient().setStopValues(1, stop);
    return path;
}

ToolConfiguration makeCircleTool(double size, float falloff) {
    ToolConfiguration config;
    config.setTipShape(BrushTipShape::Circle);
    config.setFalloff(falloff);
    config.setSize(size);
    return config;
}

std::size_t pixelIndex(const StreamImage& content, int frame, int bin) {
    return static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
}

/// @brief A minimal non-Paint Operation, for exercising rebuildPaintedContent()'s
/// own dispatch-skip behavior - Operation itself has no other real subtype yet.
class FakeOperation final : public Operation {
public:
    explicit FakeOperation(OperationId id) : Operation(id) {}
    [[nodiscard]] TimeFrequencyRect bounds() const override { return TimeFrequencyRect{}; }
};

}  // namespace

TEST_CASE("frequencyToBinIndex maps minFrequencyHz to bin 0", "[core][paint_application]") {
    const auto config = makeTestConfig();
    REQUIRE(frequencyToBinIndex(config.minFrequencyHz, config) == 0.0f);
}

TEST_CASE("frequencyToBinIndex maps maxFrequencyHz to the last bin", "[core][paint_application]") {
    const auto config = makeTestConfig();
    const float index = frequencyToBinIndex(config.maxFrequencyHz, config);
    REQUIRE(index > 98.0f);
    REQUIRE(index <= 99.0f);
}

TEST_CASE("frequencyToBinIndex clamps below minFrequencyHz and above maxFrequencyHz", "[core][paint_application]") {
    const auto config = makeTestConfig();
    REQUIRE(frequencyToBinIndex(1.0f, config) == 0.0f);
    REQUIRE(frequencyToBinIndex(1000000.0f, config) == frequencyToBinIndex(config.maxFrequencyHz, config));
}

TEST_CASE("timeToFrameIndex is 0 at time 0", "[core][paint_application]") {
    const auto config = makeTestConfig();
    REQUIRE(timeToFrameIndex(0.0, config) == 0.0);
}

TEST_CASE("timeToFrameIndex scales linearly with sampleRateHz/hopLength", "[core][paint_application]") {
    const auto config = makeTestConfig();
    // 441 samples/frame at 44100 Hz = exactly 100 frames/second.
    REQUIRE(timeToFrameIndex(1.0, config) == 100.0);
    REQUIRE(timeToFrameIndex(0.5, config) == 50.0);
}

TEST_CASE("applyPaintOperation with a fresh (transparent) gradient leaves content untouched",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    Path path;  // fresh Path -> fresh, fully transparent gradient.
    path.addNode(PathNode{TimeFrequencyPoint{0.1, 1000.0}, PathNodeType::Corner, std::nullopt, std::nullopt});
    path.addNode(PathNode{TimeFrequencyPoint{0.5, 1000.0}, PathNodeType::Corner, std::nullopt, std::nullopt});

    const PaintOperation op(1, LayerId{1}, path, makeCircleTool(0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("applyPaintOperation with full opacity paints exactly the target intensity at the stroke's own center",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeUniformHorizontalPath(0.1, 0.5, 1000.0, /*intensity=*/-10.0f, /*opacity=*/1.0f);

    const PaintOperation op(1, LayerId{1}, path, makeCircleTool(0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));  // path's own midpoint.
    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, centerBin)] == -10.0f);
    REQUIRE(content.rightMagnitudeDb[pixelIndex(content, centerFrame, centerBin)] == -10.0f);
}

TEST_CASE("applyPaintOperation leaves pixels far outside the tip's own radius untouched",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -10.0f, 1.0f);

    const PaintOperation op(1, LayerId{1}, path, makeCircleTool(0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    // Frame 90 (0.9s) is far past the stroke's own end (0.5s) plus its radius.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 90, static_cast<int>(frequencyToBinIndex(1000.0f, config)))] ==
            0.0f);
    // Bin 5 (near minFrequencyHz) is far below the stroke's own 1000 Hz plus its radius.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, static_cast<int>(timeToFrameIndex(0.3, config)), 5)] == 0.0f);
}

TEST_CASE("applyPaintOperation blends partially when opacity is between 0 and 1", "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeUniformHorizontalPath(0.1, 0.5, 1000.0, /*intensity=*/-10.0f, /*opacity=*/0.5f);

    const PaintOperation op(1, LayerId{1}, path, makeCircleTool(0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));

    // Starting from 0 dB, blending 50% toward -10 dB lands at -5 dB.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, centerBin)] == -5.0f);
}

TEST_CASE("applyPaintOperation does nothing for a non-positive frequencyToTimeScale", "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -10.0f, 1.0f);
    const PaintOperation op(1, LayerId{1}, path, makeCircleTool(0.05, 0.0f));

    applyPaintOperation(op, 0.0, content);
    applyPaintOperation(op, -5.0, content);

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("applyPaintOperation does nothing for a path with fewer than two nodes", "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    Path path;  // no nodes at all.
    const PaintOperation op(1, LayerId{1}, path, makeCircleTool(0.05, 0.0f));

    applyPaintOperation(op, 2000.0, content);

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("rebuildPaintedContent applies every PaintOperation in order, on top of a copy of base",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    const StreamImage base = makeBlankContent(config, 100);

    const Path firstStroke = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -10.0f, 1.0f);
    const PaintOperation first(1, LayerId{1}, firstStroke, makeCircleTool(0.05, 0.0f));
    const Path secondStroke = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -20.0f, 1.0f);
    const PaintOperation second(2, LayerId{1}, secondStroke, makeCircleTool(0.05, 0.0f));

    const std::vector<const Operation*> operations = {&first, &second};
    const StreamImage rebuilt = rebuildPaintedContent(base, operations, 2000.0);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    // The second stroke (fully opaque) overwrites the first's own effect.
    REQUIRE(rebuilt.leftMagnitudeDb[pixelIndex(rebuilt, centerFrame, centerBin)] == -20.0f);

    // base itself is untouched - rebuildPaintedContent works on its own copy.
    for (const float value : base.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("rebuildPaintedContent skips any operation that isn't a PaintOperation", "[core][paint_application]") {
    const auto config = makeTestConfig();
    const StreamImage base = makeBlankContent(config, 100);
    const FakeOperation fake(1);

    const std::vector<const Operation*> operations = {&fake};
    const StreamImage rebuilt = rebuildPaintedContent(base, operations, 2000.0);

    for (const float value : rebuilt.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("rebuildPaintedContent with no operations returns an unchanged copy of base",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage base = makeBlankContent(config, 100);
    base.leftMagnitudeDb[42] = -33.0f;

    const StreamImage rebuilt = rebuildPaintedContent(base, {}, 2000.0);

    REQUIRE(rebuilt.leftMagnitudeDb[42] == -33.0f);
}
