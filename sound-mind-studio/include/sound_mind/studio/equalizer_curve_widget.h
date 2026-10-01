#pragma once

#include <cstddef>
#include <optional>

#include <QRectF>
#include <QWidget>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/project_settings.h"

class QMouseEvent;
class QPaintEvent;

namespace sound_mind::studio {

/**
 * @brief A vertical, frequency-axis-oriented curve view of the Equalizer
 *        layer's own "Cut" `sound_mind::core::Gradient` - `docs/sound-mind-
 *        roadmap.md`'s "Equalizer usability" ("a real vertical EQ widget
 *        showing the current curve against frequency-domain guides... the
 *        same spirit as a hardware/plugin EQ's own frequency-response
 *        display"), flagged 2026-09-27 from real-world use.
 *
 * A visual *complement* to `GradientEditorWidget`'s own precise numeric
 * editor (`FilterConfigurationPanel`'s existing Cut-mode section), not a
 * replacement - both edit the same `Gradient`, kept in sync by
 * `FilterConfigurationPanel` forwarding each one's own `gradientChanged()`
 * onto the other's `setGradient()`. Modeled on `GradientBarWidget` (`t`
 * only ever changes via its own "remove then reinsert" workaround - see
 * `Gradient::setStopValues()`'s own docs on why - and a persistent
 * `selectedIndex()`/`selectionChanged()`, not `ToneCurveEditor`'s
 * transient drag-only selection) but drawn as a real `ToneCurveEditor`-
 * style line/curve rather than a color swatch, since what this widget
 * shows - a single scalar "Cut" amount per stop - fits a curve plot
 * exactly the way `GradientStop`'s own four independent values don't.
 *
 * **Orientation, confirmed against both `filter_application.h`'s own
 * `t=0` (lowest encoded frequency) / `t=1` (highest) convention and
 * `CanvasWidget`'s own on-screen "top is highest frequency" rendering
 * (`sound_mind::codec::toGrayscaleImage()`'s own row-0-is-highest-
 * frequency layout)**: `t=1` (highest frequency) plots at this widget's
 * own top, `t=0` (lowest) at the bottom - matching the canvas's own
 * vertical axis exactly, so this widget reads naturally sitting beside
 * it. The legacy Python Studio's own `_EqGradientWidget` (`check the
 * legacy Python Studio for a concrete precedent` - this roadmap item's
 * own suggestion) used the *opposite* convention (`t=0` at its own top) -
 * not reused verbatim here, since that codebase's own `t` ran in the
 * opposite frequency direction to begin with.
 *
 * **Cut amount** (`max(leftOpacity, rightOpacity)` per stop, the same
 * formula the legacy panel used) plots left-to-right, `0` (pass-through)
 * at the left edge, `1` (full silence) at the right - intensity is never
 * read here, matching `GradientEditorWidget::setCutMode()`'s own "Cut is
 * opacity alone, always writing silence underneath" contract exactly;
 * this widget assumes (but doesn't enforce) that the gradient it's given
 * already keeps intensity pinned to the silence floor at every stop, the
 * same assumption that contract already requires of any gradient reaching
 * Cut mode in the first place.
 *
 * **Interaction**: a click within a small hit radius of an existing
 * handle selects it and begins dragging; a click elsewhere inserts a new
 * stop at that frequency (seeded from `Gradient::insertStop()`'s own
 * "whatever `evaluate()` already produces" contract - never a visible
 * jump) and begins dragging that instead. Dragging repositions *only*
 * the stop's own frequency (`t`) - its Cut amount is unchanged, and stays
 * editable only through `GradientEditorWidget`'s own Cut spin boxes,
 * matching the legacy panel's own "drag repositions, a separate control
 * edits the value" split exactly. A double-click on an existing
 * *interior* handle removes it; the first and last stops can't be moved
 * past each other or removed, the same endpoint rules every other
 * gradient-editing surface in this codebase already enforces.
 *
 * Purely presentational, the same division of responsibility every other
 * configuration control in this codebase already draws: every edit emits
 * gradientChanged() with this widget's own new, complete gradient;
 * nothing here writes into a `FilterConfiguration` directly.
 */
class EqualizerCurveWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the widget with the default fully-transparent
    ///        two-stop gradient (`sound_mind::core::Gradient{}`) and no
    ///        project settings yet (frequency guides stay blank until
    ///        setProjectSettings() is called).
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit EqualizerCurveWidget(QWidget* parent = nullptr);

