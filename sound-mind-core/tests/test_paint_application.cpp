#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paste_operation.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyPaintOperation;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::BrushTipShape;
using sound_mind::core::Clip;
using sound_mind::core::frameIndexToTime;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::InstrumentConfiguration;
using sound_mind::core::LayerContentResolver;
using sound_mind::core::LayerId;
using sound_mind::core::MindGrainConfiguration;
using sound_mind::core::MindShotConfiguration;
using sound_mind::core::Operation;
using sound_mind::core::OperationId;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::rebuildPaintedContent;
using sound_mind::core::StampMode;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ProceduralConfiguration;
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

/// @brief A straight-line Path between two arbitrary (time, frequency)
/// points, with a uniform gradient of the given intensity/opacity - the
/// diagonal counterpart to makeUniformHorizontalPath(), for stamp-mode
/// tests that need frequency to vary along the path too.
Path makeUniformDiagonalPath(TimeFrequencyPoint start, TimeFrequencyPoint end, float intensity, float opacity) {
    Path path;
    PathNode startNode;
    startNode.anchor = start;
    startNode.type = PathNodeType::Corner;
    path.addNode(startNode);
    PathNode endNode;
    endNode.anchor = end;
    endNode.type = PathNodeType::Corner;
    path.addNode(endNode);

    auto stop = path.gradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = opacity;
    stop.rightOpacity = opacity;
    path.gradient().setStopValues(0, stop);
    path.gradient().setStopValues(1, stop);
    return path;
}

std::unique_ptr<ProceduralConfiguration> makeCircleTool(double size, float falloff) {
    auto config = std::make_unique<ProceduralConfiguration>();
    config->setTipShape(BrushTipShape::Circle);
    config->setFalloff(falloff);
    config->setSize(size);
    return config;
}

std::unique_ptr<InstrumentConfiguration> makeInstrumentTool(std::vector<double> harmonicStrengths,
                                                              double inharmonicity, double size, float falloff) {
    auto config = std::make_unique<InstrumentConfiguration>();
    config->setHarmonicStrengths(std::move(harmonicStrengths));
    config->setInharmonicity(inharmonicity);
    config->setSize(size);
    config->setFalloff(falloff);
    return config;
}

/// @brief A single-node Path (a tap, not a drag) at (timeSeconds,
/// frequencyHz), with a uniform gradient of the given intensity/opacity -
/// exercises applyPaintOperation()'s own single-stamp handling without any
/// dense-resampling noise a real (even zero-length) two-node segment would
/// introduce.
Path makeSingleTapPath(double timeSeconds, double frequencyHz, float intensity, float opacity) {
    Path path;
    PathNode node;
    node.anchor = TimeFrequencyPoint{timeSeconds, frequencyHz};
    node.type = PathNodeType::Corner;
    path.addNode(node);

    auto stop = path.gradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = opacity;
    stop.rightOpacity = opacity;
    path.gradient().setStopValues(0, stop);
    path.gradient().setStopValues(1, stop);
    return path;
}

std::unique_ptr<MindGrainConfiguration> makeMindGrainTool(LayerId sourceLayer, TimeFrequencyRect bounds) {
    auto config = std::make_unique<MindGrainConfiguration>();
    config->setReference(std::nullopt, sourceLayer, bounds);
    return config;
}

std::unique_ptr<MindShotConfiguration> makeMindShotTool(Clip clip) {
    auto config = std::make_unique<MindShotConfiguration>();
    config->setClip(sound_mind::core::MindShotId{1}, std::move(clip));
    return config;
}

std::size_t pixelIndex(const StreamImage& content, int frame, int bin) {
    return static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
}

