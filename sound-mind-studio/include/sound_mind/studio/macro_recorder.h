#pragma once

#include <functional>
#include <optional>

#include <QObject>
#include <QString>

#include "sound_mind/core/history.h"
#include "sound_mind/core/macro.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class ProjectHistory;

/**
 * @brief Records a Macro (`v0.1.44.1`, `docs/sound-mind-design.md`'s
 *        *Macros*): every History entry made, undone or redone while
 *        armed, and what the playhead does, on the Macro's own clock.
 *
 * The clock runs only while the transport (Playback or Loop) does: pausing
 * stops it, and changes made meanwhile are recorded, in order, at the
 * moment it stopped. The playhead is reported through notePlayhead() -
 * MainWindow calls it on every position update and transport change - and
 * the recorder turns that into Play/Pause/Seek events, recognizing a seek,
 * a Repeat wrap or a new Loop pass as a jump away from where steady
 * playback would have put the playhead.
 *
 * The Macro lives in the project's library from the moment recording
 * starts (so the History can keep what it starts from, see
 * `History::commit()`), marked as recording until stop().
 *
 * UI-thread only.
 */
class MacroRecorder : public QObject {
    Q_OBJECT

public:
    /// @brief How far (seconds) the playhead may stray from where steady
    ///        playback would put it before it counts as a seek.
    static constexpr double kSeekToleranceSeconds = 0.25;

    /**
     * @param history Non-owning; the recorder follows its committed, undone
     *        and redone entries. Must outlive the recorder.
     * @param parent Standard QObject parent.
     */
    explicit MacroRecorder(ProjectHistory* history, QObject* parent = nullptr);

    /**
     * @brief Starts recording a new Macro named `name` into `project`.
     *
     * Commits any uncommitted change first, so the Macro starts from a
     * recorded state. If the transport is already running (`playing`), the
     * clock starts at once with a Play event at `position`.
     *
     * @param project The project; must outlive the recording.
     * @param name The Macro's name.
     * @param position The playhead now (seconds).
     * @param playing Whether the transport is running now.
     * @return The new Macro's id; `std::nullopt` if already recording.
     */
    std::optional<sound_mind::core::MacroId> start(sound_mind::core::Project& project, const QString& name,
                                                   double position, bool playing);

    /// @brief Stops recording; the Macro stays in the project with its
    ///        duration set. A no-op if not recording.
    void stop();

    /// @brief Forgets the recording without touching any project - the
    ///        project is going away.
    void discard();

    /// @brief Whether recording.
    /// @return `true` between start() and stop().
    [[nodiscard]] bool isRecording() const noexcept { return macroId_.has_value(); }

    /// @brief The Macro being recorded.
    /// @return Its id, or `std::nullopt`.
    [[nodiscard]] std::optional<sound_mind::core::MacroId> recordingMacro() const noexcept { return macroId_; }

    /**
     * @brief Reports the playhead - call on every position update and
     *        whenever the transport starts or stops.
     * @param positionSeconds Where it is.
     * @param playing Whether the transport is running.
     */
    void notePlayhead(double positionSeconds, bool playing);

    /// @brief The Macro clock's current reading.
    /// @return Seconds.
    [[nodiscard]] double clockSeconds() const;

    /// @brief Replaces the real-time clock (tests).
    /// @param nowSeconds Returns monotonic seconds.
    void setTimeSource(std::function<double()> nowSeconds) { now_ = std::move(nowSeconds); }

signals:
    /// @brief Recording started or stopped.
    void recordingStateChanged(bool recording);

    /// @brief An event was added to the Macro being recorded.
    void eventRecorded();

private:
    void add(sound_mind::core::MacroEvent event);
    void addEntryEvent(sound_mind::core::MacroEventKind kind, sound_mind::core::HistoryEntryId id);
    [[nodiscard]] sound_mind::core::Macro* macro();

    ProjectHistory* history_;
    sound_mind::core::Project* project_ = nullptr;
    std::optional<sound_mind::core::MacroId> macroId_;
    std::function<double()> now_;
    double accumulated_ = 0.0;
    bool running_ = false;
    double runningSince_ = 0.0;
    double lastPosition_ = 0.0;
    double lastPositionAt_ = 0.0;
};

}  // namespace sound_mind::studio
