#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/brush_tip.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::brushTipDistance;
using sound_mind::core::BrushTipShape;
using sound_mind::core::kDefaultBrushTipThickness;

namespace {

/// @brief Inside the stamp (`<= 1`), at the default thickness.
bool inside(BrushTipShape shape, double x, double y, double thickness = kDefaultBrushTipThickness) {
    return brushTipDistance(shape, x, y, thickness) <= 1.0;
}

}  // namespace

// x is time (right), y is frequency (up), both normalized to the tip's own
// radius - see brushTipDistance()'s own docs.

TEST_CASE("Circle, Square and Diamond keep their existing footprints", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::Circle, 0.7, 0.7));
    CHECK_FALSE(inside(BrushTipShape::Circle, 0.8, 0.8));
    CHECK(inside(BrushTipShape::Square, 0.95, 0.95));
    CHECK(inside(BrushTipShape::Diamond, 0.5, 0.5));
    CHECK_FALSE(inside(BrushTipShape::Diamond, 0.6, 0.6));
}

TEST_CASE("Triangle points up, with its base below the centre", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::Triangle, 0.0, 0.9));    // near the top vertex.
    CHECK(inside(BrushTipShape::Triangle, 0.0, -0.45));  // just above the base.
    CHECK_FALSE(inside(BrushTipShape::Triangle, 0.0, -0.6));
    CHECK_FALSE(inside(BrushTipShape::Triangle, 0.6, 0.6));  // outside a slanted side.
    CHECK(inside(BrushTipShape::Triangle, 0.6, -0.4));
}

TEST_CASE("Cross is a plus sign whose bar width follows thickness", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::Cross, 0.95, 0.0));
    CHECK(inside(BrushTipShape::Cross, 0.0, -0.95));
    CHECK_FALSE(inside(BrushTipShape::Cross, 0.6, 0.6));  // between the arms.
    CHECK_FALSE(inside(BrushTipShape::Cross, 0.9, 0.3, 0.2));
    CHECK(inside(BrushTipShape::Cross, 0.9, 0.3, 0.4));
}

TEST_CASE("Single Stroke is one diagonal bar, rising to the right", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::SingleStroke, 0.6, 0.6));
    CHECK(inside(BrushTipShape::SingleStroke, -0.6, -0.6));
    CHECK_FALSE(inside(BrushTipShape::SingleStroke, 0.6, -0.6));
    CHECK_FALSE(inside(BrushTipShape::SingleStroke, -0.6, 0.6));
}

TEST_CASE("Star has five points with hollows between them", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::Star, 0.0, 0.95));    // the top point.
    CHECK(inside(BrushTipShape::Star, 0.0, 0.0));
    CHECK_FALSE(inside(BrushTipShape::Star, 0.0, -0.9));  // straight down sits between the two lower points.
    CHECK_FALSE(inside(BrushTipShape::Circle, 0.0, -1.1));
}

TEST_CASE("Corner is an L: a bottom bar and a left bar", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::Corner, 0.9, -0.9));
    CHECK(inside(BrushTipShape::Corner, -0.9, 0.9));
    CHECK_FALSE(inside(BrushTipShape::Corner, 0.5, 0.5));
    CHECK_FALSE(inside(BrushTipShape::Corner, 0.0, 0.0));
}

TEST_CASE("Arc is the upper half of a ring", "[core][brush_tip]") {
    CHECK(inside(BrushTipShape::Arc, 0.0, 0.85));
    CHECK_FALSE(inside(BrushTipShape::Arc, 0.0, 0.0));   // the ring's hollow centre.
    CHECK_FALSE(inside(BrushTipShape::Arc, 0.0, -0.85));  // the missing lower half.
}

TEST_CASE("Dot Spatter is separate dots whose size follows thickness", "[core][brush_tip]") {
    // The centre dot is always present; most of the area between dots isn't.
    CHECK(inside(BrushTipShape::DotSpatter, 0.0, 0.0));
    int insideCount = 0;
    int total = 0;
    for (double x = -1.0; x <= 1.0; x += 0.05) {
        for (double y = -1.0; y <= 1.0; y += 0.05) {
            if (x * x + y * y <= 1.0) {
                ++total;
                insideCount += inside(BrushTipShape::DotSpatter, x, y, 0.1) ? 1 : 0;
            }
        }
    }
    CHECK(insideCount > 0);
    CHECK(insideCount < total / 2);

    int largerCount = 0;
    for (double x = -1.0; x <= 1.0; x += 0.05) {
        for (double y = -1.0; y <= 1.0; y += 0.05) {
            largerCount += (x * x + y * y <= 1.0 && inside(BrushTipShape::DotSpatter, x, y, 0.25)) ? 1 : 0;
        }
    }
    CHECK(largerCount > insideCount);
}

TEST_CASE("Dapple is a few large soft blotches, not a circle", "[core][brush_tip]") {
    int insideCount = 0;
    int total = 0;
    for (double x = -1.0; x <= 1.0; x += 0.05) {
        for (double y = -1.0; y <= 1.0; y += 0.05) {
            if (x * x + y * y <= 1.0) {
                ++total;
                insideCount += inside(BrushTipShape::Dapple, x, y) ? 1 : 0;
            }
        }
    }
    CHECK(insideCount > total / 5);
    CHECK(insideCount < total * 9 / 10);
}

TEST_CASE("No tip reaches beyond its own radius", "[core][brush_tip]") {
    for (const auto shape : {BrushTipShape::Circle, BrushTipShape::Square, BrushTipShape::Diamond,
                             BrushTipShape::Triangle, BrushTipShape::SingleStroke, BrushTipShape::Cross,
                             BrushTipShape::Star, BrushTipShape::Corner, BrushTipShape::Arc,
                             BrushTipShape::DotSpatter, BrushTipShape::Dapple}) {
        for (const double thickness : {0.05, 0.25, 1.0}) {
            CHECK_FALSE(inside(shape, 1.05, 0.0, thickness));
            CHECK_FALSE(inside(shape, 0.0, 1.05, thickness));
            CHECK_FALSE(inside(shape, -1.05, -1.05, thickness));
        }
    }
}

TEST_CASE("ProceduralConfiguration's tip thickness defaults, clamps, and round-trips through JSON",
          "[core][brush_tip]") {
    sound_mind::core::ProceduralConfiguration config;
    CHECK(config.tipThickness() == kDefaultBrushTipThickness);
    config.setTipThickness(5.0);
    CHECK(config.tipThickness() == 1.0);
    config.setTipThickness(0.0);
    CHECK(config.tipThickness() == 0.05);
    config.setTipThickness(0.4);

    const nlohmann::json json = config;
    const auto restored = sound_mind::core::toolConfigurationFromJson(json);
    const auto* procedural = dynamic_cast<const sound_mind::core::ProceduralConfiguration*>(restored.get());
    REQUIRE(procedural != nullptr);
    CHECK(procedural->tipThickness() == 0.4);
}