/// @brief A minimal Operation that isn't a PaintOperation or a
/// FillOperation, for exercising rebuildPaintedContent()'s own
/// dispatch-skip behavior on a genuinely unrecognized subtype.
class FakeOperation final : public Operation {
public:
    explicit FakeOperation(OperationId id) : Operation(id) {}
    [[nodiscard]] TimeFrequencyRect bounds() const override { return TimeFrequencyRect{}; }
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(OperationId newId, double, double,
                                                              const sound_mind::codec::StreamCodecConfig&) const override {
        return std::make_unique<FakeOperation>(newId);
    }
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

TEST_CASE("binIndexToFrequency is the inverse of frequencyToBinIndex", "[core][paint_application]") {
    const auto config = makeTestConfig();
    for (const float frequencyHz : {20.0f, 100.0f, 1000.0f, 10000.0f, 19999.0f}) {
        const float index = frequencyToBinIndex(frequencyHz, config);
        const float roundTripped = binIndexToFrequency(index, config);
        REQUIRE(std::abs(roundTripped - frequencyHz) < 0.5f);
    }
}

TEST_CASE("binIndexToFrequency maps bin 0 to minFrequencyHz and the last bin to maxFrequencyHz",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    REQUIRE(binIndexToFrequency(0.0f, config) == config.minFrequencyHz);
    REQUIRE(std::abs(binIndexToFrequency(static_cast<float>(config.binCount - 1), config) - config.maxFrequencyHz) <
            1.0f);
}

TEST_CASE("frameIndexToTime is the inverse of timeToFrameIndex", "[core][paint_application]") {
    const auto config = makeTestConfig();
    for (const double timeSeconds : {0.0, 0.25, 1.0, 5.0}) {
        const double index = timeToFrameIndex(timeSeconds, config);
        REQUIRE(frameIndexToTime(index, config) == Catch::Approx(timeSeconds));
    }
}

TEST_CASE("frameIndexToTime is 0 at frame 0", "[core][paint_application]") {
    const auto config = makeTestConfig();
    REQUIRE(frameIndexToTime(0.0, config) == 0.0);
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

TEST_CASE("applyPaintOperation with AlongCurve stamp mode leaves gaps between stamps", "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -10.0f, 1.0f);  // 0.4s span, constant frequency.

    auto tool = makeCircleTool(0.02, 0.0f);
    tool->setStampMode(StampMode::AlongCurve);
    // Seconds-equivalent arc length == plain seconds here, since this
    // path has zero frequency variation to contribute any normalized-
    // frequency distance.
    tool->setStampInterval(0.1);

    const PaintOperation op(1, LayerId{1}, path, std::move(tool));
    applyPaintOperation(op, 2000.0, content);

    const int bin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    for (const double t : {0.1, 0.2, 0.3, 0.4, 0.5}) {
        const int frame = static_cast<int>(std::lround(timeToFrameIndex(t, config)));
        REQUIRE(content.leftMagnitudeDb[pixelIndex(content, frame, bin)] == -10.0f);
    }
    // Halfway between two stamps, well outside the 0.02s brush radius.
    const int midFrame = static_cast<int>(std::lround(timeToFrameIndex(0.15, config)));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, midFrame, bin)] == 0.0f);
}

TEST_CASE("applyPaintOperation with TimeAxis stamp mode stamps at each time-grid crossing",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path =
        makeUniformDiagonalPath(TimeFrequencyPoint{0.1, 400.0}, TimeFrequencyPoint{0.9, 1200.0}, -10.0f, 1.0f);

    // Small enough that gaps are clearly untouched, but not *so* small
    // that its own bin-radius (log-scaled - see frequencyToBinIndex()'s
    // own docs, and applyPaintOperation()'s "Local bin-radius" comment)
    // shrinks below reaching even the nearest integer bin at this path's
    // own highest frequency (1200 Hz) - which 0.01 alone did.
    auto tool = makeCircleTool(0.03, 0.0f);
    tool->setStampMode(StampMode::TimeAxis);
    tool->setStampInterval(0.3);

    const PaintOperation op(1, LayerId{1}, path, std::move(tool));
    applyPaintOperation(op, 2000.0, content);

    // The path's own start (t=0.1, always stamped), plus every 0.3s
    // crossing after it (t=0.4, t=0.7) - each at the frequency the
    // straight path itself passes through at that time.
    for (const auto& [t, frequencyHz] : {std::pair{0.1, 400.0}, std::pair{0.4, 700.0}, std::pair{0.7, 1000.0}}) {
        const int frame = static_cast<int>(std::lround(timeToFrameIndex(t, config)));
        const int bin =
            static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(static_cast<float>(frequencyHz), config))));
        REQUIRE(content.leftMagnitudeDb[pixelIndex(content, frame, bin)] == -10.0f);
    }
    // Halfway between the first two stamps (t=0.25s) - the path's own
    // frequency there (550 Hz) stays untouched.
    const int midFrame = static_cast<int>(std::lround(timeToFrameIndex(0.25, config)));
    const int midBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(550.0f, config))));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, midFrame, midBin)] == 0.0f);
}

