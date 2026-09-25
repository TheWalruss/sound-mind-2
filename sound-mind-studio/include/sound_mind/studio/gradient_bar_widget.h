#pragma once

#include <optional>

#include <QRectF>
#include <QWidget>

#include "sound_mind/core/gradient.h"

class QMouseEvent;
class QPaintEvent;

namespace sound_mind::studio {

/**
 * @brief A draggable, add/remove-stop horizontal bar for editing a
 *        `sound_mind::core::Gradient`'s own stop *positions* - real-world
 *        testing pass, 2026-09-20, finding #17 ("the Gradient tool has
 *        regressed significantly... compared to the gradient tool in the
 *        Legacy version").
 *
 * The low-level, `ToneCurveEditor`-style interaction surface only -
 * mirrors its own "click near a stop drags it; click elsewhere inserts
 * and drags a new one; double-click an interior stop removes it"
 * contract exactly, with one structural difference: a `GradientStop` has
 * four independent values (left/right intensity, left/right opacity), not
 * one - too many to represent as this bar's own vertical position the way
 * `ToneCurveEditor`'s single `y` can. So dragging here only ever changes a
 * stop's own `t` (horizontal position); its values are edited by
 * `GradientEditorWidget`'s own spin boxes instead, which is also why this
 * bar - unlike `ToneCurveEditor` - keeps a *persistent* `selectedIndex()`
 * (not just a transient drag index): a click alone, with no drag at all,
 * still needs to tell that outer widget which stop's values to display.
 *
 * Purely presentational, the same division of responsibility every other
 * configuration control in this codebase already draws: every edit (drag,
 * insert, remove) emits gradientChanged() with the bar's own new,
 * complete gradient; nothing here writes into a `FilterConfiguration`/
 * `ToolConfiguration` directly.
 *
 * **Rendering**: a live preview of the gradient itself, sampled across the
 * bar's own width and colored via `dbToDisplayByte()` - the same
 * `Red = leftIntensity, Green = rightIntensity` mapping
 * `ToolConfigurationPanel::color()`'s own pre-this-installment single-
 * color button already used - with alpha driven by each sample's own
 * (averaged) opacity, over a checkerboard background so a low-opacity
 * region reads as "transparent," the same visual language legacy's own
 * `GradientBarWidget` used. A small marker below the bar shows each
 * stop's own position, drawn larger/distinctly for `selectedIndex()`.
 */
class GradientBarWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the bar with the default fully-transparent two-stop
    ///        gradient (`sound_mind::core::Gradient{}`) and the first stop
    ///        selected.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit GradientBarWidget(QWidget* parent = nullptr);

    /// @brief This bar's own current gradient.
    /// @return The current gradient.
    [[nodiscard]] const sound_mind::core::Gradient& gradient() const noexcept { return gradient_; }

    /**
     * @brief Loads a gradient into the bar - the actual work behind
     *        switching to a different Filter/Paint tool configuration.
     *
     * Deliberately does *not* emit gradientChanged() - the same "loading
     * is a sync from some other source, not a user edit" reasoning
     * `ToneCurveEditor::setPoints()`'s own docs already give.
     * `selectedIndex()` resets to the first stop (`0`) - whatever was
     * selected in the *previous* gradient has no meaningful counterpart
     * in a newly-loaded one.
     *
     * @param gradient The new gradient.
     */
    void setGradient(sound_mind::core::Gradient gradient);

    /// @brief Which stop's own values `GradientEditorWidget`'s spin boxes
    ///        should currently display.
    /// @return The selected stop's index into `gradient().stops()`. Always
    ///         a valid index - a gradient always has at least two stops,
    ///         and this bar always keeps exactly one selected.
    [[nodiscard]] std::size_t selectedIndex() const noexcept { return selectedIndex_; }

    /**
     * @brief Removes the currently selected stop, if it's a removable
     *        interior one - the actual work behind
     *        `GradientEditorWidget`'s own "Delete Stop" button.
     *
     * A no-op (no removal, no emit) if `selectedIndex()` is currently an
     * endpoint (`0` or the last stop) - the same "can't remove either
     * endpoint" contract `Gradient::removeStop()` itself already
     * enforces; this just adds the button-driven entry point alongside
     * the bar's own double-click-to-remove gesture, both funneling
     * through the same rule. Selects the stop now at the removed one's
     * own old index (or the last stop, if it was the very last interior
     * one) afterward.
     */
    void removeSelectedStop();

    /**
     * @brief Applies new values onto the currently selected stop, in
     *        place - `GradientEditorWidget`'s own value spin boxes call
     *        this, as opposed to setGradient() (a full reload, which
     *        also resets `selectedIndex()`) or the mouse-driven
     *        insert/drag/remove above (which change *which* stop exists
     *        or *where* it sits, not just its values).
     * @param values The new values; `values.t` is ignored, matching
     *        `Gradient::setStopValues()`'s own contract - the selected
     *        stop's own position is unchanged.
     */
    void setSelectedStopValues(const sound_mind::core::GradientStop& values);

    /// @brief Sets the gradient's own `linkChannels()` flag - a plain
    ///        pass-through to `Gradient::setLinkChannels()`, still
    ///        emitting gradientChanged() since it's real, persisted state
    ///        (see `Gradient::linkChannels()`'s own docs).
    /// @param linked The new linked state.
    void setLinkChannels(bool linked);

    /// @return A reasonable default size for this bar inside a panel.
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Emitted whenever a drag, insert, or remove changes the
    ///        gradient.
    /// @param gradient The bar's own new, complete gradient.
    void gradientChanged(const sound_mind::core::Gradient& gradient);

    /// @brief Emitted whenever the selected stop changes - a plain click
    ///        (no drag), an insert (the new stop becomes selected), or a
    ///        remove (see removeSelectedStop()'s own docs).
    /// @param index The newly selected stop's index.
    void selectionChanged(std::size_t index);

protected:
    /// @brief Draws the checkerboard, the gradient preview, and every
    ///        stop's own marker - see this class's own docs.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Selects an existing stop and begins dragging it, or inserts
    ///        and begins dragging a new one - see this class's own docs.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress drag, if any - a no-op otherwise.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress drag, if any.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Removes an existing interior stop, if the double-click
    ///        landed on one - see removeSelectedStop()'s own docs.
    /// @param event The double-click event.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    /// @brief The inset plotting area within this widget's own bounds.
    [[nodiscard]] QRectF barRect() const;

    /// @brief A stop's own normalized `t` to a widget pixel x-coordinate.
    [[nodiscard]] qreal tToX(float t) const;

    /// @brief The inverse of tToX() - clamped to `[0, 1]`.
    [[nodiscard]] float xToT(qreal x) const;

    /// @brief The index of the stop within a small hit radius of `x`, or
    ///        `-1` if none is that close.
    [[nodiscard]] int hitTestStop(qreal x) const;

    /// @brief Selects `index` and, if it actually changed, emits
    ///        selectionChanged() and repaints.
    void selectStop(std::size_t index);

    sound_mind::core::Gradient gradient_;
    std::size_t selectedIndex_ = 0;
    int draggingIndex_ = -1;
};

}  // namespace sound_mind::studio
