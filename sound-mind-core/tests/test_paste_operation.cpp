#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paste_operation.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::BlendMode;
using sound_mind::core::Clip;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::Path;
using sound_mind::core::PasteOperation;
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

/// @brief Matching test_fill_operation.cpp's own makeTestBoundary() - see
/// its own docs for why the exact shape doesn't matter here.
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

/// @brief Matching test_paint_application.cpp's own makeTestConfig() -
/// see test_paint_operation.cpp's own copy of this helper for why
/// translatedCopy() needs one at all now.
StreamCodecConfig makeTestConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;
    config.binCount = 100;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

Clip makeTestClip() {
    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-5.0f, -6.0f, -7.0f, -8.0f};
    clip.sharedPhaseRadians = {0.1f, 0.2f, 0.3f, 0.4f};
    return clip;
}

}  // namespace

TEST_CASE("PasteOperation reports the id, target layer, bounds, and clip it was constructed with",
          "[core][paste_operation]") {
    const PasteOperation op(9, LayerId{4}, makeTestBounds(), makeTestClip());
    REQUIRE(op.id() == OperationId{9});
    REQUIRE(op.targetLayer().has_value());
    REQUIRE(op.targetLayer().value() == LayerId{4});
}

TEST_CASE("PasteOperation has no supersedes reference unless one is given", "[core][paste_operation]") {
    const PasteOperation op(1, LayerId{1}, makeTestBounds(), makeTestClip());
    REQUIRE_FALSE(op.supersedes().has_value());
}

TEST_CASE("PasteOperation can record which prior operation it supersedes", "[core][paste_operation]") {
    const PasteOperation op(2, LayerId{1}, makeTestBounds(), makeTestClip(), OperationId{1});
    REQUIRE(op.supersedes().has_value());
    REQUIRE(op.supersedes().value() == OperationId{1});
}

TEST_CASE("PasteOperation's bounds() is exactly what it was constructed with (its placement)",
          "[core][paste_operation]") {
    const TimeFrequencyRect bounds = makeTestBounds();
    const PasteOperation op(1, LayerId{1}, bounds, makeTestClip());
    const auto opBounds = op.bounds();
    REQUIRE(opBounds.startTimeSeconds == bounds.startTimeSeconds);
    REQUIRE(opBounds.endTimeSeconds == bounds.endTimeSeconds);
    REQUIRE(opBounds.lowFrequencyHz == bounds.lowFrequencyHz);
    REQUIRE(opBounds.highFrequencyHz == bounds.highFrequencyHz);
}

TEST_CASE("PasteOperation carries its own clip", "[core][paste_operation]") {
    const Clip clip = makeTestClip();
    const PasteOperation op(1, LayerId{1}, makeTestBounds(), clip);
    REQUIRE(op.clip().frameCount == clip.frameCount);
    REQUIRE(op.clip().binCount == clip.binCount);
    REQUIRE(op.clip().leftMagnitudeDb == clip.leftMagnitudeDb);
    REQUIRE(op.clip().rightMagnitudeDb == clip.rightMagnitudeDb);
    REQUIRE(op.clip().sharedPhaseRadians == clip.sharedPhaseRadians);
}

TEST_CASE("PasteOperation::translatedCopy() shifts the placement, keeps the clip, and supersedes the original",
          "[core][paste_operation]") {
    const Clip clip = makeTestClip();
    const PasteOperation original(5, LayerId{2}, makeTestBounds(), clip);
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 5.0, codecConfig);

    // Each bound's own frequency shifts by the same *bin* delta, not the
    // same Hz amount - see translated(TimeFrequencyRect, ...)'s own docs.
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

    const auto* pasteCopy = dynamic_cast<const PasteOperation*>(copy.get());
    REQUIRE(pasteCopy != nullptr);
    REQUIRE(pasteCopy->clip().leftMagnitudeDb == clip.leftMagnitudeDb);

    // The original is untouched.
    REQUIRE(original.bounds().startTimeSeconds == 0.2);
}

TEST_CASE("PasteOperation has no boundary unless one is given", "[core][paste_operation]") {
    const PasteOperation op(1, LayerId{1}, makeTestBounds(), makeTestClip());
    REQUIRE_FALSE(op.boundary().has_value());
}

TEST_CASE("PasteOperation can carry a Lasso boundary, independent of its own bounding-box bounds()",
          "[core][paste_operation]") {
    const TimeFrequencyRect bounds = makeTestBounds();
    const PasteOperation op(1, LayerId{1}, bounds, makeTestClip(), std::nullopt, makeTestBoundary());
    REQUIRE(op.boundary().has_value());
    REQUIRE(op.boundary()->path().nodes().size() == 3);
    REQUIRE(op.bounds().startTimeSeconds == bounds.startTimeSeconds);
}

TEST_CASE("PasteOperation::translatedCopy() shifts a Lasso boundary the same way it shifts bounds()",
          "[core][paste_operation]") {
    const PasteOperation original(5, LayerId{2}, makeTestBounds(), makeTestClip(), std::nullopt, makeTestBoundary());
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 5.0, codecConfig);

    const auto* pasteCopy = dynamic_cast<const PasteOperation*>(copy.get());
    REQUIRE(pasteCopy != nullptr);
    REQUIRE(pasteCopy->boundary().has_value());
    REQUIRE(pasteCopy->boundary()->path().nodes()[0].anchor.timeSeconds == Catch::Approx(0.3));

    // The original's own boundary is untouched.
    REQUIRE(original.boundary()->path().nodes()[0].anchor.timeSeconds == 0.2);
}

TEST_CASE("PasteOperation defaults to Overwrite blend mode", "[core][paste_operation][blend_mode]") {
    const PasteOperation op(1, LayerId{1}, makeTestBounds(), makeTestClip());
    REQUIRE(op.blendMode() == BlendMode::Overwrite);
}

TEST_CASE("PasteOperation can be constructed with a non-default blend mode",
          "[core][paste_operation][blend_mode]") {
    const PasteOperation op(1, LayerId{1}, makeTestBounds(), makeTestClip(), std::nullopt, std::nullopt,
                             BlendMode::Multiply);
    REQUIRE(op.blendMode() == BlendMode::Multiply);
}

TEST_CASE("PasteOperation::translatedCopy() carries the blend mode forward unchanged",
          "[core][paste_operation][blend_mode]") {
    const PasteOperation original(5, LayerId{2}, makeTestBounds(), makeTestClip(), std::nullopt, std::nullopt,
                                   BlendMode::Screen);
    const StreamCodecConfig codecConfig = makeTestConfig();

    const auto copy = original.translatedCopy(OperationId{9}, 0.1, 5.0, codecConfig);

    const auto* pasteCopy = dynamic_cast<const PasteOperation*>(copy.get());
    REQUIRE(pasteCopy != nullptr);
    REQUIRE(pasteCopy->blendMode() == BlendMode::Screen);
}

TEST_CASE("A Clip round-trips through JSON", "[core][paste_operation]") {
    const Clip clip = makeTestClip();

    const nlohmann::json json = clip;
    const Clip restored = json.get<Clip>();

    REQUIRE(restored.frameCount == clip.frameCount);
    REQUIRE(restored.binCount == clip.binCount);
    REQUIRE(restored.leftMagnitudeDb == clip.leftMagnitudeDb);
    REQUIRE(restored.rightMagnitudeDb == clip.rightMagnitudeDb);
    REQUIRE(restored.sharedPhaseRadians == clip.sharedPhaseRadians);
}
