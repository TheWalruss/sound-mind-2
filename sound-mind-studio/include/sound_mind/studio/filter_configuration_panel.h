#pragma once

#include <QDockWidget>

#include "sound_mind/core/filter_configuration.h"

class QDoubleSpinBox;
class QLabel;

namespace sound_mind::studio {

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
 * **`FrequencyAxisGradient`-only controls, for now** - `docs/sound-mind-
 * roadmap.md`'s `v0.Y.28.1` (Filter Layers) milestone's own confirmed
 * scope: only this one filter type has a real algorithm behind it yet
 * (see `sound_mind::core::applyFilter()`'s own docs), so this installment's
 * own panel shows only its controls directly - no `FilterType` selector,
 * the same "don't build a selector for types with nothing to select
 * between yet" precedent `ToolConfigurationPanel`'s own docs establish
 * for `ToolType`. A `FilterType` combo (and each other filter's own
 * parameter controls) arrives alongside its own algorithm, in a later
 * installment.
 *
 * **A basic, two-endpoint-stop gradient editor, not a rich visual one** -
 * `Gradient` always has at least its two endpoint stops (`t=0`, `t=1`);
 * this panel edits exactly those two directly, via plain spin boxes (no
 * draggable visual stop editor - nothing in this codebase has built one
 * yet, for any gradient, anywhere). A richer, draggable editor - and
 * interior stops - are deferred; the Equalizer layer's own specialized
 * "Cut" editor (a later installment of this same milestone) will need
 * its own nicer widget regardless, once built.
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

signals:
    /// @brief Emitted whenever any parameter control changes.
    /// @param config The panel's own new, complete configuration.
    void filterConfigurationChanged(const sound_mind::core::FilterConfiguration& config);

private:
    /// @brief Emits filterConfigurationChanged() with the current config_.
    void emitConfigChanged();

    sound_mind::core::FilterConfiguration config_;

    QLabel* frequencyGradientLabel_ = nullptr;
    QDoubleSpinBox* startLeftIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* startLeftOpacitySpinBox_ = nullptr;
    QDoubleSpinBox* startRightIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* startRightOpacitySpinBox_ = nullptr;
    QDoubleSpinBox* endLeftIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* endLeftOpacitySpinBox_ = nullptr;
    QDoubleSpinBox* endRightIntensitySpinBox_ = nullptr;
    QDoubleSpinBox* endRightOpacitySpinBox_ = nullptr;
};

}  // namespace sound_mind::studio
