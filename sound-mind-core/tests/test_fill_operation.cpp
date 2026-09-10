#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/fill_operation.h"

using sound_mind::core::FillOperation;
using sound_mind::core::Gradient;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::TimeFrequencyRect;

namespace {

TimeFrequencyRect makeTestBounds() {
    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = 0.2;
    bounds.endTimeSeconds = 0.5;
    bounds.lowFrequencyHz = 300.0;
    bounds.highFrequencyHz = 900.0;
    return bounds;
}

}  // namespace

TEST_CASE("FillOperation reports the id, target layer, bounds, and gradient it was constructed with",
          "[core][fill_operation]") {
    const FillOperation op(9, LayerId{4}, makeTestBounds(), Gradient{});
    REQUIRE(op.id() == OperationId{9});
    REQUIRE(op.targetLayer().has_value());
    REQUIRE(op.targetLayer().value() == LayerId{4});
}

TEST_CASE("FillOperation has no supersedes reference unless one is given", "[core][fill_operation]") {
    const FillOperation op(1, LayerId{1}, makeTestBounds(), Gradient{});
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("FillOperation can record which prior operation it supersedes", "[core][fill_operation]") {
    const FillOperation op(2, LayerId{1}, makeTestBounds(), Gradient{}, OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("FillOperation's bounds() is exactly what it was constructed with", "[core][fill_operation]") {
    const TimeFrequencyRect bounds = makeTestBounds();
    const FillOperation op(1, LayerId{1}, bounds, Gradient{});
    const auto opBounds = op.bounds();
    REQUIRE(opBounds.startTimeSeconds == bounds.startTimeSeconds);
    REQUIRE(opBounds.endTimeSeconds == bounds.endTimeSeconds);
    REQUIRE(opBounds.lowFrequencyHz == bounds.lowFrequencyHz);
    REQUIRE(opBounds.highFrequencyHz == bounds.highFrequencyHz);
}

TEST_CASE("FillOperation carries its own gradient", "[core][fill_operation]") {
    Gradient gradient;
    auto stop = gradient.stops().front();
    stop.leftIntensity = -12.0f;
    gradient.setStopValues(0, stop);
    const FillOperation op(1, LayerId{1}, makeTestBounds(), gradient);
    REQUIRE(op.gradient().stops().front().leftIntensity == -12.0f);
}
