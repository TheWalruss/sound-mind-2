#include "sound_mind/core/brush_tip.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace sound_mind::core {

namespace {

constexpr double kPi = std::numbers::pi;

[[nodiscard]] double cross(double ax, double ay, double bx, double by) noexcept { return ax * by - ay * bx; }

/// @brief An upward-pointing equilateral triangle inscribed in the unit
/// circle, as a polygon gauge: max over its three edges of (n . p) / (edge
/// distance), each edge 0.5 from the centre.
[[nodiscard]] double triangle(double x, double y) noexcept {
    constexpr double kCos30 = 0.8660254037844386;
    return 2.0 * std::max({-y, kCos30 * x + 0.5 * y, -kCos30 * x + 0.5 * y});
}

/// @brief A bar of half-width `halfWidth` centred on the x axis, as a
///        distance: |x| along it, |y| / halfWidth across it.
[[nodiscard]] double bar(double along, double across, double halfWidth) noexcept {
    return std::max(std::abs(along), std::abs(across) / halfWidth);
}

/// @brief A five-pointed star (one point straight up), outer radius 1 and
///        inner radius `innerRadius`: |p| over the boundary radius in p's
///        own direction.
[[nodiscard]] double star(double x, double y, double innerRadius) noexcept {
    const double r = std::hypot(x, y);
    if (r == 0.0) {
        return 0.0;
    }
    constexpr double kSector = 2.0 * kPi / 5.0;
    double phi = std::fmod(std::atan2(y, x) - kPi / 2.0, kSector);
    if (phi < 0.0) {
        phi += kSector;
    }
    const double local = std::min(phi, kSector - phi);  // 0 at a point, kSector/2 at a hollow.
    // Boundary edge from the point A = (1, 0) to the hollow
    // B = innerRadius * (cos(kSector/2), sin(kSector/2)), intersected with
    // the ray at angle `local`.
    const double bx = innerRadius * std::cos(kSector / 2.0);
    const double by = innerRadius * std::sin(kSector / 2.0);
    const double ex = bx - 1.0;
    const double ey = by;
    const double boundary = cross(1.0, 0.0, ex, ey) / cross(std::cos(local), std::sin(local), ex, ey);
    return r / boundary;
}

/// @brief The upper half of a ring with centreline radius `1 - halfWidth`
///        and half-width `halfWidth`; below the x axis, the distance to
///        the nearer of the two rounded ends.
[[nodiscard]] double arc(double x, double y, double halfWidth) noexcept {
    const double centreline = 1.0 - halfWidth;
    if (y >= 0.0) {
        return std::abs(std::hypot(x, y) - centreline) / halfWidth;
    }
    return std::min(std::hypot(x - centreline, y), std::hypot(x + centreline, y)) / halfWidth;
}

/// @brief Nine small dots: one at the centre, eight on a golden-angle
///        spiral, each of radius `dotRadius`, all kept inside the unit
///        circle.
[[nodiscard]] double dotSpatter(double x, double y, double dotRadius) noexcept {
    constexpr double kGoldenAngle = 2.399963229728653;
    double nearest = std::hypot(x, y);
    const double spread = 1.0 - dotRadius;
    for (int i = 1; i <= 8; ++i) {
        const double rho = spread * std::sqrt(static_cast<double>(i) / 8.0);
        const double angle = static_cast<double>(i) * kGoldenAngle;
        nearest = std::min(nearest, std::hypot(x - rho * std::cos(angle), y - rho * std::sin(angle)));
    }
    return nearest / dotRadius;
}

/// @brief Six large, overlapping blotches of uneven size, each with a
///        slightly wobbly edge.
[[nodiscard]] double dapple(double x, double y, double baseRadius) noexcept {
    struct Blob {
        double cx;
        double cy;
        double scale;
    };
    static constexpr std::array<Blob, 6> kBlobs{{
        {0.0, 0.0, 0.8},
        {0.45, 0.15, 1.0},
        {-0.35, 0.4, 0.85},
        {-0.3, -0.45, 1.1},
        {0.3, -0.45, 0.75},
        {0.05, 0.55, 0.7},
    }};
    double best = 1e9;
    for (std::size_t i = 0; i < kBlobs.size(); ++i) {
        const Blob& blob = kBlobs[i];
        const double dx = x - blob.cx;
        const double dy = y - blob.cy;
        const double wobble = 1.0 + 0.15 * std::sin(3.0 * std::atan2(dy, dx) + static_cast<double>(i));
        best = std::min(best, std::hypot(dx, dy) / (baseRadius * blob.scale * wobble));
    }
    return best;
}

}  // namespace

double brushTipDistance(BrushTipShape shape, double x, double y, double thickness) noexcept {
    const double t = std::clamp(thickness, kMinBrushTipThickness, kMaxBrushTipThickness);
    double distance = 0.0;
    switch (shape) {
        case BrushTipShape::Circle:
            distance = std::hypot(x, y);
            break;
        case BrushTipShape::Square:
            distance = std::max(std::abs(x), std::abs(y));
            break;
        case BrushTipShape::Diamond:
            distance = std::abs(x) + std::abs(y);
            break;
        case BrushTipShape::Triangle:
            distance = triangle(x, y);
            break;
        case BrushTipShape::SingleStroke: {
            const double along = (x + y) / std::numbers::sqrt2;
            const double across = (y - x) / std::numbers::sqrt2;
            distance = bar(along, across, t);
            break;
        }
        case BrushTipShape::Cross:
            distance = std::min(bar(x, y, t), bar(y, x, t));
            break;
        case BrushTipShape::Star:
            distance = star(x, y, 0.3 + 0.5 * t);
            break;
        case BrushTipShape::Corner:
            distance = std::min(bar(x, y - (-1.0 + t), t), bar(y, x - (-1.0 + t), t));
            break;
        case BrushTipShape::Arc:
            distance = arc(x, y, t);
            break;
        case BrushTipShape::DotSpatter:
            distance = dotSpatter(x, y, 0.6 * t);
            break;
        case BrushTipShape::Dapple:
            distance = dapple(x, y, 0.25 + 0.5 * t);
            break;
    }
    // Nothing reaches outside the tip's own [-1, 1] box, whatever the
    // shape and thickness.
    return std::max(distance, std::max(std::abs(x), std::abs(y)));
}

}  // namespace sound_mind::core
