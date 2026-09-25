#pragma once

#include <atomic>
#include <functional>
#include <thread>

namespace sound_mind::core {

/**
 * @brief A cooperative cancellation flag, shared between a `BackgroundTask`'s
 *        caller and the work function running on its own worker thread -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone (`v0.0.45.13`).
 *
 * There is no way to safely force-stop a running thread mid-instruction, so
 * cancellation here is cooperative: `requestCancel()` only ever sets a flag;
 * the work function itself is responsible for checking
 * `cancellationRequested()` periodically (between chunks of work small
 * enough that the resulting latency - however long one chunk takes - is an
 * acceptable "how long until cancel actually takes effect" bound) and
 * returning early once it reads `true`. A work function that never checks
 * simply can't be cancelled, no differently from any other cooperative
 * cancellation scheme (`std::stop_token`'s own model, for one).
 *
 * @note Thread-safety: every member is safe to call from any thread,
 *       concurrently with any other - this is the whole point of the class.
 */
class CancellationToken {
public:
    /// @brief Requests that the associated work function stop as soon as it
    ///        next checks cancellationRequested(). Idempotent - a second
    ///        call once already requested is a no-op.
    void requestCancel() noexcept { cancelled_.store(true, std::memory_order_relaxed); }

    /// @brief Whether requestCancel() has been called.
    /// @return `true` once cancellation has been requested; `false` until
    ///         then, for the lifetime of a token that's never had
    ///         requestCancel() called.
    [[nodiscard]] bool cancellationRequested() const noexcept { return cancelled_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_{false};
};

/**
 * @brief Runs a long-running operation on its own background thread, with
 *        cooperative cancellation via `CancellationToken` -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone (`v0.0.45.13`).
 *
 * A thin, generic, `std::thread`-based wrapper - deliberately not built on
 * `juce::Thread` (the primitive `LoopEngine`/`RecordEngine`/`PlaybackEngine`
 * each use for their own real-time-adjacent audio device callbacks): those
 * three are audio-device-bound workers with a fixed `run()` body each;
 * `BackgroundTask` instead wraps an arbitrary caller-supplied work function
 * (Import encoding, Pool encode/decode, Export, GPU compute dispatch - see
 * the milestone's own later installments), which a lambda-taking
 * `std::function` fits more directly than subclassing would.
 *
 * The caller is expected to poll isRunning()/wasCancelled() from its own UI
 * thread (the same "background worker + UI-thread polling `QTimer`"
 * pattern `MainWindow`'s `loopUpdateTimer_`/`updateLoopLayer()` already
 * establish for `LoopEngine`), rather than this class offering a cross-
 * thread completion signal of its own.
 *
 * @note Thread-safety: start()/requestCancel()/waitForFinished() are meant
 *       to be called from one thread at a time (typically the UI thread) -
 *       concurrent starts on the same instance, or a start() while already
 *       running, are not supported. isRunning()/wasCancelled() are safe to
 *       poll from any thread, concurrently with the above.
 */
class BackgroundTask {
public:
    /// @brief The work a `BackgroundTask` runs - receives the token it
    ///        should periodically check via `cancellationRequested()`.
    using WorkFunction = std::function<void(CancellationToken&)>;

    /// @brief Constructs a `BackgroundTask` for `work` - does not start it;
    ///        see start().
    /// @param work The operation to run on a background thread once
    ///        start() is called.
    explicit BackgroundTask(WorkFunction work);

    /// @brief Joins the worker thread if it's still running, blocking until
    ///        it actually exits - a `BackgroundTask` going out of scope
    ///        never leaves a detached thread still running against
    ///        state the destructing caller may no longer own. Does *not*
    ///        request cancellation first - a caller that wants a prompt
    ///        exit should requestCancel() (and, ideally, waitForFinished())
    ///        before letting a `BackgroundTask` be destroyed.
    ~BackgroundTask();

    BackgroundTask(const BackgroundTask&) = delete;
    BackgroundTask& operator=(const BackgroundTask&) = delete;

    /// @brief Starts `work` on a new background thread. One-shot: not safe
    ///        to call again, even after the first run finishes - a fresh
    ///        `BackgroundTask` should be constructed for each new
    ///        operation, both because a second `start()` on the same
    ///        instance would race the still-joinable previous thread, and
    ///        because a used token's own `cancellationRequested()` stays
    ///        `true` forever once requested, which would immediately
    ///        cancel a reused instance's next run too.
    void start();

    /// @brief Requests that the running work function stop - see
    ///        `CancellationToken::requestCancel()`'s own docs on why this
    ///        doesn't itself guarantee an immediate stop. A no-op if not
    ///        currently running.
    void requestCancel();

    /// @brief Whether the worker thread is currently running `work`.
    ///
    /// An acquire load, paired with a release store the instant `work`
    /// returns: a caller that observes this go from `true` to `false` (by
    /// polling, typically) is also guaranteed to see whatever `work` itself
    /// wrote before returning - e.g. a result stored in a plain member the
    /// caller owns and passed into `work` by reference/capture - not just
    /// the flag itself. This is what makes a poll-for-completion pattern
    /// (see the class docs) safe without needing its own separate
    /// synchronization for the actual result.
    ///
    /// @return `true` from start() until the work function returns
    ///         (whether it completed normally or bailed out early on
    ///         cancellation) - see waitForFinished()'s own docs for
    ///         blocking until it flips back to `false`.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Whether requestCancel() has been called for the current (or
    ///        most recent) run - see `CancellationToken::cancellationRequested()`.
    ///        This reflects the request itself, not whether the work
    ///        function actually honored it and stopped early; a work
    ///        function that ignores the token still reports `true` here
    ///        once requestCancel() was called, even though it ran to
    ///        completion regardless.
    /// @return `true` if requestCancel() has been called.
    [[nodiscard]] bool wasCancelled() const noexcept;

    /// @brief Blocks the calling thread until the worker thread actually
    ///        exits. A no-op if not currently running (including if never
    ///        start()ed). Safe to call after requestCancel() to know when
    ///        it's actually safe to inspect/roll back whatever the work
    ///        function was doing.
    void waitForFinished();

private:
    WorkFunction work_;
    CancellationToken token_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

}  // namespace sound_mind::core
