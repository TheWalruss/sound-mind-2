#pragma once

#include <QDockWidget>

#include "sound_mind/core/filter_configuration.h"

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QSpinBox;
class QWidget;

namespace sound_mind::studio {

class ToneCurveEditor;

/**
 * @brief A dockable panel for configuring the currently selected Filter
 *        layer - see `docs/sound-mind-design.md`'s "Filter Layer".
 *
 * Purely presentational, the same division of responsibility as
 * `ToolConfigurationPanel`: every edit emits filterConfigurationChanged()
 * with the panel's own current, complete `FilterConfiguration` -
 * `MainWindow` is what actually threads it into whichever `Layer` is
 * currently selected in `LayersPanel`, and disables this panel entirely
 * (via ordinary `QWidget::setEnabled(false)`) whenever the current
 * selection isn't a Filter-type layer at all - this panel has no concept
 * of layer selection or identity of its own, the same "purely
 * presentational" boundary `ToolConfigurationPanel` already draws around
 * `PaintOperation`/Pick.
 *
 * **A `FilterType` selector, listing only the types with a real algorithm
 * behind them** - `docs/sound-mind-roadmap.md`'s `v0.Y.28.1` (Filter
 * Layers) milestone's own multi-installment scope: every one of the six
 * designed filter types now has one (see `sound_mind::core::
 * applyFilter()`'s own docs), in `docs/sound-mind-design.md`'s own
 * family order (Blur & focus, then Tonal, then Spectral shaping) - so
 * this selector now lists all six. Selecting a type shows only that
 * type's own parameter group; every other group stays hidden
 * (`QWidget::setVisible(false)`), the same "meaningless unless `type()`
 * matches" contract `FilterConfiguration`'s own per-field docs already
 * state.
 *
 * **A basic, two-endpoint-stop gradient editor for `FrequencyAxisGradient`,
 * not a rich visual one** - `Gradient` always has at least its two
 * endpoint stops (`t=0`, `t=1`); this panel edits exactly those two
 * directly, via plain spin boxes (no draggable visual stop editor -
 * nothing in this codebase has built one yet, for any gradient,
 * anywhere). A richer, draggable editor - and interior stops - are
 * deferred; the Equalizer layer's own specialized "Cut" editor (a later
 * installment of this same milestone) will need its own nicer widget
 * regardless, once built.
 *
 * **A real add/drag-point curve editor for `ToneCurve`**
 * (`ToneCurveEditor`, `tone_curve_editor.h`) - confirmed with the user
 * ahead of Installment C's own implementation, in place of another
 * two-endpoint-spin-box panel like Frequency-Axis Gradient's own. See
 * that class's own docs for its interaction model.
 *
 * **A specialized "Cut" editor for the Equalizer layer** - `setEqualizerMode()`
 * (called by `MainWindow` whenever the selected layer's own `type()` is
 * `LayerType::Equalizer`) hides the `FilterType` combo entirely (the
 * Equalizer is always a `FrequencyAxisGradient` filter - switching it to
 * something else doesn't make sense) and shows a dedicated Cut group
 * instead of the generic Frequency-Axis Gradient one: the same two
 * endpoint stops, but exposing only Left/Right **Cut** per stop (`0` =
 * pass-through, `1` = full silence) - intensity is never shown, always
 * written as the silence floor underneath, confirmed with the user ahead
 * of implementation (Decision #61's own already-recorded reasoning for
 * why this needs no new blend math, only a constrained editor).
 */
class FilterConfigurationPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with a fresh, fully transparent default
    ///        `FilterConfiguration` - see `FilterConfiguration`'s own docs.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit FilterConfigurationPanel(QWidget* parent = nullptr);

    /// @brief The filter configuration this panel's controls currently
    ///        describe.
    /// @return The current configuration.
    [[nodiscard]] const sound_mind::core::FilterConfiguration& filterConfiguration() const noexcept {
        return config_;
    }

    /**
     * @brief Loads a configuration into the panel's own controls - the
     *        actual work behind switching `LayersPanel`'s own selection
     *        to a different Filter-type layer.
     *
     * Deliberately does *not* emit filterConfigurationChanged() - the
     * same "loading is a sync from some other source, not a user edit"
     * reasoning `ToolConfigurationPanel::setToolConfiguration()`'s own
     * docs give; emitting here would make `MainWindow`'s own wiring
     * indistinguishable from a real change and re-apply the unchanged
     * configuration right back onto whatever layer was just selected.
     *
     * @param config The configuration to display.
     */
    void setFilterConfiguration(const sound_mind::core::FilterConfiguration& config);

    /**
     * @brief Switches between the generic per-`FilterType` editor (the
     *        default) and the Equalizer's own specialized "Cut" editor -
     *        see this class's own docs.
     *
     * Purely a display mode - like `setFilterConfiguration()`, this
     * doesn't emit filterConfigurationChanged(); `MainWindow` calls this
     * whenever the selection changes, based on the newly selected
     * layer's own `type()`, entirely separately from loading that
     * layer's own configuration.
     *
     * @param isEqualizer `true` to show the Cut editor (and hide the
     *        `FilterType` combo); `false` for the normal, generic panel.
     */
    void setEqualizerMode(bool isEqualizer);

signals:
    /// @brief Emitted whenever any parameter control changes.
    /// @param config The panel's own new, complete configuration.
    void filterConfigurationChanged(const sound_mind::core::FilterConfiguration& config);

private:
    /// @brief Emits filterConfigurationChanged() with the current config_.
    void emitConfigChanged();

    /// @brief Shows the one parameter group matching `config_.type()` and
    ///        hides every other one - see this class's own docs.
    void updateVisibleGroup();

    sound_mind::core::FilterConfiguration config_;
    bool isEqualizerMode_ = false;

    QComboBox* filterTypeCombo_ = nullptr;
    QLabel* filterTypeLabel_ = nullptr;

    QWidget* frequencyAxisGradientSection_ = nullptr;
    QLabel* frequencyGradientLabel_ = nullptr;
    QDoubleSpinBox* startLeftIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* startLeftOpacitySpinBox_ = nullptr;
    QDoubleSpinBox* startRightIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* startRightOpacitySpinBox_ = nullptr;
    QDoubleSpinBox* endLeftIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* endLeftOpacitySpinBox_ = nullptr;
    QDoubleSpinBox* endRightIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* endRightOpacitySpinBox_ = nullptr;

    QGroupBox* uniformBlurGroup_ = nullptr;
    QDoubleSpinBox* blurSigmaSpinBox_ = nullptr;

    QGroupBox* edgePreservingBlurGroup_ = nullptr;
    QSpinBox* medianSizeSpinBox_ = nullptr;

    QGroupBox* directionalBlurGroup_ = nullptr;
    QSpinBox* directionalBlurLengthSpinBox_ = nullptr;
    QDoubleSpinBox* directionalBlurAngleSpinBox_ = nullptr;

    QGroupBox* sharpenGroup_ = nullptr;
    QDoubleSpinBox* sharpenAmountSpinBox_ = nullptr;

    QGroupBox* toneCurveGroup_ = nullptr;
    ToneCurveEditor* toneCurveEditor_ = nullptr;

    QGroupBox* equalizerCutGroup_ = nullptr;
    QDoubleSpinBox* startLeftCutSpinBox_ = nullptr;
    QDoubleSpinBox* startRightCutSpinBox_ = nullptr;
    QDoubleSpinBox* endLeftCutSpinBox_ = nullptr;
    QDoubleSpinBox* endRightCutSpinBox_ = nullptr;
};

}  // namespace sound_mind::studio
