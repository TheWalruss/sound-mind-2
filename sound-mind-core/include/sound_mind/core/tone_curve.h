#pragma once

#include <array>
#include <vector>

namespace sound_mind::core {

/**
 * @brief Precomputes each control point's own tangent for a monotone
 *        cubic Hermite spline (the Fritsch-Carlson method) through
 *        `points` - the shared math behind `ToneCurve`'s own remap
 *        (`applyFilter()`, `filter_application.cpp`) and its live preview
 *        in `sound_mind::studio::ToneCurveEditor` - computed once and
 *        reused across every `evaluateToneCurve()` call for the same
 *        `points`, rather than recomputed per sample.
 *
 * Monotone, not a plain (possibly overshooting) cubic spline: a tone
 * curve is meant to *remap* loudness, and a spline that overshoots past
 * a control point's own neighbors near a steep segment would fold the
 * mapping back on itself (a higher input producing a *lower* output
 * somewhere it shouldn't) - exactly the artifact Fritsch-Carlson's own
 * tangent-rescaling step exists to prevent. This is the standard
 * "Monotone cubic interpolation" reference algorithm (Fritsch & Carlson
 * 1980): initial tangents as the average of each point's own neighboring
 * secant slopes (raw secant at the two endpoints), zeroed across any
 * flat or sign-reversing secant, then rescaled per segment so neither
 * tangent's own ratio to that segment's secant can push the curve past
 * it - not a byte-for-byte port of legacy's own `scipy.interpolate.
 * PchipInterpolator` (a different, more involved tangent estimate from
 * de Boor & Swartz), but the same class of monotone-safe cubic Hermite
 * spline, chosen because it's precisely specifiable from a standard
 * reference rather than scipy's own internal weighting (`CLAUDE.md`'s own
 * "kept only as a lessons-learned reference, not as code to port
 * directly").
 *
 * @param points Control points, each `{x, y}`, ordered by `x` - the same
 *        shape `FilterConfiguration::toneCurvePoints()` already returns.
 *        Fewer than two points produces an all-zero tangent set (a
 *        defensive fallback; `evaluateToneCurve()` doesn't consult
 *        tangents at all in that case).
 * @return One tangent per point, same order/count as `points`.
 */
[[nodiscard]] std::vector<float> monotoneCubicTangents(const std::vector<std::array<float, 2>>& points);

/**
 * @brief Evaluates the monotone cubic Hermite spline through `points` at
 *        `x`, using `tangents` (`monotoneCubicTangents(points)`'s own
 *        result, passed in rather than recomputed - see that function's
 *        own docs).
 *
 * `x` is clamped to `[points.front()[0], points.back()[0]]` first - the
 * same clamp-then-interpolate contract `Gradient::evaluate()` already
 * uses for its own `t`, so a curve never extrapolates past its own
 * defined domain. Passes exactly through every control point's own `y`
 * at that point's own `x` (a property of Hermite splines in general, not
 * specific to the monotone variant).
 *
 * @param points Control points, each `{x, y}`, ordered by `x`.
 * @param tangents `monotoneCubicTangents(points)`'s own result for the
 *        same `points` - passing a mismatched or stale set is undefined.
 * @param x The position to evaluate the curve at.
 * @return The curve's own value at `x` - `x` unchanged if `points` is
 *         empty, or the single point's own `y` if `points` has exactly
 *         one entry (both defensive fallbacks; `FilterConfiguration`'s
 *         own invariant is "always at least the two endpoints", so
 *         neither case is expected in practice).
 */
[[nodiscard]] float evaluateToneCurve(const std::vector<std::array<float, 2>>& points,
                                       const std::vector<float>& tangents, float x);

/**
 * @brief Convenience overload that computes `monotoneCubicTangents(points)`
 *        internally, for a one-off evaluation.
 *
 * Prefer the tangents-taking overload when evaluating the same `points`
 * many times (e.g. once per cell across a whole composite, in
 * `applyFilter()`'s own `ToneCurve` case) - it computes the shared
 * tangent set exactly once instead of once per call.
 *
 * @param points Control points, each `{x, y}`, ordered by `x`.
 * @param x The position to evaluate the curve at.
 * @return The curve's own value at `x` - see the tangents-taking
 *         overload's own docs for the exact contract.
 */
[[nodiscard]] float evaluateToneCurve(const std::vector<std::array<float, 2>>& points, float x);

}  // namespace sound_mind::core
