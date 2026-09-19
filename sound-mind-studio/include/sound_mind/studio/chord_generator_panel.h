#pragma once

#include <QDockWidget>
#include <QString>

#include "sound_mind/core/chord_generator.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
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
 *
 * **As of `v0.0.40.3` (Chords/Arpeggiator/Sequencer, Installment C):** an
 * "Input Mode" combo (`inputModeCombo_`) switches this panel between the
 * Chord Builder controls above and a second, independent **Custom
 * Notation** mode - a plain text box (`notationTextEdit_`) for Sound
 * Mind's own compact sequence notation (`sequence_notation.h`), plus its
 * own BPM/Reference Hz spin boxes (self-contained, the same way
 * `ChordGeneratorParams::bpm`/`referenceHz` already default independently
 * of any live project setting, rather than reading `ProjectSettings`
 * directly). Only one mode's own group is ever visible at a time, the same
 * per-mode dynamic visibility `updateGroupVisibility()`'s own docs already
 * describe for Block/Arpeggio. Emits notationChanged() instead of
 * paramsChanged() while in this mode - **validated locally, live, as you
 * type**: a direct call to `sound_mind::core::parseSequenceNotation()`
 * (independent of `ChordGeneratorController`, which handles an invalid
 * string gracefully on its own - see its own docs) drives
 * `notationErrorLabel_`, so a typo is visible immediately rather than only
 * once you try to stamp.
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
    /// @brief Emitted whenever a Chord Builder control changes, while
    ///        Input Mode is Chord Builder.
    /// @param params The panel's own new, complete parameters.
    void paramsChanged(const sound_mind::core::ChordGeneratorParams& params);

    /// @brief Emitted whenever a Custom Notation control changes, while
    ///        Input Mode is Custom Notation - see this class's own docs.
    /// @param notation The current notation text - may be invalid; see
    ///        `sound_mind::core::parseSequenceNotation()`'s own docs.
    /// @param referenceHz The current Reference Hz spin box value.
    /// @param bpm The current BPM spin box value.
    void notationChanged(const QString& notation, double referenceHz, double bpm);

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

    /// @brief Re-validates notationTextEdit_'s own current text (a direct
    ///        `parseSequenceNotation()` call, updating notationErrorLabel_'s
    ///        own text/visibility) and emits notationChanged() regardless
    ///        of whether it's currently valid - see this class's own docs.
    void emitNotationChanged();

    /// @brief Shows chordBuilderGroup_/hides notationGroup_, or vice versa,
    ///        for inputModeCombo_'s own current selection, and emits
    ///        whichever of paramsChanged()/notationChanged() applies to the
    ///        newly active mode - so a mode switch alone (before touching
    ///        any other control) still tells `ChordGeneratorController`
    ///        which source is now current.
    void updateInputModeVisibility();

    QComboBox* inputModeCombo_ = nullptr;
    QWidget* chordBuilderGroup_ = nullptr;

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

    QWidget* notationGroup_ = nullptr;
    QPlainTextEdit* notationTextEdit_ = nullptr;
    QDoubleSpinBox* notationBpmSpinBox_ = nullptr;
    QDoubleSpinBox* notationReferenceHzSpinBox_ = nullptr;
    QLabel* notationErrorLabel_ = nullptr;

    sound_mind::core::ChordGeneratorParams params_;
};

}  // namespace sound_mind::studio
