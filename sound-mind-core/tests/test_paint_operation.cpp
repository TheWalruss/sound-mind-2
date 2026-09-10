#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/paint_operation.h"

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
