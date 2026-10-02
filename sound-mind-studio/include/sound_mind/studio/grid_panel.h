#pragma once

#include <vector>

#include <QDockWidget>

#include "sound_mind/studio/axis_labels.h"
#include "sound_mind/studio/grid_config.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief A dockable panel for configuring canvas overlays - Axis Labels,
 *        Overlay Grids (Frequency/Timing), and Snap to Grid - see
 *        `docs/sound-mind-design.md`'s "Overlay Grids"/"Axis Labels", one
 *        panel for every canvas-overlay setting, rather than the legacy
 *        Studio's own split across a View menu, a Tools panel checkbox,
 *        and a separate Overlay dock.
 *
 * Purely presentational, the same division of responsibility as every
 * other dock panel (`ToolConfigurationPanel`, in particular): every
 * control change emits a signal carrying the panel's own new value -
 * `MainWindow` is what actually threads it into `CanvasWidget`/
 * `PickController`/`SelectionController`.
 *
 * **Every control here is session-only Studio UI state, never persisted
 * in the project file** - see `FrequencyGridConfig`'s own docs for why:
 * none of it affects encoding, decoding, or any stored pixel data, only
 * what the canvas currently draws as a reference aid and (for Snap to
 * Grid) where a drag momentarily lands.
 *
 * **Deliberately free-entry only, for now**: Custom Frequencies is a
 * plain Hz list (comma/whitespace-separated); curated preset sets (for
 * either Custom Frequencies or an alternate tuning reference) are
 * deferred - see `FrequencyGridConfig`'s own docs.
 */
class GridPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with axis labels off, no Frequency/Timing
    ///        Grid source active, and Snap to Grid off - matching
    ///        `CanvasWidget`'s/`FrequencyGridConfig`'s/`TimingGridConfig`'s
    ///        own defaults.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit GridPanel(QWidget* parent = nullptr);

    /// @brief The frequency (vertical) axis label mode the panel's own
    ///        combo box currently shows.
    /// @return The current mode.
    [[nodiscard]] VerticalAxisLabelMode verticalAxisLabelMode() const noexcept { return verticalAxisLabelMode_; }

    /// @brief The time (horizontal) axis label mode the panel's own
    ///        combo box currently shows.
    /// @return The current mode.
    [[nodiscard]] HorizontalAxisLabelMode horizontalAxisLabelMode() const noexcept {
        return horizontalAxisLabelMode_;
    }

    /// @brief The Frequency Grid configuration the panel's own controls
    ///        currently describe.
    /// @return The current configuration.
    [[nodiscard]] const FrequencyGridConfig& frequencyGridConfig() const noexcept { return frequencyGridConfig_; }

    /// @brief The Timing Grid configuration the panel's own controls
    ///        currently describe.
    /// @return The current configuration.
    [[nodiscard]] const TimingGridConfig& timingGridConfig() const noexcept { return timingGridConfig_; }

    /// @brief Whether the panel's own "Snap to Grid" checkbox is
    ///        currently checked.
    /// @return `true` if Snap to Grid is on; `false` otherwise.
    [[nodiscard]] bool snapToGridEnabled() const noexcept { return snapToGridEnabled_; }

    /**
     * @brief Applies a Notes checklist result directly - the dialog-free
     *        half of showNotesDialog(), so tests never have to invoke the
     *        real, modal-dialog-showing slot directly (the same
     *        `captureMindShot()`/`captureMindShotWithDetails()` split
     *        this codebase already establishes for every modal-dialog-
     *        showing action - see that pair's own docs).
     *
     * Replaces `frequencyGridConfig().noteGridExcludedSteps` entirely
     * from whichever entries are `false` here - the same "every entry
     * checked by default means no exclusions" convention
     * `showNotesDialog()` itself seeds the real dialog with.
     *
     * @param checkedStates One entry per step-within-octave, in order
     *        (`0` to `stepsPerOctave(frequencyGridConfig().noteGridTemperament) - 1`) -
     *        a shorter list than that leaves the missing tail's own
     *        steps untouched (neither excluded nor re-included).
     */
    void applyNoteSelection(const std::vector<bool>& checkedStates);

    /**
     * @brief Applies an Octaves checklist result directly - see
     *        `applyNoteSelection()`'s own docs for why this exists
     *        alongside showOctavesDialog().
     *
     * Replaces `frequencyGridConfig().noteGridExcludedOctaves` entirely
     * from whichever entries are `false` here.
     *
     * @param checkedStates One entry per octave number, in order,
     *        starting from this panel's own fixed minimum octave (`-1`,
     *        matching `showOctavesDialog()`'s own range) - a shorter
     *        list leaves the missing tail's own octaves untouched.
     */
    void applyOctaveSelection(const std::vector<bool>& checkedStates);

signals:
    /// @brief The "Vertical axis" combo box changed.
    /// @param mode The newly selected mode.
    void verticalAxisLabelModeChanged(sound_mind::studio::VerticalAxisLabelMode mode);

    /// @brief The "Horizontal axis" combo box changed.
    /// @param mode The newly selected mode.
    void horizontalAxisLabelModeChanged(sound_mind::studio::HorizontalAxisLabelMode mode);

    /// @brief Any Frequency Grid control changed (a source checkbox, the
    ///        harmonic fundamental, the custom frequency list, or the
    ///        line style).
    /// @param config The panel's own new, complete Frequency Grid
    ///        configuration.
    void frequencyGridConfigChanged(const sound_mind::studio::FrequencyGridConfig& config);

    /// @brief Any Timing Grid control changed (the mode, the interval,
    ///        the tempo subdivision, or the line style).
    /// @param config The panel's own new, complete Timing Grid
    ///        configuration.
    void timingGridConfigChanged(const sound_mind::studio::TimingGridConfig& config);

    /// @brief The "Snap to Grid" checkbox changed.
    /// @param enabled The new checked state.
    void snapToGridChanged(bool enabled);

