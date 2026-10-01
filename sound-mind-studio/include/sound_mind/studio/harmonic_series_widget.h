#pragma once

#include <vector>

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QWidget>

class QColor;
class QMouseEvent;
class QPaintEvent;
class QPainter;

namespace sound_mind::studio {

/**
 * @brief A draggable bar-chart editor for an `InstrumentConfiguration`'s
 *        own `harmonicStrengths()` - `docs/sound-mind-roadmap.md`'s own
 *        "Instrument harmonic-series visual editor" ("dragging harmonic-
 *        strength bars directly, rather than only the plain per-harmonic
 *        spin boxes `v0.Y.32.1` shipped"), flagged as part of the same
 *        `v0.Y.58.1` UI polish milestone the Equalizer's own
 *        `EqualizerCurveWidget` closed out.
 *
 * A visual *complement* to `ToolConfigurationPanel`'s existing per-
 * harmonic `QDoubleSpinBox` rows, not a replacement - both edit the same
 * `std::vector<double>`, kept in sync by `ToolConfigurationPanel`
 * forwarding each one's own change onto the other. Unlike
 * `EqualizerCurveWidget`'s own `Gradient` (a variable-length list of
 * stops, each with its own frequency *and* value), a harmonic series is a
 * fixed-length list of plain scalars, one per harmonic, with no position
 * to drag separately from the value itself - so this widget is simpler
 * than that one: a fixed bank of vertical bars, one per harmonic, each a
 * direct-set slider (not a select-then-drag stop) - clicking or dragging
 * anywhere in a bar's own column sets that harmonic's strength to the
 * clicked height immediately, the same "click sets the value" directness
 * a graphic equalizer's own fader bank has. The bar count always matches
 * `harmonicStrengths().size()` exactly - this widget never adds or
 * removes harmonics itself (unlike `EqualizerCurveWidget`'s own click-to-
 * insert gesture); that's `harmonicCountSpinBox_`'s own job.
 *
 * **Vertical axis**: a fixed `[0, 2.0]` range, not `InstrumentConfiguration`'s
 * own full `[0, 10]` (the per-harmonic spin boxes' own declared range,
 * see `ToolConfigurationPanel`'s own docs) - a typical instrument's own
 * loudest harmonic is the fundamental, `1.0` by default, so `10` would
 * leave every realistic value crammed into the bottom tenth of the
 * widget. `2.0` gives generous headroom above that default while keeping
 * the common case legible; a value already set higher (only reachable via
 * the spin box, never by dragging here) draws clamped at this widget's
 * own full height rather than overflowing it. A value this widget's own
 * drag *sets* is always within `[0, 2.0]`, even though the underlying
 * `double` it writes has no such ceiling of its own.
 *
 * Purely presentational, the same division of responsibility every other
 * configuration control in this codebase already draws: every edit emits
 * harmonicStrengthsChanged() with this widget's own new, complete vector;
 * nothing here writes into a `ToolConfiguration` directly.
 */
class HarmonicSeriesWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the widget with the default fully-empty (zero-bar)
    ///        series - `setHarmonicStrengths()` is expected to be called
    ///        once real data exists, the same "blank until synced"
    ///        contract every other presentational editor in this
    ///        codebase starts from.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit HarmonicSeriesWidget(QWidget* parent = nullptr);

    /// @brief This widget's own current per-harmonic strengths,
    ///        fundamental first.
    /// @return The current strengths.
    [[nodiscard]] const std::vector<double>& harmonicStrengths() const noexcept { return strengths_; }

    /**
     * @brief Loads a harmonic series into the widget - the actual work
     *        behind switching to a different `InstrumentConfiguration`,
     *        and `ToolConfigurationPanel`'s own same-vector sync from the
     *        numeric spin boxes' own edits.
     *
     * Deliberately does *not* emit harmonicStrengthsChanged() - the same
     * "loading is a sync from some other source, not a user edit"
     * reasoning `GradientBarWidget::setGradient()`'s own docs already
     * give. The bar count changes to match `strengths.size()` exactly.
     *
     * @param strengths The new per-harmonic strengths, fundamental first.
     */
    void setHarmonicStrengths(std::vector<double> strengths);

    /**
     * @brief Renders a small, standalone thumbnail of `strengths`' own
     *        bar chart - the same visual language paintEvent() draws
     *        live, but to an offscreen `QImage` at `size` instead of this
     *        widget's own current bounds. `ToolConfigurationPanel`'s own
     *        Tool Preset combo uses this to show each saved Instrument
     *        preset's own harmonic content as an icon next to its name -
     *        `docs/sound-mind-roadmap.md`'s own "a small rendered
     *        thumbnail of its harmonic content", the same spirit as
     *        `MindWavesPanel`'s own per-row grayscale MindWave preview
     *        (`MindWaveController::refreshMindWavesPanel()`).
     * @param strengths The per-harmonic strengths to render.
     * @param size The thumbnail's own pixel size.
     * @return The rendered thumbnail. Transparent where no bar is drawn.
     */
    [[nodiscard]] static QImage renderThumbnail(const std::vector<double>& strengths, const QSize& size);

    /// @return A reasonable default size for this widget inside a panel.
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Emitted whenever a click or drag sets a bar's own height to
    ///        a new value.
    /// @param strengths This widget's own new, complete per-harmonic
    ///        strengths.
    void harmonicStrengthsChanged(const std::vector<double>& strengths);

protected:
    /// @brief Draws the quarter-value gridlines and every harmonic's own
    ///        bar - see this class's own docs.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Sets whichever bar the press landed in to the clicked
    ///        height and begins dragging it - see this class's own docs.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress drag, if any, updating the same
    ///        bar `mousePressEvent()` started on regardless of the
    ///        cursor's own current column - a no-op otherwise.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress drag, if any.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// @brief The inset plotting area within this widget's own bounds.
    [[nodiscard]] QRectF plotRect() const;

    /// @brief A strength value to a widget pixel y-coordinate - `0` at
    ///        the bottom, this class's own display ceiling at the top.
    [[nodiscard]] static qreal valueToY(const QRectF& rect, double value);

    /// @brief The inverse of valueToY() - clamped to `[0, display ceiling]`.
    [[nodiscard]] static double yToValue(const QRectF& rect, qreal y);

    /// @brief Which bar column `x` falls within, clamped to the last bar
    ///        if `x` lands past the final one's own right edge (so a drag
    ///        that overshoots the widget's own right side still tracks
    ///        the last bar rather than losing the drag entirely).
    /// @param rect The plotting area, as plotRect() returns it.
    /// @param x The widget-space x-coordinate.
    /// @param count The number of bars currently shown.
    [[nodiscard]] static int columnAt(const QRectF& rect, qreal x, std::size_t count);

    /// @brief Shared bar-drawing body for both paintEvent() and
    ///        renderThumbnail() - draws every one of `strengths`' own
    ///        bars into `rect` via `painter`, filled with `fillColor`
    ///        (`palette().highlight()` for the live widget,
    ///        `QApplication::palette().highlight()` for a standalone
    ///        thumbnail with no widget of its own to read a palette
    ///        from).
    static void paintBars(QPainter& painter, const QRectF& rect, const std::vector<double>& strengths,
                           const QColor& fillColor);

    /// @brief Sets bar `index`'s own value from widget-space `y` and, if
    ///        it actually changed, emits harmonicStrengthsChanged() and
    ///        repaints.
    void setBarValueFromY(std::size_t index, qreal y);

    std::vector<double> strengths_;
    int draggingIndex_ = -1;
};

}  // namespace sound_mind::studio
