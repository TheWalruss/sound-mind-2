#pragma once

#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

/// @brief The default `ProceduralConfiguration::tipThickness()`.
inline constexpr double kDefaultBrushTipThickness = 0.25;

/// @brief The smallest allowed tip thickness.
inline constexpr double kMinBrushTipThickness = 0.05;

/// @brief The largest allowed tip thickness.
inline constexpr double kMaxBrushTipThickness = 1.0;

/**
 * @brief A brush tip's footprint, as a normalized distance: `<= 1` means
 *        the point lies inside the stamp, and the value grows toward `1`
 *        at the shape's edge so a stamp's falloff fades the same way for
 *        every shape.
 *
 * `x` is the time offset from the stamp's centre and `y` the frequency
 * offset (positive = higher, i.e. up on the canvas), each divided by the
 * tip's own radius along that axis - so every shape fits in [-1, 1] on
 * both axes, and nothing outside that box is ever inside.
 *
 * Shapes:
 * - **Circle**, **Square**, **Diamond** - as named.
 * - **Triangle** - pointing up, inscribed in the unit circle (base at
 *   y = -0.5).
 * - **SingleStroke** - one diagonal bar, rising to the right.
 * - **Cross** - a plus sign.
 * - **Star** - five points, one straight up.
 * - **Corner** - an L: a bar along the bottom and one along the left.
 * - **Arc** - the upper half of a ring.
 * - **DotSpatter** - a fixed scatter of small, separate dots.
 * - **Dapple** - a few large, overlapping soft blotches.
 *
 * `thickness` (clamped to [kMinBrushTipThickness, kMaxBrushTipThickness])
 * is the line width of the bar-based shapes (Cross, SingleStroke, Corner,
 * Arc) and of Star's arms, as a fraction of the radius, and the dot size
 * of DotSpatter and Dapple. Circle, Square, Diamond and Triangle ignore
 * it.
 *
 * @param shape The tip shape.
 * @param x Normalized time offset.
 * @param y Normalized frequency offset (up is positive).
 * @param thickness Line width / dot size, as a fraction of the radius.
 * @return The normalized distance - `<= 1` inside.
 * @note Pure and thread-safe; called per pixel per stamp, so it stays
 *       allocation-free.
 */
[[nodiscard]] double brushTipDistance(BrushTipShape shape, double x, double y, double thickness) noexcept;

}  // namespace sound_mind::core
