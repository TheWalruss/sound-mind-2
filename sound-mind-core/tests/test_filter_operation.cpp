#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/filter_operation.h"
#include "sound_mind/core/paint_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterOperation;
using sound_mind::core::FilterType;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::SelectionRegion;
using sound_mind::core::TimeFrequencyPoint;
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

/// @brief A plain triangular (Path-kind) boundary - see test_fill_operation.cpp's
/// own identical helper for why its exact shape doesn't matter here.
SelectionRegion makeTestBoundary() {
    Path path;
    PathNode a;
    a.anchor = TimeFrequencyPoint{0.2, 300.0};
    a.type = PathNodeType::Corner;
    PathNode b;
    b.anchor = TimeFrequencyPoint{0.5, 300.0};
    b.type = PathNodeType::Corner;
    PathNode c;
    c.anchor = TimeFrequencyPoint{0.35, 900.0};
    c.type = PathNodeType::Corner;
    path.addNode(a);
    path.addNode(b);
    path.addNode(c);
    return SelectionRegion(std::move(path));
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

FilterConfiguration makeTestFilterConfiguration() {
    FilterConfiguration config;
    config.setType(FilterType::UniformBlur);
    config.setBlurSigma(2.5f);
    return config;
}

}  // namespace

TEST_CASE("FilterOperation reports the id, target layer, bounds, and config it was constructed with",
          "[core][filter_operation]") {
    const FilterOperation op(9, LayerId{4}, makeTestBounds(), makeTestFilterConfiguration());
    REQUIRE(op.id() == OperationId{9});
    REQUIRE(op.targetLayer().has_value());
    REQUIRE(op.targetLayer().value() == LayerId{4});
}

TEST_CASE("FilterOperation has no supersedes reference unless one is given", "[core][filter_operation]") {
    const FilterOperation op(1, LayerId{1}, makeTestBounds(), makeTestFilterConfiguration());
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("FilterOperation can record which prior operation it supersedes", "[core][filter_operation]") {
    const FilterOperation op(2, LayerId{1}, makeTestBounds(), makeTestFilterConfiguration(), OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("FilterOperation's bounds() is exactly what it was constructed with", "[core][filter_operation]") {
    const TimeFrequencyRect bounds = makeTestBounds();
    const FilterOperation op(1, LayerId{1}, bounds, makeTestFilterConfiguration());
    const auto opBounds = op.bounds();
    REQUIRE(opBounds.startTimeSeconds == bounds.startTimeSeconds);
    REQUIRE(opBounds.endTimeSeconds == bounds.endTimeSeconds);
    REQUIRE(opBounds.lowFrequencyHz == bounds.lowFrequencyHz);
    REQUIRE(opBounds.highFrequencyHz == bounds.highFrequencyHz);
}

TEST_CASE("FilterOperation carries its own configuration", "[core][filter_operation]") {
    const FilterOperation op(1, LayerId{1}, makeTestBounds(), makeTestFilterConfiguration());
    REQUIRE(op.config().type() == FilterType::UniformBlur);
    REQUIRE(op.config().blurSigma() == 2.5f);
}

TEST_CASE("FilterOperation::translatedCopy() shifts bounds, keeps the config, and supersedes the original",
          "[core][filter_operation]") {
    const FilterOperation original(5, LayerId{2}, makeTestBounds(), makeTestFilterConfiguration());
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 5.0, codecConfig);

    const float expectedLowFrequency = sound_mind::core::binIndexToFrequency(
        sound_mind::core::frequencyToBinIndex(300.0f, codecConfig) + 5.0f, codecConfig);
    const float expectedHighFrequency = sound_mind::core::binIndexToFrequency(
        sound_mind::core::frequencyToBinIndex(900.0f, codecConfig) + 5.0f, codecConfig);

    REQUIRE(copy != nullptr);
    REQUIRE(copy->id() == OperationId{9});
    REQUIRE(copy->supersedes().has_value());
    REQUIRE(*copy->supersedes() == OperationId{5});
    REQUIRE(copy->targetLayer() == LayerId{2});
    REQUIRE(copy->bounds().startTimeSeconds == Catch::Approx(0.3));
    REQUIRE(copy->bounds().endTimeSeconds == Catch::Approx(0.6));
    REQUIRE(copy->bounds().lowFrequencyHz == Catch::Approx(expectedLowFrequency));
    REQUIRE(copy->bounds().highFrequencyHz == Catch::Approx(expectedHighFrequency));

    const auto* filterCopy = dynamic_cast<const FilterOperation*>(copy.get());
    REQUIRE(filterCopy != nullptr);
    REQUIRE(filterCopy->config().type() == FilterType::UniformBlur);
    REQUIRE(filterCopy->config().blurSigma() == 2.5f);

    // The original is untouched.
    REQUIRE(original.bounds().startTimeSeconds == 0.2);
}

TEST_CASE("FilterOperation has no boundary unless one is given", "[core][filter_operation]") {
    const FilterOperation op(1, LayerId{1}, makeTestBounds(), makeTestFilterConfiguration());
    REQUIRE_FALSE(op.boundary().has_value());
}

TEST_CASE("FilterOperation can carry a Lasso boundary, independent of its own bounding-box bounds()",
          "[core][filter_operation]") {
    const TimeFrequencyRect bounds = makeTestBounds();
    const FilterOperation op(1, LayerId{1}, bounds, makeTestFilterConfiguration(), std::nullopt, makeTestBoundary());
    REQUIRE(op.boundary().has_value());
    REQUIRE(op.boundary()->path().nodes().size() == 3);
    REQUIRE(op.bounds().startTimeSeconds == bounds.startTimeSeconds);
    REQUIRE(op.bounds().endTimeSeconds == bounds.endTimeSeconds);
}

TEST_CASE("FilterOperation::translatedCopy() shifts a Lasso boundary the same way it shifts bounds()",
          "[core][filter_operation]") {
    const FilterOperation original(5, LayerId{2}, makeTestBounds(), makeTestFilterConfiguration(), std::nullopt,
                                    makeTestBoundary());
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 5.0, codecConfig);

    const auto* filterCopy = dynamic_cast<const FilterOperation*>(copy.get());
    REQUIRE(filterCopy != nullptr);
    REQUIRE(filterCopy->boundary().has_value());
    REQUIRE(filterCopy->boundary()->path().nodes().size() == 3);
    REQUIRE(filterCopy->boundary()->path().nodes()[0].anchor.timeSeconds == Catch::Approx(0.3));
    const float expectedFrequency = sound_mind::core::binIndexToFrequency(
        sound_mind::core::frequencyToBinIndex(300.0f, codecConfig) + 5.0f, codecConfig);
    REQUIRE(filterCopy->boundary()->path().nodes()[0].anchor.frequencyHz == Catch::Approx(expectedFrequency));

    // The original's own boundary is untouched.
    REQUIRE(original.boundary()->path().nodes()[0].anchor.timeSeconds == 0.2);
}
