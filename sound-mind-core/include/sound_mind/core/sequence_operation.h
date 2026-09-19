#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

/**
 * @brief A single note within a `SequenceOperation` - a pitch and a
 *        timing, per `docs/sound-mind-design.md`'s "Chords/Arpeggiator/
 *        Sequencer" ("A sequence is a written description of a series of
 *        notes or frequencies over time"). `v0.Y.40.1` Installment A.
 *
 * Deliberately just pitch and timing, matching the design doc's own literal
 * definition of a note event - no per-note intensity/opacity/color of its
 * own. A `SequenceOperation`'s own shared `config()` supplies that (its
 * `defaultGradient()`), the same way a single painted stroke's own gradient
 * applies uniformly along its length - every note in one sequence plays
 * through the same instrument/gradient, not a per-note one.
 *
 * A raw Hz frequency, not a note name - `docs/sound-mind-design.md`'s own
 * "arbitrary frequencies, not just named pitches" requirement (Deferred
 * Decision #4) applies here at the representation level already: whatever
 * note-name-to-Hz mapping a future Chord Generator or notation parser uses
 * (Installments B/C) resolves to a plain Hz value before it ever reaches
 * this struct.
 */
struct NoteEvent {
    /// @brief When this note starts, in seconds from the project's start.
    double startTimeSeconds = 0.0;

    /// @brief How long this note lasts, in seconds. `0` (or negative) is a
    ///        single instantaneous stamp rather than a held tone - see
    ///        `applySequenceOperation()`'s own docs for exactly how this
    ///        renders.
    double durationSeconds = 1.0;

    /// @brief This note's own pitch, in Hz.
    double frequencyHz = 440.0;
};

/// @brief Serializes a note event to its JSON representation.
void to_json(nlohmann::json& json, const NoteEvent& note);

/// @brief Parses a note event from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NoteEvent& note);

/**
 * @brief A written series of notes, rendered as paint events through a
 *        shared tool configuration - logged non-destructively, see
 *        `docs/sound-mind-design.md`'s "Chords/Arpeggiator/Sequencer" and
 *        `docs/sound-mind-architecture.md`'s Core Data Model. `v0.Y.40.1`
 *        Installment A.
 *
 * The fifth concrete `Operation` subtype (after `PaintOperation`,
 * `FillOperation`, `PasteOperation`, `WarpOperation`). **The core
 * "generated sequences stay editable" mechanism**: this operation stores
 * `notes()` themselves, not baked pixels - `applySequenceOperation()`
 * regenerates every note's own stamp fresh, every time a layer's content is
 * rebuilt (`rebuildPaintedContent()`, the same replay-from-the-log
 * architecture undo/redo already relies on). Re-voicing (different
 * `frequencyHz` values) or re-timing (different `startTimeSeconds`/
 * `durationSeconds`) a sequence therefore never needs repainting -
 * matching `docs/sound-mind-design.md`'s own "re-voice it, re-time it, or
 * point it at a different instrument, without repainting it from scratch."
 *
 * **Immutable once constructed, like every other `Operation` subtype**
 * (see `Operation::supersedes()`'s own docs on the append-only log): a
 * "re-voice" or "re-time" edit is a Studio-level action that constructs a
 * *new* `SequenceOperation` (with the edited `notes()`/`config()`) and
 * appends it with `supersedes()` pointing at the one it replaces - not an
 * in-place mutation of `notes()` on an already-logged instance. There is
 * deliberately no `setNotes()`/mutable `notes()` accessor here.
 *
 * **`config()` is "any paintable tip"** (`docs/sound-mind-design.md`'s own
 * framing) - a plain procedural brush, a Sound Mind Instrument, a Mind
 * Shot, or a Mind Grain, the same `std::unique_ptr<ToolConfiguration>`
 * every note in this sequence shares, matching `PaintOperation::config()`'s
 * own "owned snapshot, not a shared reference" precedent exactly.
 */
class SequenceOperation : public LayerContentOperation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer's content this sequence paints into.
     * @param notes This sequence's own notes, in any order - see
     *        `notes()`'s own docs.
     * @param config The tool configuration every note in this sequence
     *        plays through - an owned snapshot, not a shared reference.
     *        `ToolConfiguration` is abstract (see its own docs), so this
     *        is always a `unique_ptr` to some concrete subtype - never
     *        null.
     * @param supersedes The prior operation this one replaces, if any -
     *        see `Operation::supersedes()`'s own docs.
     */
    SequenceOperation(OperationId id, LayerId targetLayer, std::vector<NoteEvent> notes,
                       std::unique_ptr<ToolConfiguration> config,
                       std::optional<OperationId> supersedes = std::nullopt) noexcept
        : LayerContentOperation(id, targetLayer, supersedes), notes_(std::move(notes)), config_(std::move(config)) {}

    /**
     * @brief This operation's own time/frequency footprint - the union of
     *        every note's own `[startTimeSeconds, startTimeSeconds +
     *        durationSeconds]` time span and `frequencyHz` (a zero-height
     *        span per note, but the overall rect spans every note's own
     *        pitch).
     * @return The bounding rectangle; all-zero for a sequence with no notes.
     */
    [[nodiscard]] TimeFrequencyRect bounds() const override {
        if (notes_.empty()) {
            return TimeFrequencyRect{};
        }
        TimeFrequencyRect rect{notes_.front().startTimeSeconds,
                                notes_.front().startTimeSeconds + notes_.front().durationSeconds,
                                notes_.front().frequencyHz, notes_.front().frequencyHz};
        for (const NoteEvent& note : notes_) {
            rect.startTimeSeconds = std::min(rect.startTimeSeconds, note.startTimeSeconds);
            rect.endTimeSeconds = std::max(rect.endTimeSeconds, note.startTimeSeconds + note.durationSeconds);
            rect.lowFrequencyHz = std::min(rect.lowFrequencyHz, note.frequencyHz);
            rect.highFrequencyHz = std::max(rect.highFrequencyHz, note.frequencyHz);
        }
        return rect;
    }

    /// @brief This sequence's own notes - see this class's own docs for
    ///        why there's no mutable accessor.
    /// @return The current notes, in construction order.
    [[nodiscard]] const std::vector<NoteEvent>& notes() const noexcept { return notes_; }

    /// @brief The tool configuration every note in this sequence plays
    ///        through.
    /// @return This operation's own tool configuration.
    [[nodiscard]] const ToolConfiguration& config() const noexcept { return *config_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override;

private:
    std::vector<NoteEvent> notes_;
    std::unique_ptr<ToolConfiguration> config_;
};

}  // namespace sound_mind::core
