#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/warp_operation.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::WarpAxis;
using sound_mind::core::WarpMode;
using sound_mind::core::WarpOperation;

namespace {

TimeFrequencyRect makeTestBounds() {
    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = 0.2;
    bounds.endTimeSeconds = 0.5;
    bounds.lowFrequencyHz = 300.0;
    bounds.highFrequencyHz = 900.0;
    return bounds;
}

Path makeTestCurve() {
    Path path;
    PathNode a;
    a.anchor = TimeFrequencyPoint{0.2, 300.0};
    a.type = PathNodeType::Corner;
    PathNode b;
    b.anchor = TimeFrequencyPoint{0.5, 500.0};
    b.type = PathNodeType::Corner;
    path.addNode(a);
    path.addNode(b);
    return path;
}

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

TEST_CASE("WarpOperation reports the id, target layer, bounds, curve, axis, and mode it was constructed with",
          "[core][warp_operation]") {
    const WarpOperation op(9, LayerId{4}, makeTestBounds(), makeTestCurve(), WarpAxis::Frequency, WarpMode::Displace);
    REQUIRE(op.id() == OperationId{9});
    REQUIRE(op.targetLayer().has_value());
    REQUIRE(op.targetLayer().value() == LayerId{4});
    REQUIRE(op.bounds().startTimeSeconds == 0.2);
    REQUIRE(op.curve().nodes().size() == 2);
    REQUIRE(op.axis() == WarpAxis::Frequency);
    REQUIRE(op.mode() == WarpMode::Displace);
}

TEST_CASE("WarpOperation has no supersedes reference unless one is given", "[core][warp_operation]") {
    const WarpOperation op(1, LayerId{1}, makeTestBounds(), makeTestCurve(), WarpAxis::Time, WarpMode::Stretch);
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("WarpOperation can record which prior operation it supersedes", "[core][warp_operation]") {
    const WarpOperation op(2, LayerId{1}, makeTestBounds(), makeTestCurve(), WarpAxis::Time, WarpMode::Stretch,
                            OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("WarpOperation::translatedCopy() shifts bounds and the curve, keeps axis/mode, and supersedes the "
          "original",
          "[core][warp_operation]") {
    const WarpOperation original(5, LayerId{2}, makeTestBounds(), makeTestCurve(), WarpAxis::Frequency,
                                  WarpMode::Stretch);
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 5.0, codecConfig);

    REQUIRE(copy != nullptr);
    REQUIRE(copy->id() == OperationId{9});
    REQUIRE(copy->supersedes().has_value());
    REQUIRE(*copy->supersedes() == OperationId{5});
    REQUIRE(copy->targetLayer() == LayerId{2});
    REQUIRE(copy->bounds().startTimeSeconds == Catch::Approx(0.3));

    const auto* warpCopy = dynamic_cast<const WarpOperation*>(copy.get());
    REQUIRE(warpCopy != nullptr);
    REQUIRE(warpCopy->axis() == WarpAxis::Frequency);
    REQUIRE(warpCopy->mode() == WarpMode::Stretch);
    REQUIRE(warpCopy->curve().nodes()[0].anchor.timeSeconds == Catch::Approx(0.3));

    // The original is untouched.
    REQUIRE(original.bounds().startTimeSeconds == 0.2);
    REQUIRE(original.curve().nodes()[0].anchor.timeSeconds == 0.2);
}

TEST_CASE("WarpAxis and WarpMode round-trip through JSON", "[core][warp_operation]") {
    REQUIRE(nlohmann::json(WarpAxis::Frequency).get<std::string>() == "frequency");
    REQUIRE(nlohmann::json(WarpAxis::Time).get<std::string>() == "time");
    REQUIRE(nlohmann::json(WarpMode::Displace).get<std::string>() == "displace");
    REQUIRE(nlohmann::json(WarpMode::Stretch).get<std::string>() == "stretch");
    REQUIRE(nlohmann::json("frequency").get<WarpAxis>() == WarpAxis::Frequency);
    REQUIRE(nlohmann::json("stretch").get<WarpMode>() == WarpMode::Stretch);
}
