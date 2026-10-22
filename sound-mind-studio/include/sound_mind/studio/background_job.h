#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include "sound_mind/core/background_task.h"

class QAbstractButton;

namespace sound_mind::studio {

/**
 * @brief One long operation run off the UI thread, with its result applied
 *        back on the UI thread (`v0.1.47.2`) - the Studio's single shape for
 *        import, export, playback preparation, analysis and the rest.
 *
 * A job is split in two:
 * - the **work** runs on a background thread (a `core::BackgroundTask`),
 *   touches nothing shared, checks its `CancellationToken`, and returns a
 *   **completion**: a closure holding its result;
 * - the completion runs later on the UI thread, where it may change the
 *   project and the widgets.
 *
 * The job polls for the work to finish (a `QTimer`, so no cross-thread
 * signal is needed), then runs the completion and emits finished(). The
 * completion is told whether cancellation was requested at any point before
 * it ran - even after the work itself had finished - so a result made stale
 * by, say, a project switch can be dropped. The work may return an empty
 * completion (nothing to apply).
 * If it throws, the `onError` handler given to start() gets the message.
 *
 * isRunning() stays `true` until the completion has run, so a caller that
 * sees it `false` also sees the result applied. A completion may start the
 * same job again (a "latest request wins" re-run).
 *
 * An optional cancel button (setCancelButton()) is shown while the job runs
 * and calls requestCancel() when clicked.
 *
 * @note UI thread only, except for the work function itself. Destroying a
 *       running job requests cancellation and waits for the work to stop;
 *       its completion is then dropped.
 */
class BackgroundJob : public QObject {
    Q_OBJECT

public:
    /// @brief The UI-thread half: applies the work's result. Told whether
    ///        requestCancel() was called during the run.
    using Completion = std::function<void(bool cancelled)>;
    /// @brief The background half: computes a result and returns the
    ///        completion that applies it (or an empty one).
    using Work = std::function<Completion(sound_mind::core::CancellationToken&)>;
    /// @brief Told the message of an exception the work threw.
    using ErrorHandler = std::function<void(const QString&)>;

    /// @brief Makes an idle job.
    /// @param parent The owning QObject.
    /// @param pollInterval How often to check whether the work finished.
    explicit BackgroundJob(QObject* parent = nullptr,
                           std::chrono::milliseconds pollInterval = std::chrono::milliseconds(33));

    /// @brief Requests cancellation and waits for running work; its
    ///        completion is dropped.
    ~BackgroundJob() override;

    /**
     * @brief Starts `work` on a background thread.
     * @param work The background half; see the class docs.
     * @param onError Told an exception's message if `work` throws; may be
     *        empty (the error is then ignored).
     * @return `false` (nothing started) if the job is already running.
     */
    bool start(Work work, ErrorHandler onError = {});

    /// @brief Whether a job has started and its completion hasn't run yet.
    /// @return `true` from start() until the completion has run.
    [[nodiscard]] bool isRunning() const noexcept { return task_ != nullptr; }

    /// @brief Asks the running work to stop (via its token) and marks the
    ///        run cancelled, even if the work has already finished. A no-op
    ///        when idle. The completion still runs, told `cancelled`.
    void requestCancel();

    /// @brief Whether requestCancel() was called during the current run.
    /// @return `true` once cancellation was requested; `false` when idle.
    [[nodiscard]] bool cancelRequested() const noexcept;

    /// @brief Waits for the running work to finish, then runs its
    ///        completion now - for "finish this before switching projects".
    ///        A no-op when idle.
    void finishNow();

    /// @brief Shows `button` while the job runs and cancels the job when it
    ///        is clicked. The button stays owned by its parent.
    /// @param button The status-bar cancel button, or `nullptr` for none.
    void setCancelButton(QAbstractButton* button);

signals:
    /// @brief Emitted on each poll while the work is still running - for
    ///        progress displays.
    void stillRunning();

    /// @brief Emitted after a run's completion has run (or after a run with
    ///        an empty completion or an error ended).
    /// @param cancelled Whether cancellation had been requested.
    void finished(bool cancelled);

private:
    /// @brief The poll timer's slot: delivers a finished run.
    void poll();
    /// @brief Runs the finished work's completion and resets the job.
    void deliver();

    std::unique_ptr<sound_mind::core::BackgroundTask> task_;
    /// @brief Written by the work's thread before it ends; read after
    ///        `BackgroundTask::isRunning()` turned false (acquire/release).
    Completion completion_;
    QString errorMessage_;
    bool failed_ = false;
    bool cancelRequested_ = false;
    ErrorHandler onError_;
    QTimer timer_;
    QPointer<QAbstractButton> cancelButton_;
};

}  // namespace sound_mind::studio
