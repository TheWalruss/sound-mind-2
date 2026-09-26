#pragma once

#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_wave.h"

namespace sound_mind::studio {

/**
 * @brief One kind of project action a `MacroRecorder` can capture -
 *        `docs/sound-mind-roadmap.md`'s `v0.Y.49.1` (Macro Mode)
 *        Installment A's own confirmed recording scope: exactly the
 *        actions the roadmap's own prose names ("starting Playback, then
 *        activating/hiding layers, painting, modifying filter settings,
 *        changing MindWave configurations").
 */
enum class MacroEventType {
    PlaybackStarted,
    PlaybackStopped,
    LayerVisibilityChanged,
    PaintCommitted,
    FilterConfigurationChanged,
    MindWaveConfigurationChanged,
};

/**
 * @brief One recorded action, timestamped against the project's own
 *        playback position - see `MacroRecorder::recordEvent()`'s own
 *        docs for why playback-position seconds, not wall-clock time.
 *
 * Deliberately not yet a fully executable "replay instruction" - this
 * installment only proves out the recording/storage half (`v0.Y.49.1`
 * Installment A); a `description` (the same human-readable-label
 * precedent `UndoStack::UndoCommand` already established for its own
 * History Panel) plus the affected layer/MindWave id where applicable is
 * enough to inspect what a macro contains, without yet committing to
 * exactly what data a future replay/export installment will need to
 * reconstruct the action precisely.
 */
struct MacroEvent {
    /// @brief Playback position, in seconds, this event was recorded at -
    ///        see this struct's own docs on why playback-position seconds,
    ///        not wall-clock time.
    double timestampSeconds = 0.0;
    /// @brief What kind of action this event records.
    MacroEventType type = MacroEventType::PlaybackStarted;
    /// @brief A human-readable label for this event - the same precedent
    ///        `UndoStack::UndoCommand` already established for its own
    ///        History Panel.
    QString description;
    /// @brief The layer this event affected, if applicable.
    std::optional<sound_mind::core::LayerId> layerId;
    /// @brief The MindWave this event affected, if applicable.
    std::optional<sound_mind::core::MindWaveId> mindWaveId;
    /// @brief `UndoStack::currentIndex()` immediately after this event's
    ///        own mutation was pushed - `v0.Y.49.1` Installment B's own
    ///        replay mechanism (`MainWindow::playMacro()`) jumps to this
    ///        index (via `UndoStack::jumpTo()`) once playback reaches
    ///        `timestampSeconds`. Meaningless for `PlaybackStarted`/
    ///        `PlaybackStopped` (neither is ever pushed to `UndoStack`) -
    ///        `playMacro()` skips both event types entirely rather than
    ///        jumping to this field for them.
    std::size_t undoStackIndexAfter = 0;
};

/**
 * @brief Records a timestamped sequence of project actions while active -
 *        `docs/sound-mind-roadmap.md`'s `v0.Y.49.1` (Macro Mode)
 *        Installment A.
 *
 * Session-only, matching `UndoStack`'s own precedent (`undo_stack.h`'s own
 * docs) - not persisted with the project, and not yet consumed by
 * anything (no in-app scripted playback, no video export) - those are
 * each their own later installment, deliberately deferred (confirmed with
 * the user) so this one installment can focus on proving out the
 * recording/storage data model alone.
 *
 * `recordEvent()` is a no-op whenever `isRecording()` is `false` - every
 * call site in `MainWindow` calls it unconditionally, the same way every
 * `LayerController` mutator unconditionally pushes to `UndoStack` and lets
 * that class decide what to do with it, rather than each call site
 * guarding on recording state itself.
 */
class MacroRecorder : public QObject {
    Q_OBJECT

public:
    /// @brief Builds a recorder with nothing recorded yet.
    /// @param parent The owning object, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit MacroRecorder(QObject* parent = nullptr);

    /// @brief Starts a new recording, discarding whatever the previous
    ///        one captured. A no-op (does not restart/clear, does not
    ///        update startUndoIndex()) if already recording.
    /// @param startUndoIndex `UndoStack::currentIndex()` at the moment
    ///        recording begins - `playMacro()`'s own checkpoint to jump
    ///        back to before replaying anything.
    void startRecording(std::size_t startUndoIndex);

    /// @brief Stops recording. The already-captured events() remain
    ///        available for inspection/replay until the next
    ///        startRecording() call. A no-op if not currently recording.
    void stopRecording();

    /// @return Whether a recording is currently in progress.
    [[nodiscard]] bool isRecording() const noexcept { return recording_; }

    /// @return The `UndoStack` index `startRecording()` was last called
    ///         with - meaningless (`0`) if nothing has ever been
    ///         recorded.
    [[nodiscard]] std::size_t startUndoIndex() const noexcept { return startUndoIndex_; }

    /**
     * @brief Appends one event, if (and only if) currently recording.
     *
     * @param timestampSeconds The project's own current playback position,
     *        in seconds - not wall-clock time since recording started, so
     *        a macro's own timing is expressed in the same terms as the
     *        project's timeline it's scripting, the natural basis for
     *        "at this point in the piece, do this" rather than "this many
     *        real-world seconds after I pressed record."
     * @param type Which kind of action this is.
     * @param description A human-readable label for this event.
     * @param undoStackIndexAfter See `MacroEvent::undoStackIndexAfter`'s
     *        own docs.
     * @param layerId The affected layer, if this event type has one.
     * @param mindWaveId The affected MindWave, if this event type has one.
     */
    void recordEvent(double timestampSeconds, MacroEventType type, const QString& description,
                      std::size_t undoStackIndexAfter, std::optional<sound_mind::core::LayerId> layerId = std::nullopt,
                      std::optional<sound_mind::core::MindWaveId> mindWaveId = std::nullopt);

    /// @return Every event captured by the current (or most recently
    ///         stopped) recording, in the order they were recorded.
    [[nodiscard]] const std::vector<MacroEvent>& events() const noexcept { return events_; }

    /// @brief Discards whatever the current (or most recently stopped)
    ///        recording captured, and stops recording if one is in
    ///        progress - `MainWindow::setProject()`'s own "a project
    ///        switch invalidates a recorded macro outright" guard
    ///        (`v0.Y.49.1` Installment B): every `undoStackIndexAfter`
    ///        only means anything against the `UndoStack` it was recorded
    ///        from, which `setProject()` itself clears on every switch.
    void discardEvents();

signals:
    /// @brief Emitted whenever isRecording() changes.
    void recordingStateChanged(bool recording);

private:
    bool recording_ = false;
    std::size_t startUndoIndex_ = 0;
    std::vector<MacroEvent> events_;
};

}  // namespace sound_mind::studio
