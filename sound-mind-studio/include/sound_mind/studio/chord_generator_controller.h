#pragma once

#include <vector>

#include <QObject>

#include "sound_mind/core/chord_generator.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class PaintController;

/**
 * @brief Owns the Chord Generator panel's own current parameters, and turns
 *        a canvas click into a real, undoable `SequenceOperation` - see
 *        `docs/sound-mind-design.md`'s "Chords/Arpeggiator/Sequencer",
 *        Installment B (the Chord Generator panel).
 *
 * Purely presentational-adjacent, the same shape as `PaintController`/
 * `PickController`: owns Chord Generator *session* state (the currently
 * configured `sound_mind::core::ChordGeneratorParams`) and calls straight
 * into `sound_mind::core` for the actual chord/arpeggio math
 * (`buildChordNotes()`), but never reaches into `CanvasWidget` or
 * `ChordGeneratorPanel` directly - `previewChanged()` is what `MainWindow`
 * connects to whatever needs to reflect it (the Chord Overlay).
 *
 * **Stamps through whatever `PaintController::toolConfiguration()`
 * currently is** - confirmed with the user as this installment's own scope:
 * rather than a second, parallel instrument picker inside the Chord
 * Generator panel, a chord's own `SequenceOperation::config()` is simply a
 * clone of the Paint tool's own current configuration at the moment of
 * stamping, matching the design doc's own "anything paintable can be the
 * instrument a sequence plays through" framing - switching the Tool
 * Configuration Panel between Procedural/Instrument/Mind Shot/Mind Grain
 * changes what a stamped chord sounds like, exactly as it already does for
 * an ordinary painted stroke.
 *
 * **Re-voicing/re-timing an already-stamped chord is Pick's job, not this
 * class's** - a `SequenceOperation`'s own `Operation::translatedCopy()`
 * already makes it pickable and movable for free (see
 * `docs/sound-mind-architecture.md`'s Decision on this installment); this
 * class only ever builds *new* chords from scratch.
 */
class ChordGeneratorController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a controller with no project set and default
     *        parameters (a C4 Major triad, Block mode).
     * @param paintController Supplies the tool configuration new chords
     *        stamp through, and whose rebuildLayerContent()/
     *        notifyOperationCommitted() this one calls after every stamp -
     *        see this class's own docs on why. Not owned; must outlive
     *        this object.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit ChordGeneratorController(PaintController* paintController, QObject* parent = nullptr);

    /// @brief Sets which project stamping targets.
    /// @param project The project to stamp into; may be `nullptr` (nothing
    ///        stampable until a real one is set again).
    void setProject(sound_mind::core::Project* project);

    /// @brief The parameters new chords/arpeggios are built from.
    /// @return The current parameters.
    [[nodiscard]] const sound_mind::core::ChordGeneratorParams& params() const noexcept { return params_; }

    /**
     * @brief Sets the parameters new chords/arpeggios are built from, and
     *        emits previewChanged() - every control in the Chord Generator
     *        panel calls this on edit, live, so the Chord Overlay always
     *        reflects whatever is currently configured, before anything is
     *        stamped.
     * @param params The new parameters. `params.startTimeSeconds` is
     *        ignored here (and by previewNotes(), which always previews at
     *        `0.0`) - only stampAt() ever resolves a real start time, from
     *        wherever the user actually clicks.
     */
    void setParams(sound_mind::core::ChordGeneratorParams params);

    /**
     * @brief The current parameters' own note pitches, for the Chord
     *        Overlay - `docs/sound-mind-design.md`'s "Chord Overlay".
     *
     * Built via `buildChordNotes()` at `startTimeSeconds = 0.0` (the actual
     * start time is meaningless for a pitch-only preview - see
     * `setChordPreview()`'s own docs on why the overlay only ever draws
     * frequency, not time, positions), then deduplicated - an arpeggio
     * revisiting the same note on a later repeat only draws one line for
     * it, not an overlapping stack.
     *
     * @return Every distinct note pitch the current params() would stamp,
     *         in Hz, ascending.
     */
    [[nodiscard]] std::vector<double> previewFrequenciesHz() const;

    /**
     * @brief Resolves params() at a real start time and appends the result
     *        as a new `SequenceOperation` to the project's own
     *        `OperationLog`, then rebuilds `targetLayer`'s own painted
     *        content - a no-op if no project is set, or if params().chordIndex
     *        is out of range for params().category (see `buildChordNotes()`'s
     *        own docs).
     *
     * @param targetLayer Which layer to stamp into.
     * @param timeSeconds When the chord starts, in seconds - ordinarily
     *        wherever the user clicked on the canvas's own time axis (see
     *        `CanvasWidget::chordStampRequested()`'s own docs on why only
     *        the time axis, not frequency, comes from the click).
     */
    void stampAt(sound_mind::core::LayerId targetLayer, double timeSeconds);

signals:
    /// @brief Emitted whenever params() changes - see setParams()'s own
    ///        docs.
    void previewChanged();

    /// @brief Emitted whenever a layer's own rendered content changes as a
    ///        result of stampAt().
    /// @param layer Which layer's content changed.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;
    sound_mind::core::ChordGeneratorParams params_;
};

}  // namespace sound_mind::studio
