#pragma once

#include <vector>

#include <QSize>
#include <QWidget>

class QPaintEvent;

namespace sound_mind::studio {

/**
 * @brief A read-only line plot of an arbitrary `[0, 1]`-valued signal -
 *        `v0.Y.58.1`'s own "MindWave UI/UX uplift" (`docs/sound-mind-
 *        roadmap.md`), built so `MindWaveEditor`'s own Continuous
 *        generator group can show a live preview of the actual shape
 *        Shape/Skew/Character/the shared Period/Phase/noise fields
 *        currently produce, updating as any of them change - the
 *        "friendlier... familiar on-ramp" `docs/sound-mind-design.md`'s
 *        own "Continuous Controls" asks for, alongside `RotaryDialWidget`.
 *
 * Deliberately generic - nothing here knows about `MindWave`,
 * `GeneratorType::Continuous`, or `reduceMindWaveToSignal()`; it only
 * ever draws whatever `setSamples()` was last given, the same "purely
 * presentational, caller does the computing" division every other
 * display-only widget in this codebase already draws (`CanvasWidget`'s
 * own MindWave/Equalizer preview overlays, in particular). Unlike every
 * *editable* widget in this codebase, this one has no signals at all - a
 * plain display, the same role a VU meter or spectrum analyzer plays,
 * never a control.
 *
 * **Samples, not a continuous formula** - drawn as a connected polyline
 * through each of setSamples()'s own values, left to right, `0` at this
 * widget's own bottom edge and `1` at its own top (the same "higher value
 * draws higher on screen" convention `HarmonicSeriesWidget`'s own bars and
 * `ToneCurveEditor`'s own curve already use) - not densely re-sampled or
 * smoothed, since the caller (`reduceMindWaveToSignal()`, in
 * `MindWaveEditor`'s own case) already chooses however many samples look
 * smooth enough for the size this widget is actually drawn at.
 */
class WaveformPreviewWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the widget with no samples yet - draws as an empty
    ///        plot area until setSamples() is first called.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit WaveformPreviewWidget(QWidget* parent = nullptr);

    /// @brief This widget's own currently-drawn samples.
    /// @return The current samples.
    [[nodiscard]] const std::vector<float>& samples() const noexcept { return samples_; }

    /**
     * @brief Replaces the plotted samples and repaints.
     * @param samples The new samples, each expected in `[0, 1]` (values
     *        outside that range are clamped when drawn, not rejected -
     *        this widget never validates its own input, matching every
     *        other presentational widget in this codebase).
     */
    void setSamples(std::vector<float> samples);

    /// @return A reasonable default size for this widget inside a panel -
    ///         wide and short, matching a plotted waveform's own aspect.
    [[nodiscard]] QSize sizeHint() const override;

protected:
    /// @brief Draws the plot border and the sample polyline, if any.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> samples_;
};

}  // namespace sound_mind::studio