    /// @brief This widget's own current gradient.
    /// @return The current gradient.
    [[nodiscard]] const sound_mind::core::Gradient& gradient() const noexcept { return gradient_; }

    /**
     * @brief Loads a gradient into the widget - both the actual work
     *        behind switching `LayersPanel`'s own selection to the
     *        Equalizer layer, and `FilterConfigurationPanel`'s own
     *        same-gradient sync from `GradientEditorWidget`'s own edits.
     *
     * Deliberately does *not* emit gradientChanged() - the same "loading
     * is a sync from some other source, not a user edit" reasoning
     * `GradientBarWidget::setGradient()`'s own docs already give (and the
     * same reasoning that keeps `FilterConfigurationPanel`'s own
     * cross-widget sync from looping back on itself). `selectedIndex()`
     * resets to the first stop (`0`).
     *
     * @param gradient The new gradient.
     */
    void setGradient(sound_mind::core::Gradient gradient);

    /**
     * @brief Sets the project settings this widget's own frequency-axis
     *        guides are computed against - `MainWindow` calls this
     *        whenever a project opens (or closes, with `std::nullopt`).
     *
     * Without settings, the plot area still draws (the curve itself needs
     * no project context at all - `t` is already normalized), just with
     * no frequency gridlines/labels - the same "no-op-safe before a
     * project exists" default every other project-context-dependent
     * control in this codebase already has.
     *
     * @param settings The current project's own settings, or
     *        `std::nullopt` if none is open.
     */
    void setProjectSettings(std::optional<sound_mind::core::ProjectSettings> settings);

    /// @brief Which stop this widget's own handles currently show as
    ///        selected (highlighted) - purely a display detail, since
    ///        (unlike `GradientBarWidget`) nothing reads a stop's *value*
    ///        back out through this widget.
    /// @return The selected stop's index into `gradient().stops()`.
    [[nodiscard]] std::size_t selectedIndex() const noexcept { return selectedIndex_; }

    /// @return A reasonable default size for this widget inside a panel -
    ///         taller than wide, matching its own vertical orientation.
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Emitted whenever a drag, insert, or remove changes the
    ///        gradient.
    /// @param gradient This widget's own new, complete gradient.
    void gradientChanged(const sound_mind::core::Gradient& gradient);

    /// @brief Emitted whenever the selected stop changes - a plain click
    ///        (no drag), an insert (the new stop becomes selected), or a
    ///        remove.
    /// @param index The newly selected stop's index.
    void selectionChanged(std::size_t index);

protected:
    /// @brief Draws the plot border, frequency gridlines/labels, the
    ///        quarter Cut-amount gridlines, the curve itself, and every
    ///        stop's own handle - see this class's own docs.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Selects an existing handle and begins dragging it, or
    ///        inserts and begins dragging a new one - see this class's
    ///        own docs.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress drag (repositioning frequency
    ///        only), if any - a no-op otherwise.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress drag, if any.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Removes an existing interior handle, if the double-click
    ///        landed on one.
    /// @param event The double-click event.
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    /// @brief The inset plotting area within this widget's own bounds.
    [[nodiscard]] QRectF plotRect() const;

    /// @brief A stop's own normalized `t` to a widget pixel y-coordinate -
    ///        `t=1` at the top, `t=0` at the bottom (see this class's own
    ///        docs on why).
    [[nodiscard]] qreal tToY(float t) const;

    /// @brief The inverse of tToY() - clamped to `[0, 1]`.
    [[nodiscard]] float yToT(qreal y) const;

    /// @brief A stop's own Cut amount (`[0, 1]`) to a widget pixel
    ///        x-coordinate - `0` at the left, `1` at the right.
    [[nodiscard]] qreal cutToX(float cut) const;

    /// @brief `stop`'s own Cut amount - `max(leftOpacity, rightOpacity)`,
    ///        clamped to `[0, 1]` (see this class's own docs).
    [[nodiscard]] static float cutAmount(const sound_mind::core::GradientStop& stop) noexcept;

    /// @brief The index of the stop within a small hit radius of
    ///        `widgetPosition`, or `-1` if none is that close.
    [[nodiscard]] int hitTestStop(QPointF widgetPosition) const;

    /// @brief Selects `index` and, if it actually changed, emits
    ///        selectionChanged() and repaints.
    void selectStop(std::size_t index);

    sound_mind::core::Gradient gradient_;
    std::optional<sound_mind::core::ProjectSettings> settings_;
    std::size_t selectedIndex_ = 0;
    int draggingIndex_ = -1;
};

}  // namespace sound_mind::studio
