#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::ToolConfiguration;

namespace {

Path makeTestPath() {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{0.0, 100.0};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{1.0, 500.0};
    end.type = PathNodeType::Corner;
    path.addNode(end);
    return path;
}

/// @brief Matching test_paint_application.cpp's own makeTestConfig() -
/// a real config translatedCopy()/Path::translated() need to interpret
/// each node's own frequency against (see those methods' own docs on why
/// a bin-space delta, not a raw Hz one, needs one at all).
StreamCodecConfig makeTestConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;
    config.binCount = 100;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

}  // namespace

TEST_CASE("PaintOperation reports the id, target layer, path, and config it was constructed with",
          "[core][paint_operation]") {
    const PaintOperation op(7, LayerId{3}, makeTestPath(), ToolConfiguration{});
    REQUIRE(op.id() == OperationId{7});
    REQUIRE(op.targetLayer().has_value());
    REQUIRE(op.targetLayer().value() == LayerId{3});
    REQUIRE(op.path().nodes().size() == 2);
}

TEST_CASE("PaintOperation has no supersedes reference unless one is given", "[core][paint_operation]") {
    const PaintOperation op(1, LayerId{1}, makeTestPath(), ToolConfiguration{});
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("PaintOperation can record which prior operation it supersedes", "[core][paint_operation]") {
    const PaintOperation op(2, LayerId{1}, makeTestPath(), ToolConfiguration{}, OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("PaintOperation's bounds() is exactly its own Path's bounds()", "[core][paint_operation]") {
    const Path path = makeTestPath();
    const PaintOperation op(1, LayerId{1}, path, ToolConfiguration{});
    const auto opBounds = op.bounds();
    const auto pathBounds = path.bounds();
    REQUIRE(opBounds.startTimeSeconds == pathBounds.startTimeSeconds);
    REQUIRE(opBounds.endTimeSeconds == pathBounds.endTimeSeconds);
    REQUIRE(opBounds.lowFrequencyHz == pathBounds.lowFrequencyHz);
    REQUIRE(opBounds.highFrequencyHz == pathBounds.highFrequencyHz);
}

TEST_CASE("PaintOperation carries its own tool configuration", "[core][paint_operation]") {
    ToolConfiguration config;
    config.setName("My Brush");
    const PaintOperation op(1, LayerId{1}, makeTestPath(), config);
    REQUIRE(op.config().name() == "My Brush");
}

TEST_CASE("PaintOperation::translatedCopy() shifts the path, keeps everything else, and supersedes the original",
          "[core][paint_operation]") {
    ToolConfiguration config;
    config.setName("My Brush");
    const PaintOperation original(5, LayerId{2}, makeTestPath(), config);
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.5, 5.0, codecConfig);

    REQUIRE(copy != nullptr);
    REQUIRE(copy->id() == OperationId{9});
    REQUIRE(copy->supersedes().has_value());
    REQUIRE(*copy->supersedes() == OperationId{5});
    REQUIRE(copy->targetLayer() == LayerId{2});

    // Each node's own frequency shifts by the same *bin* delta, not the
    // same Hz amount - see Path::translated()'s own docs. Computed via
    // the real conversion functions, not a hand-derived Hz number, since
    // the log-scaled math isn't meant to be reproduced by hand here.
    const float expectedFrequency0 = sound_mind::core::binIndexToFrequency(
        sound_mind::core::frequencyToBinIndex(100.0f, codecConfig) + 5.0f, codecConfig);
    const float expectedFrequency1 = sound_mind::core::binIndexToFrequency(
        sound_mind::core::frequencyToBinIndex(500.0f, codecConfig) + 5.0f, codecConfig);

    const auto* paintCopy = dynamic_cast<const PaintOperation*>(copy.get());
    REQUIRE(paintCopy != nullptr);
    REQUIRE(paintCopy->config().name() == "My Brush");
    REQUIRE(paintCopy->path().nodes().at(0).anchor.timeSeconds == 0.5);
    REQUIRE(paintCopy->path().nodes().at(0).anchor.frequencyHz == Catch::Approx(expectedFrequency0));
    REQUIRE(paintCopy->path().nodes().at(1).anchor.timeSeconds == 1.5);
    REQUIRE(paintCopy->path().nodes().at(1).anchor.frequencyHz == Catch::Approx(expectedFrequency1));

    // The original is untouched - translatedCopy() never mutates.
    REQUIRE(original.path().nodes().at(0).anchor.timeSeconds == 0.0);
}
