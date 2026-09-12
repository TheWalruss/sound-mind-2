#include "sound_mind/core/tone_curve.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sound_mind::core {

std::vector<float> monotoneCubicTangents(const std::vector<std::array<float, 2>>& points) {
    const std::size_t n = points.size();
    std::vector<float> tangents(n, 0.0f);
    if (n < 2) {
        return tangents;
    }

    std::vector<float> secants(n - 1);
    for (std::size_t k = 0; k + 1 < n; ++k) {
        const float dx = points[k + 1][0] - points[k][0];
        secants[k] = (dx > 0.0f) ? (points[k + 1][1] - points[k][1]) / dx : 0.0f;
    }

    // Initial tangents: the raw secant at each endpoint, the average of
    // the two neighboring secants everywhere else.
    tangents.front() = secants.front();
    tangents.back() = secants.back();
    for (std::size_t k = 1; k + 1 < n; ++k) {
        tangents[k] = (secants[k - 1] + secants[k]) * 0.5f;
    }

    // Zero the tangent on both sides of any flat (or already-inverted)
    // secant, so a locally constant segment stays flat rather than
    // overshooting into a dip or bump.
    for (std::size_t k = 0; k + 1 < n; ++k) {
        if (secants[k] == 0.0f) {
            tangents[k] = 0.0f;
            tangents[k + 1] = 0.0f;
        }
    }

    // Fritsch-Carlson's own monotonicity guarantee: for each segment,
    // rescale its own two bordering tangents so neither one's ratio to
    // the segment's secant can push the curve past it. Processed in
    // order, one segment at a time - a tangent shared by two adjacent
    // segments can be touched by both, the same single forward pass the
    // reference algorithm itself uses.
    for (std::size_t k = 0; k + 1 < n; ++k) {
        if (secants[k] == 0.0f) {
            continue;
        }
        const float alpha = tangents[k] / secants[k];
        const float beta = tangents[k + 1] / secants[k];
        if (alpha < 0.0f) {
            tangents[k] = 0.0f;
        }
        if (beta < 0.0f) {
            tangents[k + 1] = 0.0f;
        }
        const float alphaClamped = std::max(alpha, 0.0f);
        const float betaClamped = std::max(beta, 0.0f);
        const float magnitudeSquared = alphaClamped * alphaClamped + betaClamped * betaClamped;
        if (magnitudeSquared > 9.0f) {
            const float tau = 3.0f / std::sqrt(magnitudeSquared);
            tangents[k] = tau * alphaClamped * secants[k];
            tangents[k + 1] = tau * betaClamped * secants[k];
        }
    }

    return tangents;
}

float evaluateToneCurve(const std::vector<std::array<float, 2>>& points, const std::vector<float>& tangents,
                         float x) {
    const std::size_t n = points.size();
    if (n == 0) {
        return x;
    }
    if (n == 1) {
        return points.front()[1];
    }

    const float clampedX = std::clamp(x, points.front()[0], points.back()[0]);
    for (std::size_t k = 0; k + 1 < n; ++k) {
        const float x0 = points[k][0];
        const float x1 = points[k + 1][0];
        if (clampedX >= x0 && clampedX <= x1) {
            const float h = x1 - x0;
            const float t = (h > 0.0f) ? (clampedX - x0) / h : 0.0f;
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            const float h10 = t3 - 2.0f * t2 + t;
            const float h01 = -2.0f * t3 + 3.0f * t2;
            const float h11 = t3 - t2;
            return h00 * points[k][1] + h10 * h * tangents[k] + h01 * points[k + 1][1] + h11 * h * tangents[k + 1];
        }
    }
    return points.back()[1];
}

float evaluateToneCurve(const std::vector<std::array<float, 2>>& points, float x) {
    return evaluateToneCurve(points, monotoneCubicTangents(points), x);
}

}  // namespace sound_mind::core