TEST_CASE("applyPaintOperation with FrequencyAxis stamp mode stamps at each frequency-grid crossing",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path =
        makeUniformDiagonalPath(TimeFrequencyPoint{0.1, 400.0}, TimeFrequencyPoint{0.9, 1200.0}, -10.0f, 1.0f);

    // See the TimeAxis test above for why this can't be too small.
    auto tool = makeCircleTool(0.03, 0.0f);
    tool->setStampMode(StampMode::FrequencyAxis);
    tool->setStampInterval(200.0);

    const PaintOperation op(1, LayerId{1}, path, std::move(tool));
    applyPaintOperation(op, 2000.0, content);

    // The path's own start (freq 400 Hz, always stamped), plus every
    // 200 Hz crossing after it (600, 800, 1000, 1200), each at the time
    // the straight path itself passes through that frequency.
    for (const auto& [frequencyHz, t] :
         {std::pair{400.0, 0.1}, std::pair{600.0, 0.3}, std::pair{800.0, 0.5}, std::pair{1000.0, 0.7},
          std::pair{1200.0, 0.9}}) {
        const int frame = static_cast<int>(std::lround(timeToFrameIndex(t, config)));
        const int bin =
            static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(static_cast<float>(frequencyHz), config))));
        REQUIRE(content.leftMagnitudeDb[pixelIndex(content, frame, bin)] == -10.0f);
    }
    // Halfway between the first two stamps (500 Hz) - the path's own
    // time there (0.2s) stays untouched.
    const int midFrame = static_cast<int>(std::lround(timeToFrameIndex(0.2, config)));
    const int midBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(500.0f, config))));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, midFrame, midBin)] == 0.0f);
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

TEST_CASE("applyPaintOperation with an InstrumentConfiguration stamps one bin-exact spike per harmonic",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);

    const PaintOperation op(1, LayerId{1}, path, makeInstrumentTool({1.0, 0.5}, 0.0, 0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int fundamentalBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    const int secondHarmonicBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(2000.0f, config))));

    // Strength 1.0, starting from 0 dB: a full blend to the target.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, fundamentalBin)] == -10.0f);
    // Strength 0.5: exactly half the blend.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, secondHarmonicBin)] == Catch::Approx(-5.0f));

    // No frequency-axis blending - the bin immediately next to the
    // fundamental's own exact bin is untouched.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, fundamentalBin + 1)] == 0.0f);
}

TEST_CASE("applyPaintOperation with an InstrumentConfiguration blends each harmonic's own spike along the time axis "
          "only",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);

    const PaintOperation op(1, LayerId{1}, path, makeInstrumentTool({1.0}, 0.0, 0.05, 0.5f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int fundamentalBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, fundamentalBin)] == -10.0f);
    // A frame close to the edge of the brush radius (falloff 0.5, so the
    // outer half of the radius fades) is only partially blended - not the
    // full target value. frameRadius here is 5 frames (0.05s / 0.01s per
    // frame); offset 4 sits well within the fading outer half.
    const int nearFrame = centerFrame + 4;
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, nearFrame, fundamentalBin)] < 0.0f);
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, nearFrame, fundamentalBin)] > -10.0f);
    // Well outside the brush radius (0.05s), nothing is painted.
    const int farFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3 + 1.0, config)));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, farFrame, fundamentalBin)] == 0.0f);
}

TEST_CASE("applyPaintOperation with an InstrumentConfiguration skips a harmonic stretched past the configured "
          "frequency range",
          "[core][paint_application]") {
    const auto config = makeTestConfig();  // maxFrequencyHz == 20000.0f.
    StreamImage content = makeBlankContent(config, 100);
    // Fundamental at 15000 Hz: harmonic 1 (15000 Hz) is in range, harmonic
    // 2 (30000 Hz) is well past maxFrequencyHz - it must be skipped
    // entirely, not clamped and stacked onto the top bin.
    const Path path = makeSingleTapPath(0.3, 15000.0, -10.0f, 1.0f);

    const PaintOperation op(1, LayerId{1}, path, makeInstrumentTool({1.0, 1.0}, 0.0, 0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int topBin = static_cast<int>(config.binCount) - 1;
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, topBin)] == 0.0f);
}

