#pragma once

#include <QDockWidget>

#include "sound_mind/core/chord_generator.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace sound_mind::studio {

/**
 * @brief A dockable panel for configuring the Chord Generator - see
 *        `docs/sound-mind-design.md`'s "Chords/Arpeggiator/Sequencer"
 *        ("The Chord Generator is the common, structured case... choose a
 *        root note, a chord category... and a specific voicing, and either
 *        sound every note together (block chord) or sequence them one at a
 *        time (arpeggio) in a chosen order and rhythm").
 *
 * **"Voicing" is "which named chord within the category"** - confirmed with
 * the user as this installment's own scope (see
 * `sound_mind::core::ChordGeneratorParams`'s own docs); no separate
 * inversion/octave-doubling control.
 *
 * **No instrument picker of its own** - a stamped chord always plays
 * through whatever `PaintController::toolConfiguration()` (the Tool
 * Configuration Panel's own current selection) currently is, per
 * `ChordGeneratorController`'s own docs; this panel is purely "what to
 * play", not "through what".
 *
 * **Arming a stamp is a toolbar action, not a button on this panel** -
 * `MainWindow`'s own `ChordStamp` tool mode toggle (the same kind of plain
 * checkable toolbar action as Paint/Pick/Select/Path), matching every other
 * canvas placement gesture in this codebase, rather than a redundant
 * "Stamp" button here. Every control here only ever edits params() and
 * live-updates the Chord Overlay (`paramsChanged()`) - what an actual click
 * on the canvas then does is `MainWindow`'s/`ChordGeneratorController`'s
 * own job.
 *
 * Purely presentational, the same division of responsibility as every
 * other dock panel: every edit emits paramsChanged() with the panel's own
 * current, complete `ChordGeneratorParams` (`startTimeSeconds` always `0.0`
 * here - only a real canvas click ever resolves a real one).
 */
class ChordGeneratorPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief Builds the panel with default parameters (a C4 Major triad,
    ///        Block mode, one second long).
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit ChordGeneratorPanel(QWidget* parent = nullptr);

    /// @brief The parameters this panel's controls currently describe.
    /// @return The current parameters; `startTimeSeconds` is always `0.0`.
    [[nodiscard]] const sound_mind::core::ChordGeneratorParams& params() const noexcept { return params_; }

signals:
    /// @brief Emitted whenever any control changes.
    /// @param params The panel's own new, complete parameters.
    void paramsChanged(const sound_mind::core::ChordGeneratorParams& params);

private:
    /// @brief Repopulates chordCombo_ with chordsInCategory(params_.category)'s
    ///        own entries, keeping as close as possible to the previously
    ///        selected chord's own index (clamped, not name-matched - the
    ///        same "index survives a category switch, name doesn't"
    ///        behavior `ToolConfigurationPanel::changeToolType()`'s own
    ///        field carry-over establishes for shared fields).
    void rebuildChordCombo();

    /// @brief Shows/hides blockGroup_/arpeggioGroup_ (and, within it,
    ///        customIndicesLineEdit_/randomSeedSpinBox_) for params_.mode/
    ///        params_.order, the same per-type dynamic visibility
    ///        `ToolConfigurationPanel::changeToolType()`'s own docs
    ///        describe.
    void updateGroupVisibility();

    /// @brief Recomputes params_.rootMidiNote from rootCombo_/octaveSpinBox_,
    ///        refreshes notesPreviewLabel_, and emits paramsChanged().
    void emitParamsChanged();

    QComboBox* rootCombo_ = nullptr;
    QSpinBox* octaveSpinBox_ = nullptr;
    QComboBox* categoryCombo_ = nullptr;
    QComboBox* chordCombo_ = nullptr;
    QLabel* notesPreviewLabel_ = nullptr;

    QComboBox* modeCombo_ = nullptr;

    QWidget* blockGroup_ = nullptr;
    QDoubleSpinBox* blockDurationSpinBox_ = nullptr;

    QWidget* arpeggioGroup_ = nullptr;
    QComboBox* orderCombo_ = nullptr;
    QLineEdit* customIndicesLineEdit_ = nullptr;
    QDoubleSpinBox* bpmSpinBox_ = nullptr;
    QComboBox* subdivisionCombo_ = nullptr;
    QSpinBox* noteDurationPercentSpinBox_ = nullptr;
    QSpinBox* repeatsSpinBox_ = nullptr;
    QSpinBox* randomSeedSpinBox_ = nullptr;

    sound_mind::core::ChordGeneratorParams params_;
};

}  // namespace sound_mind::studio
