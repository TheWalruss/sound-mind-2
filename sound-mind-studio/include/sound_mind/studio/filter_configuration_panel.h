#pragma once

#include <utility>
#include <vector>

#include <QDockWidget>
#include <QString>

#include "sound_mind/core/convolution_kernel.h"
#include "sound_mind/core/filter_configuration.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
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
 * Layers) milestone's own multi-installment scope built the first six;
 * `v0.Y.36.1` (Deferred Filters) Installment A added the eight "Noise &
 * distortion" types below them, in `docs/sound-mind-design.md`'s own
 * family order (Blur & focus, then Noise & distortion, then Tonal, then
 * Spectral shaping) - so this selector now lists fourteen. Selecting a
 * type shows only that type's own parameter group; every other group
 * stays hidden (`QWidget::setVisible(false)`), the same "meaningless
 * unless `type()` matches" contract `FilterConfiguration`'s own
 * per-field docs already state. **None of the eight Noise & distortion
 * groups offer a MindWave-binding combo** - matching
 * `FilterConfiguration`'s own docs on why binding those eight is
 * deferred, not built here. **`SpeckleAdd`'s own Density/Intensity
 * controls and `DynamicSpeckle`'s own are separate widgets that both
 * read/write the same underlying `speckleDensity()`/`speckleIntensity()`
 * fields** - the two types share those fields, but each gets its own
 * group (with its own tooltip explaining the live-vs-deterministic
 * distinction) rather than one group doing double duty under two
 * different titles. **Editing either group's own Density/Intensity spin
 * box immediately pushes the same value onto its sibling in the other
 * group too** (each change handler updates the other widget directly,
 * `QSignalBlocker`-guarded against re-triggering itself) - without this,
 * switching the `FilterType` combo between the two would show a stale
 * value on whichever group wasn't visible while the other's own field was
 * last edited (caught by a test written for exactly this scenario, not by
 * inspection).
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
 * **A MindWave-binding combo next to each of the five bindable parameters**
 * (`blurSigma`, `medianSize`, `directionalBlurLength`,
 * `directionalBlurAngleDegrees`, `sharpenAmount` - see `sound_mind::core::
 * FilterConfiguration`'s own docs, `v0.Y.31.1` Installment D3) - "None" or
 * any MindWave `setAvailableMindWaves()` currently lists. `ToneCurve`'s
 * own curve and `FrequencyAxisGradient`'s own gradient have no such combo,
 * matching `FilterConfiguration`'s own docs on why: neither is a single
 * number.
 *
 * **`ChannelBalance`/`Invert`/`Convolve`** (`v0.Y.36.1` Installment B) round
 * out Tonal/Spectral shaping, in `docs/sound-mind-design.md`'s own listed
 * sub-order within each family (`ToneCurve` then `ChannelBalance` then
 * `Invert` for Tonal; `FrequencyAxisGradient` then `Convolve` for Spectral
 * shaping) - the selector now lists seventeen types. Neither binds to a
 * MindWave, the same deferral Installment A's eight already established.
 * `Invert` has no parameters of its own - its own group is just an
 * explanatory label. **`Convolve` is full legacy parity, confirmed with
 * the user**: a configurable odd kernel size (a fresh, identity kernel of
 * the new size replaces the old one on a size change - editing is
 * per-size, not preserved across a resize), a size-by-size grid of plain
 * spin boxes rebuilt from scratch on every size change
 * (`rebuildConvolveKernelGrid()`), eight classic built-in presets, a
 * Normalize checkbox, an Amount (dry/wet) spin box, and a small
 * save/load library UI (`setAvailableConvolutionKernels()` mirrors
 * `setAvailableMindWaves()`'s own role, fed by `MainWindow` from
 * `Project::convolutionKernels()`). Both the Preset combo and the Load
 * Kernel combo are one-shot triggers, not sticky selections - applying
 * either immediately writes new kernel values into the grid and then
 * resets its own combo back to a leading placeholder item, since the
 * loaded/preset values are just a starting point for further hand-editing,
 * not a persistent link back to that preset or saved entry (editing the
 * grid afterward never mutates a saved library entry, and removing one
 * from the library never affects a Filter layer that already loaded it -
 * see `NamedConvolutionKernel`'s own docs).
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

    /**
     * @brief Sets which MindWaves each of the five bindable parameters'
     *        own combo can offer - `MindWaveController`'s own answer to
     *        keeping this panel in sync with the project's current
     *        MindWave library (`v0.Y.31.1` Installment D3), the same
     *        role `LayersPanel::setAvailableMindWaves()` already plays
     *        for a layer's own opacity binding.
     *
     * Immediately rebuilds every combo's own item list (not deferred to
     * the next `setFilterConfiguration()` call), preserving each combo's
     * own current selection when the bound id is still present among
     * `mindWaves`.
     *
     * @param mindWaves Every current library entry's own id/name, in
     *        whatever order they should appear in each combo (after a
     *        leading "None" entry, always first).
     */
    void setAvailableMindWaves(const std::vector<std::pair<sound_mind::core::MindWaveId, QString>>& mindWaves);

    /**
     * @brief Sets which saved convolution kernels the Load Kernel combo can
     *        offer - `MainWindow`'s own answer to keeping this panel in
     *        sync with the project's current convolution kernel library
     *        (`v0.Y.36.1` Installment B), the same role `setAvailableMindWaves()`
     *        already plays for MindWave bindings.
     *
     * Takes full `NamedConvolutionKernel` entries, not just id/name pairs
     * like `setAvailableMindWaves()` - unlike a MindWave binding (a
     * persistent id reference `applyFilter()` resolves fresh every time),
     * loading a kernel here copies its `size`/`coefficients`/`normalize`
     * directly into `config_` on selection, so this panel needs the actual
     * data up front, not just enough to label a combo entry.
     *
     * Immediately rebuilds the combo's own item list (not deferred to the
     * next `setFilterConfiguration()` call). The combo has no "current
     * selection" of its own to preserve - see this class's own docs on why
     * loading a kernel is a one-shot trigger, not a sticky binding.
     *
     * @param kernels Every current library entry, in whatever order they
     *        should appear in the combo (after a leading placeholder
     *        entry, always first).
     */
    void setAvailableConvolutionKernels(const std::vector<sound_mind::core::NamedConvolutionKernel>& kernels);

signals:
    /// @brief Emitted whenever any parameter control changes.
    /// @param config The panel's own new, complete configuration.
    void filterConfigurationChanged(const sound_mind::core::FilterConfiguration& config);

    /**
     * @brief Emitted when the user clicks Convolve's own "Save As New
     *        Kernel" button - `MainWindow`'s cue to append a new entry to
     *        the project's own convolution kernel library
     *        (`Project::addConvolutionKernel()`) and refresh this panel's
     *        own Load Kernel combo via `setAvailableConvolutionKernels()`.
     *        This panel has no `Project` reference of its own, the same
     *        boundary every other cross-Project action in this codebase
     *        keeps.
     * @param size The kernel's own current side length.
     * @param coefficients The kernel's own current coefficients, row-major.
     * @param normalize The kernel's own current Normalize setting.
     */
    void saveConvolutionKernelRequested(int size, std::vector<float> coefficients, bool normalize);

private:
    /// @brief Emits filterConfigurationChanged() with the current config_.
    void emitConfigChanged();

    /// @brief Shows the one parameter group matching `config_.type()` and
    ///        hides every other one - see this class's own docs.
    void updateVisibleGroup();

    /// @brief Rebuilds every MindWave-binding combo's own item list from
    ///        `availableMindWaves_`, preserving each combo's own current
    ///        selection if the bound id is still present.
    void rebuildMindWaveCombos();

    /**
     * @brief Destroys and recreates `convolveKernelSpinBoxes_` for a new
     *        `size` x `size` kernel, laid out in `convolveKernelGridLayout_`,
     *        each one wired to `applyConvolveKernelFromGrid()` - the actual
     *        work behind every kernel-size change, and behind syncing to a
     *        freshly loaded `FilterConfiguration`. Reads initial values
     *        from `config_.convolveKernel()` if it already has exactly
     *        `size * size` entries (a `setFilterConfiguration()` sync);
     *        otherwise seeds a fresh identity kernel (a live size change).
     * @param size The new kernel size - forced odd/positive by the caller.
     */
    void rebuildConvolveKernelGrid(int size);

    /// @brief Reads every spin box in `convolveKernelSpinBoxes_` back into
    ///        `config_.convolveKernel()` and emits filterConfigurationChanged()
    ///        - every cell's own change handler calls this, the same
    ///        "re-read the whole group's own values" pattern
    ///        Frequency-Axis Gradient's own stop editor already establishes.
    void applyConvolveKernelFromGrid();

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
    QComboBox* blurSigmaMindWaveCombo_ = nullptr;

    QGroupBox* edgePreservingBlurGroup_ = nullptr;
    QSpinBox* medianSizeSpinBox_ = nullptr;
    QComboBox* medianSizeMindWaveCombo_ = nullptr;

    QGroupBox* directionalBlurGroup_ = nullptr;
    QSpinBox* directionalBlurLengthSpinBox_ = nullptr;
    QComboBox* directionalBlurLengthMindWaveCombo_ = nullptr;
    QDoubleSpinBox* directionalBlurAngleSpinBox_ = nullptr;
    QComboBox* directionalBlurAngleMindWaveCombo_ = nullptr;

    QGroupBox* sharpenGroup_ = nullptr;
    QDoubleSpinBox* sharpenAmountSpinBox_ = nullptr;
    QComboBox* sharpenAmountMindWaveCombo_ = nullptr;

    /// @brief See setAvailableMindWaves()'s own docs.
    std::vector<std::pair<sound_mind::core::MindWaveId, QString>> availableMindWaves_;

    // --- v0.Y.36.1 Installment A: Noise & distortion - none of these
    // eight bind to a MindWave, see this class's own docs.

    QGroupBox* speckleAddGroup_ = nullptr;
    QDoubleSpinBox* speckleAddDensitySpinBox_ = nullptr;
    QDoubleSpinBox* speckleAddIntensitySpinBox_ = nullptr;

    QGroupBox* speckleRemoveGroup_ = nullptr;
    QDoubleSpinBox* speckleThresholdSpinBox_ = nullptr;

    QGroupBox* denoiseGroup_ = nullptr;
    QDoubleSpinBox* noiseFloorSpinBox_ = nullptr;
    QDoubleSpinBox* reductionSpinBox_ = nullptr;

    QGroupBox* bitDepthCrushGroup_ = nullptr;
    QDoubleSpinBox* crushAmountSpinBox_ = nullptr;

    QGroupBox* granularNoiseGroup_ = nullptr;
    QSpinBox* grainSizeSpinBox_ = nullptr;
    QDoubleSpinBox* grainAmountSpinBox_ = nullptr;

    QGroupBox* dynamicSpeckleGroup_ = nullptr;
    QDoubleSpinBox* dynamicSpeckleDensitySpinBox_ = nullptr;
    QDoubleSpinBox* dynamicSpeckleIntensitySpinBox_ = nullptr;

    QGroupBox* feedbackDistortionGroup_ = nullptr;
    QDoubleSpinBox* feedbackAmountSpinBox_ = nullptr;

    QGroupBox* spectralWavefoldGroup_ = nullptr;
    QDoubleSpinBox* foldGainSpinBox_ = nullptr;

    QGroupBox* toneCurveGroup_ = nullptr;
    ToneCurveEditor* toneCurveEditor_ = nullptr;

    // --- v0.Y.36.1 Installment B: the rest of Tonal/Spectral shaping -
    // none of these three bind to a MindWave either, see this class's own
    // docs.

    QGroupBox* channelBalanceGroup_ = nullptr;
    QDoubleSpinBox* channelBalanceSpinBox_ = nullptr;

    QGroupBox* invertGroup_ = nullptr;

    QGroupBox* convolveGroup_ = nullptr;
    QComboBox* convolvePresetCombo_ = nullptr;
    QSpinBox* convolveKernelSizeSpinBox_ = nullptr;
    QWidget* convolveKernelGridContainer_ = nullptr;
    QGridLayout* convolveKernelGridLayout_ = nullptr;
    std::vector<QDoubleSpinBox*> convolveKernelSpinBoxes_;
    QCheckBox* convolveNormalizeCheckBox_ = nullptr;
    QDoubleSpinBox* convolveAmountSpinBox_ = nullptr;
    QPushButton* convolveSaveKernelButton_ = nullptr;
    QComboBox* convolveLoadKernelCombo_ = nullptr;
    /// @brief See setAvailableConvolutionKernels()'s own docs.
    std::vector<sound_mind::core::NamedConvolutionKernel> availableConvolutionKernels_;

    QGroupBox* equalizerCutGroup_ = nullptr;
    QDoubleSpinBox* startLeftCutSpinBox_ = nullptr;
    QDoubleSpinBox* startRightCutSpinBox_ = nullptr;
    QDoubleSpinBox* endLeftCutSpinBox_ = nullptr;
    QDoubleSpinBox* endRightCutSpinBox_ = nullptr;
};

}  // namespace sound_mind::studio
