#pragma once

#include <QSize>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace sound_mind::studio {

/**
 * @brief A classic synth-style rotary knob, for any single scalar value in
 *        a fixed `[minimum, maximum]` range - `v0.Y.58.1`'s own "MindWave
 *        UI/UX uplift" (`docs/sound-mind-roadmap.md`), built specifically
 *        so `MindWaveEditor`'s own Continuous generator (Shape/Skew/
 *        Character) gets "a classic visual LFO-style interface," per
 *        `docs/sound-mind-design.md`'s own "Continuous Controls" ("the
 *        way a modular synth's shape/slope/smoothness-style function
 *        generator does"), confirmed with the user over three other
 *        candidates.
 *
 * Deliberately a general-purpose, `MindWave`-agnostic control - nothing
 * here knows about `GeneratorType::Continuous` or any other domain
 * concept, the same "purely presentational" division every other widget
 * in this codebase draws. A visual *complement* to a plain `QDoubleSpinBox`
 * showing the same value, not a replacement for one - `MindWaveEditor`
 * embeds one of each side by side per Continuous field, mirroring the
 * `EqualizerCurveWidget`/`GradientEditorWidget` and `HarmonicSeriesWidget`/
 * per-harmonic-spin-box pairs this same milestone already established:
 * the dial is for quick, tactile, by-ear adjustment; the spin box stays
 * the precise, typeable readout. Kept in sync by whichever one changes
 * forwarding its own new value onto the other's setValue() - see
 * setValue()'s own docs on why that can't loop back.
 *
 * **Interaction - vertical drag, not absolute angle.** Clicking anywhere on
 * the dial and dragging the mouse *vertically* changes the value (up
 * increases, down decreases) - the real-world convention every DAW's own
 * knob widget already uses, since directly mapping the cursor's absolute
 * angle from the dial's own center is awkward with a mouse (small
 * movements near the center swing the angle wildly) and isn't how
 * physical or software knobs are actually operated. Dragging the full
 * `kPixelsPerFullSweep` vertical distance sweeps the entire
 * `[minimum, maximum]` range, regardless of how far the cursor strays
 * horizontally or past this widget's own edges - the same "the drag
 * distance decouples precision from the widget's own drawn size"
 * reasoning a real hardware fader/knob's own mechanical travel has
 * nothing to do with how it looks.
 *
 * **Rendering**: a ring, swept from this widget's own fixed `-135°` (at
 * `minimum`) to `+135°` (at `maximum`), measured clockwise from straight
 * up - the same `270°` sweep (with a `90°` dead zone at the bottom) every
 * physical rotary knob uses. The swept portion (`minimum` up to the
 * current value) is drawn in `palette().highlight()`; the remainder in
 * `palette().mid()`; a short radial pointer line marks the exact current
 * angle on top of both.
 */
class RotaryDialWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the dial with the default `[0, 1]` range and a
    ///        starting value of `0`.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit RotaryDialWidget(QWidget* parent = nullptr);

    /// @brief This dial's own current value.
    /// @return The current value, always within `[minimum(), maximum()]`.
    [[nodiscard]] double value() const noexcept { return value_; }

    /// @return The lower end of this dial's own range.
    [[nodiscard]] double minimum() const noexcept { return minimum_; }

    /// @return The upper end of this dial's own range.
    [[nodiscard]] double maximum() const noexcept { return maximum_; }

    /**
     * @brief Sets this dial's own value range - `MindWaveEditor`'s three
     *        Continuous dials all use the default `[0, 1]` (matching
     *        their own paired spin boxes' range exactly), but nothing
     *        here assumes that range specifically.
     *
     * The current value() is re-clamped into the new range if it would
     * otherwise fall outside it; doesn't itself emit valueChanged() even
     * if the clamp actually changes it, the same "a structural reset, not
     * a user edit" reasoning setValue() gives.
     *
     * @param minimum The new lower end.
     * @param maximum The new upper end. Swapped with `minimum` if given
     *        smaller than it, so the range is always well-formed.
     */
    void setRange(double minimum, double maximum);

    /**
     * @brief Loads a value into the dial - the actual work behind
     *        syncing from the paired spin box's own edits, or loading a
     *        freshly-selected `MindWave`.
     *
     * Deliberately does *not* emit valueChanged() - the same "loading is
     * a sync from some other source, not a user edit" reasoning
     * `GradientBarWidget::setGradient()`'s own docs already give, and the
     * same reasoning that keeps the dial/spin-box pair's own cross-sync
     * from looping back on itself.
     *
     * @param value The new value, clamped into `[minimum(), maximum()]`.
     */
    void setValue(double value);

    /// @return A reasonable default size for this dial inside a panel -
    ///         small and square, matching a real knob's own proportions.
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Emitted whenever a drag changes the value.
    /// @param value This dial's own new value.
    void valueChanged(double value);

protected:
    /// @brief Draws the ring (swept and unswept portions) and the
    ///        pointer line - see this class's own docs.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Begins a vertical drag from wherever the press landed -
    ///        see this class's own docs on why the press position's own
    ///        angle is never used directly.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress drag, if any - a no-op otherwise.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress drag, if any.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// @brief `value`'s own angle, in degrees, clockwise from straight up
    ///        - `-135` at minimum(), `+135` at maximum().
    [[nodiscard]] double angleForValue(double value) const;

    /// @brief Clamps `value` into `[minimum_, maximum_]` and, if it
    ///        actually changed, stores it, emits valueChanged(), and
    ///        repaints.
    void applyDraggedValue(double value);

    double minimum_ = 0.0;
    double maximum_ = 1.0;
    double value_ = 0.0;
    bool dragging_ = false;
    qreal dragStartY_ = 0.0;
    double dragStartValue_ = 0.0;
};

}  // namespace sound_mind::studio