TEST_CASE("applyPaintOperation with an InstrumentConfiguration stretches higher harmonics sharp per inharmonicity",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);

    // Only harmonic 4 has any strength, isolating its own placement.
    // harmonicHz = 4 * 1000 * sqrt(1 + 0.01 * 4^2) = 4000 * sqrt(1.16).
    const double inharmonicity = 0.01;
    const PaintOperation op(1, LayerId{1}, path, makeInstrumentTool({0.0, 0.0, 0.0, 1.0}, inharmonicity, 0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const double stretchedHz = 4.0 * 1000.0 * std::sqrt(1.0 + inharmonicity * 16.0);
    const int stretchedBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(static_cast<float>(stretchedHz), config))));
    const int unstretchedBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(4000.0f, config))));

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, stretchedBin)] == -10.0f);
    if (unstretchedBin != stretchedBin) {
        REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, unstretchedBin)] == 0.0f);
    }
}

TEST_CASE("applyPaintOperation with an InstrumentConfiguration skips a non-positive-strength harmonic",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);

    const PaintOperation op(1, LayerId{1}, path, makeInstrumentTool({0.0, 1.0}, 0.0, 0.05, 0.0f));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int fundamentalBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    const int secondHarmonicBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(2000.0f, config))));

    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, fundamentalBin)] == 0.0f);
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, secondHarmonicBin)] == -10.0f);
}

TEST_CASE("applyPaintOperation with a MindShotConfiguration blits the clip centered on the stamp position, verbatim",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    // Intensity/opacity are irrelevant to a Mind Shot stamp - it never
    // reads the path's own gradient.
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);

    Clip clip;
    clip.frameCount = 3;
    clip.binCount = 3;
    clip.leftMagnitudeDb.assign(9, -7.0f);
    clip.rightMagnitudeDb.assign(9, -8.0f);
    clip.sharedPhaseRadians.assign(9, 0.25f);

    const PaintOperation op(1, LayerId{1}, path, makeMindShotTool(clip));
    applyPaintOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));

    for (int dt = -1; dt <= 1; ++dt) {
        for (int df = -1; df <= 1; ++df) {
            const std::size_t index = pixelIndex(content, centerFrame + dt, centerBin + df);
            REQUIRE(content.leftMagnitudeDb[index] == -7.0f);
            REQUIRE(content.rightMagnitudeDb[index] == -8.0f);
            REQUIRE(content.sharedPhaseRadians[index] == 0.25f);
        }
    }
    // Just outside the 3x3 clip's own extent - untouched.
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame + 2, centerBin)] == 0.0f);
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, centerBin + 2)] == 0.0f);
}

TEST_CASE("applyPaintOperation with a MindShotConfiguration paints nothing when no Mind Shot has been configured",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);

    const PaintOperation op(1, LayerId{1}, path, std::make_unique<MindShotConfiguration>());
    applyPaintOperation(op, 2000.0, content);

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("applyPaintOperation with a MindShotConfiguration silently clips a stamp extending past the canvas edge",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    // Time 0 - a centered 3-wide clip needs one frame before frame 0,
    // which doesn't exist.
    const Path path = makeSingleTapPath(0.0, 1000.0, -10.0f, 1.0f);

    Clip clip;
    clip.frameCount = 3;
    clip.binCount = 1;
    clip.leftMagnitudeDb.assign(3, -5.0f);
    clip.rightMagnitudeDb.assign(3, -5.0f);
    clip.sharedPhaseRadians.assign(3, 0.0f);

    const PaintOperation op(1, LayerId{1}, path, makeMindShotTool(clip));

    REQUIRE_NOTHROW(applyPaintOperation(op, 2000.0, content));

    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, 0, centerBin)] == -5.0f);
}

