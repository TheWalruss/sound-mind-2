#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include "sound_mind/core/background_task.h"

using sound_mind::core::BackgroundTask;
using sound_mind::core::CancellationToken;

TEST_CASE("CancellationToken starts uncancelled and reflects requestCancel", "[core][background_task]") {
    CancellationToken token;
    CHECK_FALSE(token.cancellationRequested());

    token.requestCancel();

    CHECK(token.cancellationRequested());
}

TEST_CASE("CancellationToken::requestCancel is idempotent", "[core][background_task]") {
    CancellationToken token;
    token.requestCancel();
    token.requestCancel();
    CHECK(token.cancellationRequested());
}

TEST_CASE("BackgroundTask runs its work function on a background thread, not the caller's own",
          "[core][background_task]") {
    const std::thread::id callerThreadId = std::this_thread::get_id();
    std::thread::id workThreadId;
    BackgroundTask task([&](CancellationToken&) { workThreadId = std::this_thread::get_id(); });

    task.start();
    task.waitForFinished();  // joins the worker thread - a real happens-before edge, so the plain
                              // (non-atomic) workThreadId write above is safe to read here.

    CHECK(workThreadId != callerThreadId);
}

TEST_CASE("BackgroundTask::isRunning reflects the work function's own lifetime", "[core][background_task]") {
    std::atomic<bool> releaseWork{false};
    std::atomic<bool> workStarted{false};
    BackgroundTask task([&](CancellationToken&) {
        workStarted.store(true, std::memory_order_relaxed);
        while (!releaseWork.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    CHECK_FALSE(task.isRunning());

    task.start();
    while (!workStarted.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(task.isRunning());

    releaseWork.store(true, std::memory_order_relaxed);
    task.waitForFinished();
    CHECK_FALSE(task.isRunning());
}

TEST_CASE("BackgroundTask::requestCancel is observable from inside the work function via its own token",
          "[core][background_task]") {
    std::atomic<bool> observedCancellation{false};
    BackgroundTask task([&](CancellationToken& token) {
        while (!token.cancellationRequested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        observedCancellation.store(true, std::memory_order_relaxed);
    });

    task.start();
    task.requestCancel();
    task.waitForFinished();

    CHECK(observedCancellation.load(std::memory_order_relaxed));
    CHECK(task.wasCancelled());
}

TEST_CASE("BackgroundTask::wasCancelled is false when requestCancel was never called", "[core][background_task]") {
    BackgroundTask task([](CancellationToken&) {});
    task.start();
    task.waitForFinished();
    CHECK_FALSE(task.wasCancelled());
}

TEST_CASE("BackgroundTask's destructor joins a still-running thread rather than leaving it detached",
          "[core][background_task]") {
    std::atomic<bool> workFinished{false};
    {
        BackgroundTask task([&](CancellationToken&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            workFinished.store(true, std::memory_order_relaxed);
        });
        task.start();
        // No waitForFinished() call - ~BackgroundTask() below must itself
        // block until the work function actually returns, per its own
        // docs, rather than returning with the thread still running
        // detached against a now-destroyed BackgroundTask.
    }
    CHECK(workFinished.load(std::memory_order_relaxed));
}
