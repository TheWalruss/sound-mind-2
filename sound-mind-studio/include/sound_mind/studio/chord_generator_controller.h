#pragma once

#include <string>
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
 *
 * **As of `v0.0.40.3` (Chords/Arpeggiator/Sequencer, Installment C):** a
 * second, independent source of what to stamp - `setNotation()`, resolving
 * Sound Mind's own text notation (`sequence_notation.h`) instead of a
 * built chord. Only one source is ever "current" at a time (whichever of
 * `setParams()`/`setNotation()` was called most recently) -
 * `previewFrequenciesHz()`/`stampAt()` both dispatch on it internally, so
 * neither `CanvasWidget` nor `MainWindow` needs to know or care which kind
 * of input produced the notes currently being previewed/stamped, matching
 * the design doc's own "the same underlying notation... generated the same
 * way a chord is" framing. Invalid notation text is handled gracefully,
 * not thrown across the Qt signal/slot boundary: `previewFrequenciesHz()`
 * returns empty and `stampAt()` does nothing, the same "nothing happens by
 * accident" fallback `buildChordNotes()`'s own out-of-range-index handling
 * already establishes - `ChordGeneratorPanel`'s own live inline validation
 * (a direct call to `parseSequenceNotation()`, independent of this class)
 * is what actually surfaces a parse error to the user, before they ever
 * try to stamp.
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
     * @brief Sets the notation text new sequences are parsed from, and
     *        emits previewChanged() - `ChordGeneratorPanel`'s own Custom
     *        Notation controls call this on edit, the same live-update role
     *        setParams() plays for the Chord Builder controls. Switches
     *        this controller's own current source to notation - a later
     *        `previewFrequenciesHz()`/`stampAt()` call parses `notation`
     *        fresh via `parseSequenceNotation()`, not a snapshot taken here.
     * @param notation The notation text - see `parseSequenceNotation()`'s
     *        own docs for the grammar; invalid text is accepted here
     *        without error (see this class's own docs on why).
     * @param referenceHz The tuning reference for note names in `notation` -
     *        see `parseSequenceNotation()`'s own docs.
     * @param bpm The tempo beats-suffixed durations in `notation` resolve
     *        against - see `parseSequenceNotation()`'s own docs.
     */
    void setNotation(std::string notation, double referenceHz, double bpm);

    /**
     * @brief The current source's own note pitches, for the Chord Overlay -
     *        `docs/sound-mind-design.md`'s "Chord Overlay".
     *
     * Resolved at `startTimeSeconds = 0.0` (the actual start time is
     * meaningless for a pitch-only preview - see `setChordPreview()`'s own
     * docs on why the overlay only ever draws frequency, not time,
     * positions), then deduplicated - an arpeggio (or a notation sequence)
     * revisiting the same note more than once only draws one line for it,
     * not an overlapping stack.
     *
     * @return Every distinct note pitch the current source would stamp, in
     *         Hz, ascending - empty if the current source is invalid
     *         notation text (see this class's own docs).
     */
    [[nodiscard]] std::vector<double> previewFrequenciesHz() const;

    /**
     * @brief Resolves the current source at a real start time and appends
     *        the result as a new `SequenceOperation` to the project's own
     *        `OperationLog`, then rebuilds `targetLayer`'s own painted
     *        content - a no-op if no project is set, if the current source
     *        is a Chord Builder chord with an out-of-range `chordIndex`
     *        (see `buildChordNotes()`'s own docs), or if it's invalid
     *        notation text (see this class's own docs).
     *
     * @param targetLayer Which layer to stamp into.
     * @param timeSeconds When the sequence starts, in seconds - ordinarily
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
    /// @brief Which of setParams()/setNotation() was called most recently -
    ///        see this class's own docs on why only one source is ever
    ///        current at a time.
    enum class InputSource { ChordBuilder, Notation };

    /// @brief The current source's own notes at `startTimeSeconds` - the
    ///        shared resolution logic both previewFrequenciesHz() and
    ///        stampAt() build on, dispatching on inputSource_. Invalid
    ///        notation text is caught here and resolved to an empty
    ///        result, never thrown out of this method.
    [[nodiscard]] std::vector<sound_mind::core::NoteEvent> resolveNotes(double startTimeSeconds) const;

    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;
    InputSource inputSource_ = InputSource::ChordBuilder;
    sound_mind::core::ChordGeneratorParams params_;
    std::string notation_;
    double notationReferenceHz_ = 440.0;
    double notationBpm_ = 120.0;
};

}  // namespace sound_mind::studio