TEST_CASE("applyPaintOperation with a MindGrainConfiguration blits a clip resolved live from resolveLayerContent",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    // The whole source is one uniform value - see this test file's own
    // reasoning: makes the resulting clip's own exact size/position (which
    // rangeFor()'s rounding governs) irrelevant to what's being tested
    // here (that applyPaintOperation() actually reads through
    // resolveLayerContent() rather than needing its own pixel content).
    StreamImage source = makeBlankContent(config, 100);
    std::fill(source.leftMagnitudeDb.begin(), source.leftMagnitudeDb.end(), -7.0f);
    std::fill(source.rightMagnitudeDb.begin(), source.rightMagnitudeDb.end(), -8.0f);

    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);
    const TimeFrequencyRect bounds{0.1, 0.5, 500.0, 2000.0};
    const PaintOperation op(1, LayerId{1}, path, makeMindGrainTool(LayerId{2}, bounds));

    const LayerContentResolver resolve = [&source](LayerId id) -> const StreamImage* {
        return id == LayerId{2} ? &source : nullptr;
    };
    applyPaintOperation(op, 2000.0, content, resolve);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, centerBin)] == -7.0f);
    REQUIRE(content.rightMagnitudeDb[pixelIndex(content, centerFrame, centerBin)] == -8.0f);
}

TEST_CASE("applyPaintOperation with a MindGrainConfiguration re-reads the source fresh on every call, not a cached "
          "copy",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);
    const TimeFrequencyRect bounds{0.1, 0.5, 500.0, 2000.0};
    const PaintOperation op(1, LayerId{1}, path, makeMindGrainTool(LayerId{2}, bounds));
    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int centerBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));

    StreamImage sourceBefore = makeBlankContent(config, 100);
    std::fill(sourceBefore.leftMagnitudeDb.begin(), sourceBefore.leftMagnitudeDb.end(), -7.0f);
    StreamImage contentA = makeBlankContent(config, 100);
    applyPaintOperation(
        op, 2000.0, contentA, [&sourceBefore](LayerId) -> const StreamImage* { return &sourceBefore; });
    REQUIRE(contentA.leftMagnitudeDb[pixelIndex(contentA, centerFrame, centerBin)] == -7.0f);

    // The exact same operation, replayed against a *different* current
    // source (as if the source layer had since been repainted) - the new
    // stamp must reflect the new value, not -7.0f again.
    StreamImage sourceAfter = makeBlankContent(config, 100);
    std::fill(sourceAfter.leftMagnitudeDb.begin(), sourceAfter.leftMagnitudeDb.end(), -3.0f);
    StreamImage contentB = makeBlankContent(config, 100);
    applyPaintOperation(
        op, 2000.0, contentB, [&sourceAfter](LayerId) -> const StreamImage* { return &sourceAfter; });
    REQUIRE(contentB.leftMagnitudeDb[pixelIndex(contentB, centerFrame, centerBin)] == -3.0f);
}

