#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/tone_curve.h"

using sound_mind::core::evaluateToneCurve;
using sound_mind::core::monotoneCubicTangents;

TEST_CASE("evaluateToneCurve reproduces the exact identity line for the default two-point curve",
          "[core][tone_curve]") {
    const std::vector<std::array<float, 2>> points{{0.0f, 0.0f}, {1.0f, 1.0f}};

    for (const float x : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        CHECK(evaluateToneCurve(points, x) == Catch::Approx(x));
    }
}

TEST_CASE("evaluateToneCurve passes exactly through every control point", "[core][tone_curve]") {
    const std::vector<std::array<float, 2>> points{{0.0f, 0.0f}, {0.5f, 0.2f}, {1.0f, 1.0f}};
    const auto tangents = monotoneCubicTangents(points);

    for (const auto& point : points) {
        CHECK(evaluateToneCurve(points, tangents, point[0]) == Catch::Approx(point[1]).margin(0.0001));
    }
}

TEST_CASE("evaluateToneCurve matches independently-computed reference values for a 3-point curve",
          "[core][tone_curve]") {
    const std::vector<std::array<float, 2>> points{{0.0f, 0.0f}, {0.5f, 0.2f}, {1.0f, 1.0f}};
    const auto tangents = monotoneCubicTangents(points);

    // Reference values computed independently ahead of this test, from
    // this same documented algorithm.
    CHECK(evaluateToneCurve(points, tangents, 0.25f) == Catch::Approx(0.0625f).margin(0.0001));
    CHECK(evaluateToneCurve(points, tangents, 0.75f) == Catch::Approx(0.5625f).margin(0.0001));
}

TEST_CASE("evaluateToneCurve never overshoots outside [min(y), max(y)] even for a steep early rise",
          "[core][tone_curve]") {
    // A classic overshoot trap for a plain (non-monotone) cubic spline:
    // a steep rise in the first tenth of the domain, then a near-flat
    // tail - a naive spline would dip below 0 or bulge above 1 near the
    // corner. The monotone variant must not.
    const std::vector<std::array<float, 2>> points{{0.0f, 0.0f}, {0.1f, 0.9f}, {1.0f, 1.0f}};
    const auto tangents = monotoneCubicTangents(points);

    float previous = evaluateToneCurve(points, tangents, 0.0f);
    for (int i = 1; i <= 200; ++i) {
        const float x = static_cast<float>(i) / 200.0f;
        const float y = evaluateToneCurve(points, tangents, x);
        CHECK(y >= previous - 0.0001f);
        CHECK(y >= 0.0f);
        CHECK(y <= 1.0f);
        previous = y;
    }
}

TEST_CASE("evaluateToneCurve clamps x outside the control points' own domain", "[core][tone_curve]") {
    const std::vector<std::array<float, 2>> points{{0.0f, 0.1f}, {1.0f, 0.9f}};

    CHECK(evaluateToneCurve(points, -0.5f) == Catch::Approx(0.1f));
    CHECK(evaluateToneCurve(points, 1.5f) == Catch::Approx(0.9f));
}

TEST_CASE("evaluateToneCurve falls back defensively for fewer than two points", "[core][tone_curve]") {
    CHECK(evaluateToneCurve({}, 0.42f) == Catch::Approx(0.42f));

    const std::vector<std::array<float, 2>> singlePoint{{0.3f, 0.7f}};
    CHECK(evaluateToneCurve(singlePoint, 0.0f) == Catch::Approx(0.7f));
    CHECK(evaluateToneCurve(singlePoint, 1.0f) == Catch::Approx(0.7f));
}

TEST_CASE("evaluateToneCurve's tangents-computing overload matches the explicit-tangents overload",
          "[core][tone_curve]") {
    const std::vector<std::array<float, 2>> points{{0.0f, 0.0f}, {0.3f, 0.1f}, {0.7f, 0.9f}, {1.0f, 1.0f}};
    const auto tangents = monotoneCubicTangents(points);

    for (const float x : {0.0f, 0.2f, 0.5f, 0.8f, 1.0f}) {
        CHECK(evaluateToneCurve(points, x) == Catch::Approx(evaluateToneCurve(points, tangents, x)));
    }
}