private:
    /// @brief Parses customFrequenciesLineEdit_'s own current text into
    ///        frequencyGridConfig_.customFrequenciesHz - a
    ///        comma/whitespace-separated list of positive Hz values;
    ///        anything else in a token (an empty one from adjacent
    ///        separators, non-numeric text, a non-positive value) is
    ///        silently skipped rather than rejecting the whole field, so
    ///        a still-being-typed list never blocks every other already-
    ///        valid entry in it.
    void updateCustomFrequenciesFromText();

    /// @brief Enables/disables harmonicFundamentalSpinBox_/
    ///        customFrequenciesLineEdit_ to match their own checkbox's
    ///        current state - called after every Frequency Grid checkbox
    ///        toggle, so a field never sits editable while its own
    ///        source is off (and silently ignored).
    void updateFrequencyGridControlsEnabled();

    /// @brief Enables/disables keyCombo_/scaleCombo_ to match whether
    ///        `frequencyGridConfig_.noteGridTemperament` currently
    ///        supports Key/Scale filtering at all - see
    ///        `sound_mind::core::supportsKeyAndScale()`'s own docs.
    ///        Called after every Temperament combo change.
    void updateKeyAndScaleControlsEnabled();

    /// @brief Replaces `frequencyGridConfig_.noteGridExcludedSteps` with
    ///        whichever steps `noteGridKey`/`noteGridScale` currently
    ///        resolve to under `noteGridTemperament` - discarding any
    ///        previous manual edit to the list entirely (confirmed with
    ///        the user: a higher-level Key/Scale selection always wins
    ///        over whatever the Notes checklist dialog last left behind).
    ///        A no-op (leaves noteGridExcludedSteps untouched) while
    ///        `noteGridTemperament` doesn't support Key/Scale filtering -
    ///        the caller is still responsible for emptying it directly
    ///        when a Temperament change itself is what's being handled.
    void applyKeyAndScaleToExcludedSteps();

    /// @brief Opens a `CheckListDialog` listing every step-within-octave
    ///        for the current temperament (labeled by pitch class name
    ///        where `supportsKeyAndScale()` makes one available, plain
    ///        step numbers otherwise), seeded from
    ///        `frequencyGridConfig_.noteGridExcludedSteps` - on Accept,
    ///        replaces that set from whichever boxes end up unchecked.
    ///        The actual work behind the Frequency Grid's own "Notes..."
    ///        button.
    void showNotesDialog();

    /// @brief Opens a `CheckListDialog` listing a fixed, generous octave
    ///        range (matching this panel's own note-grid computation's
    ///        own covered span), seeded from
    ///        `frequencyGridConfig_.noteGridExcludedOctaves` - on Accept,
    ///        replaces that set from whichever boxes end up unchecked.
    ///        The actual work behind the Frequency Grid's own
    ///        "Octaves..." button.
    void showOctavesDialog();

    /// @brief Enables/disables timingGridIntervalSpinBox_/
    ///        timingGridSubdivisionCombo_ to match timingGridModeCombo_'s
    ///        own current selection - called after every Timing Grid
    ///        mode change, same reasoning as
    ///        updateFrequencyGridControlsEnabled()'s own docs.
    void updateTimingGridControlsEnabled();

    /// @brief Emits frequencyGridConfigChanged() with the current
    ///        frequencyGridConfig_.
    void emitFrequencyGridConfigChanged();

    /// @brief Emits timingGridConfigChanged() with the current
    ///        timingGridConfig_.
    void emitTimingGridConfigChanged();

    VerticalAxisLabelMode verticalAxisLabelMode_ = VerticalAxisLabelMode::Off;
    HorizontalAxisLabelMode horizontalAxisLabelMode_ = HorizontalAxisLabelMode::Off;
    QComboBox* verticalAxisCombo_ = nullptr;
    QComboBox* horizontalAxisCombo_ = nullptr;

    FrequencyGridConfig frequencyGridConfig_;
    QCheckBox* noteGridCheckBox_ = nullptr;
    QComboBox* temperamentCombo_ = nullptr;
    QComboBox* keyCombo_ = nullptr;
    QComboBox* scaleCombo_ = nullptr;
    QPushButton* notesButton_ = nullptr;
    QPushButton* octavesButton_ = nullptr;
    QCheckBox* harmonicSeriesCheckBox_ = nullptr;
    QDoubleSpinBox* harmonicFundamentalSpinBox_ = nullptr;
    QCheckBox* customFrequenciesCheckBox_ = nullptr;
    QLineEdit* customFrequenciesLineEdit_ = nullptr;
    QPushButton* frequencyGridColorButton_ = nullptr;
    QDoubleSpinBox* frequencyGridWidthSpinBox_ = nullptr;
    QComboBox* frequencyGridDashStyleCombo_ = nullptr;

    TimingGridConfig timingGridConfig_;
    QComboBox* timingGridModeCombo_ = nullptr;
    QDoubleSpinBox* timingGridIntervalSpinBox_ = nullptr;
    QComboBox* timingGridSubdivisionCombo_ = nullptr;
    QPushButton* timingGridColorButton_ = nullptr;
    QDoubleSpinBox* timingGridWidthSpinBox_ = nullptr;
    QComboBox* timingGridDashStyleCombo_ = nullptr;

    bool snapToGridEnabled_ = false;
    QCheckBox* snapToGridCheckBox_ = nullptr;
};

}  // namespace sound_mind::studio