TEST_CASE("applyPaintOperation with a MindGrainConfiguration paints nothing with the default (empty) resolver",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);
    const TimeFrequencyRect bounds{0.1, 0.5, 500.0, 2000.0};
    const PaintOperation op(1, LayerId{1}, path, makeMindGrainTool(LayerId{2}, bounds));

    // No resolver passed at all - the default parameter.
    applyPaintOperation(op, 2000.0, content);

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("applyPaintOperation with a MindGrainConfiguration paints nothing when the resolver finds no layer",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);
    const TimeFrequencyRect bounds{0.1, 0.5, 500.0, 2000.0};
    const PaintOperation op(1, LayerId{1}, path, makeMindGrainTool(LayerId{2}, bounds));

    applyPaintOperation(op, 2000.0, content, [](LayerId) -> const StreamImage* { return nullptr; });

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("applyPaintOperation with a MindGrainConfiguration paints nothing when the resolved layer has no content "
          "yet (a degenerate, zero-frame source)",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    // A resolvable but genuinely empty source - captureClip() returns an
    // empty Clip for this (see its own docs), the same as a layer that
    // was never actually painted/imported into yet.
    const StreamImage emptySource = makeBlankContent(config, 0);
    StreamImage content = makeBlankContent(config, 100);
    const Path path = makeSingleTapPath(0.3, 1000.0, -10.0f, 1.0f);
    const TimeFrequencyRect bounds{0.1, 0.5, 500.0, 2000.0};
    const PaintOperation op(1, LayerId{1}, path, makeMindGrainTool(LayerId{2}, bounds));

    applyPaintOperation(op, 2000.0, content, [&emptySource](LayerId) -> const StreamImage* { return &emptySource; });

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

TEST_CASE("rebuildPaintedContent skips any operation that isn't a recognized Operation subtype",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    const StreamImage base = makeBlankContent(config, 100);
    const FakeOperation fake(1);

    const std::vector<const Operation*> operations = {&fake};
    const StreamImage rebuilt = rebuildPaintedContent(base, operations, 2000.0);

    for (const float value : rebuilt.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("rebuildPaintedContent also applies a FillOperation, mixed in with PaintOperations",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    const StreamImage base = makeBlankContent(config, 100);

    const Path stroke = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -10.0f, 1.0f);
    const PaintOperation paint(1, LayerId{1}, stroke, makeCircleTool(0.05, 0.0f));

    sound_mind::core::TimeFrequencyRect fillBounds;
    fillBounds.startTimeSeconds = frameIndexToTime(60.0, config);
    fillBounds.endTimeSeconds = frameIndexToTime(80.0, config);
    fillBounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    fillBounds.highFrequencyHz = binIndexToFrequency(30.0f, config);
    sound_mind::core::Gradient fillGradient;
    auto stop = fillGradient.stops().front();
    stop.leftIntensity = -5.0f;
    stop.leftOpacity = 1.0f;
    fillGradient.setStopValues(0, stop);
    fillGradient.setStopValues(1, stop);
    const sound_mind::core::FillOperation fill(2, LayerId{1}, fillBounds, fillGradient);

    const std::vector<const Operation*> operations = {&paint, &fill};
    const StreamImage rebuilt = rebuildPaintedContent(base, operations, 2000.0);

    const int paintFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int paintBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    REQUIRE(rebuilt.leftMagnitudeDb[pixelIndex(rebuilt, paintFrame, paintBin)] == -10.0f);

    REQUIRE(rebuilt.leftMagnitudeDb[pixelIndex(rebuilt, 70, 20)] == Catch::Approx(-5.0f));
}

TEST_CASE("rebuildPaintedContent also applies a PasteOperation, mixed in with Paint/FillOperations",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    const StreamImage base = makeBlankContent(config, 100);

    const Path stroke = makeUniformHorizontalPath(0.1, 0.5, 1000.0, -10.0f, 1.0f);
    const PaintOperation paint(1, LayerId{1}, stroke, makeCircleTool(0.05, 0.0f));

    sound_mind::core::TimeFrequencyRect pasteBounds;
    pasteBounds.startTimeSeconds = frameIndexToTime(60.0, config);
    pasteBounds.endTimeSeconds = frameIndexToTime(61.0, config);
    pasteBounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    pasteBounds.highFrequencyHz = binIndexToFrequency(11.0f, config);

    sound_mind::core::Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.sharedPhaseRadians = {0.0f, 0.0f, 0.0f, 0.0f};
    const sound_mind::core::PasteOperation paste(2, LayerId{1}, pasteBounds, clip);

    const std::vector<const Operation*> operations = {&paint, &paste};
    const StreamImage rebuilt = rebuildPaintedContent(base, operations, 2000.0);

    const int paintFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int paintBin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    REQUIRE(rebuilt.leftMagnitudeDb[pixelIndex(rebuilt, paintFrame, paintBin)] == -10.0f);

    // The clip's own [bin=0][frame=0] corner lands at the paste bounds' own low corner (frame 60, bin 10).
    REQUIRE(rebuilt.leftMagnitudeDb[pixelIndex(rebuilt, 60, 10)] == -1.0f);
    REQUIRE(rebuilt.leftMagnitudeDb[pixelIndex(rebuilt, 61, 11)] == -4.0f);
}

TEST_CASE("rebuildPaintedContent with no operations returns an unchanged copy of base",
          "[core][paint_application]") {
    const auto config = makeTestConfig();
    StreamImage base = makeBlankContent(config, 100);
    base.leftMagnitudeDb[42] = -33.0f;

    const StreamImage rebuilt = rebuildPaintedContent(base, {}, 2000.0);

    REQUIRE(rebuilt.leftMagnitudeDb[42] == -33.0f);
}
