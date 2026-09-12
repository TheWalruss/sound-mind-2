#pragma once

#include <array>
#include <vector>

#include <QPointF>
#include <QRectF>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace sound_mind::studio {

/**
 * @brief A draggable, add/remove-point curve editor for `FilterType::
 *        ToneCurve`'s own control points - see `sound_mind::core::
 *        evaluateToneCurve()`'s own docs (`tone_curve.h`) for the exact
 *        monotone cubic spline this widget renders a live preview of, and
 *        `docs/sound-mind-architecture.md`'s own Decision recording why a
 *        real editor was built here rather than another two-endpoint
 *        spin-box panel (`FilterConfigurationPanel`'s own established
 *        pattern for `FrequencyAxisGradient`).
 *
 * Purely presentational, the same division of responsibility every other
 * configuration control in this codebase already draws: every edit
 * (drag, add, remove) emits pointsChanged() with the widget's own new,
 * complete, sorted-by-x point list; nothing here writes into a
 * `FilterConfiguration` directly.
 *
 * **Interaction**: a left click within a small hit radius of an existing
 * point begins dragging it; a left click anywhere else inserts a new
 * point there (in sorted-by-x order) and begins dragging that instead. A
 * double-click on an existing *interior* point removes it - the first
 * and last points can't be removed this way (matching `Gradient::
 * removeStop()`'s own "refuses to drop either endpoint" precedent,
 * `gradient.h`), since `evaluateToneCurve()`'s own domain is exactly
 * `[points.front()[0], points.back()[0]]` and losing an endpoint would
 * silently shrink it. The first point's own x stays pinned at `0`, and
 * the last point's own x stays pinned at `1`, while dragging - only
 * their own y moves; every interior point's own x is clamped strictly
 * between its immediate neighbors so points can never cross or reorder.
 */
class ToneCurveEditor : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the editor with the default identity curve
    ///        (`{0,0}`, `{1,1}`) - see `FilterConfiguration`'s own docs.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit ToneCurveEditor(QWidget* parent = nullptr);

    /// @brief This widget's own current control points, sorted by `x`.
    /// @return The current control points.
    [[nodiscard]] const std::vector<std::array<float, 2>>& points() const noexcept { return points_; }

    /**
     * @brief Loads a point list into the widget - the actual work behind
     *        switching `LayersPanel`'s own selection to a different Tone
     *        Curve Filter layer.
     *
     * Deliberately does *not* emit pointsChanged() - the same "loading is
     * a sync from some other source, not a user edit" reasoning
     * `FilterConfigurationPanel::setFilterConfiguration()`'s own docs
     * already give.
     *
     * @param points The new control points - not validated (an
     *        out-of-order or fewer-than-two-point list is the caller's
     *        own responsibility, matching `toneCurvePoints()`'s own
     *        contract).
     */
    void setPoints(std::vector<std::array<float, 2>> points);

    /// @return A reasonable default size for this widget inside a panel.
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Emitted whenever a drag, add, or remove changes the point list.
    /// @param points The widget's own new, complete, sorted-by-x point list.
    void pointsChanged(const std::vector<std::array<float, 2>>& points);

protected:
    /// @brief Draws the grid, the identity reference diagonal, the curve
    ///        itself, and every control point's own handle - see this
    ///        class's own docs.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Begins dragging an existing point, or inserts and begins
    ///        dragging a new one - see this class's own docs.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress drag, if any - a no-op otherwise.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress drag, if any.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Removes an existing interior point, if the double-click
    ///        landed on one - see this class's own docs.
    /// @param event The double-click event.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    /// @brief The inset plotting area within this widget's own bounds,
    ///        leaving a small margin so a point drawn at `x=0`/`x=1` or
    ///        `y=0`/`y=1` isn't clipped at the very edge.
    [[nodiscard]] QRectF plotRect() const;

    /// @brief Maps a control point (`[0,1]` on both axes) to a widget
    ///        pixel position - `y` flips (plot `y=0` at the bottom,
    ///        matching a conventional curve editor, versus Qt's own
    ///        top-down pixel `y`).
    [[nodiscard]] QPointF plotToWidget(std::array<float, 2> point) const;

    /// @brief The inverse of plotToWidget() - clamped to `[0,1]` on both
    ///        axes.
    [[nodiscard]] std::array<float, 2> widgetToPlot(QPointF widgetPosition) const;

    /// @brief The index of the point within a small hit radius of
    ///        `widgetPosition`, or `-1` if none is that close.
    [[nodiscard]] int hitTestPoint(QPointF widgetPosition) const;

    std::vector<std::array<float, 2>> points_{{0.0f, 0.0f}, {1.0f, 1.0f}};
    int draggingIndex_ = -1;
};

}  // namespace sound_mind::studio
