#include "test_background_job.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>

#include <QPushButton>
#include <QSignalSpy>
#include <QThread>
#include <QtTest/QtTest>

#include "sound_mind/studio/background_job.h"

using sound_mind::core::CancellationToken;
using sound_mind::studio::BackgroundJob;

namespace {

/// Spins until `token` is cancelled or `stop` is set.
void waitFor(const CancellationToken& token, const std::atomic<bool>& stop) {
    while (!token.cancellationRequested() && !stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

}  // namespace

// v0.1.47.2 - the Studio's one shape for long operations.
void BackgroundJobTest::theCompletionRunsOnTheUiThreadAfterTheWork() {
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    std::atomic<Qt::HANDLE> workThread{nullptr};
    Qt::HANDLE completionThread = nullptr;
    int result = 0;
    QSignalSpy finished(&job, &BackgroundJob::finished);

    QVERIFY(job.start([&](CancellationToken&) -> BackgroundJob::Completion {
        workThread = QThread::currentThreadId();
        const int computed = 42;
        return [&, computed](bool) {
            completionThread = QThread::currentThreadId();
            result = computed;
        };
    }));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(result, 42);
    QVERIFY(workThread.load() != QThread::currentThreadId());
    QCOMPARE(completionThread, QThread::currentThreadId());
    QCOMPARE(finished.at(0).at(0).toBool(), false);
}

void BackgroundJobTest::itIsRunningUntilTheCompletionHasRun() {
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    std::atomic<bool> release{false};
    bool applied = false;
    job.start([&](CancellationToken& token) -> BackgroundJob::Completion {
        waitFor(token, release);
        return [&](bool) { applied = true; };
    });
    QVERIFY(job.isRunning());
    release = true;
    QTRY_VERIFY(!job.isRunning());
    QVERIFY(applied);  // not running implies applied.
}

void BackgroundJobTest::aSecondStartWhileRunningIsRefused() {
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    std::atomic<bool> release{false};
    QVERIFY(job.start([&](CancellationToken& token) -> BackgroundJob::Completion {
        waitFor(token, release);
        return {};
    }));
    QVERIFY(!job.start([](CancellationToken&) -> BackgroundJob::Completion { return {}; }));
    release = true;
    QTRY_VERIFY(!job.isRunning());
}

void BackgroundJobTest::theCancelButtonShowsWhileRunningAndCancels() {
    QPushButton button;
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    job.setCancelButton(&button);
    QVERIFY(button.isHidden());
    std::atomic<bool> never{false};
    bool sawCancel = false;
    QSignalSpy finished(&job, &BackgroundJob::finished);
    job.start([&](CancellationToken& token) -> BackgroundJob::Completion {
        waitFor(token, never);
        const bool cancelled = token.cancellationRequested();
        return [&, cancelled](bool) { sawCancel = cancelled; };
    });
    QVERIFY(!button.isHidden());
    button.click();
    QVERIFY(job.cancelRequested());
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(sawCancel);
    QVERIFY(finished.at(0).at(0).toBool());
    QVERIFY(button.isHidden());
}

void BackgroundJobTest::finishNowWaitsAndAppliesAtOnce() {
    BackgroundJob job(nullptr, std::chrono::hours(1));  // the poll alone would never deliver in time.
    bool applied = false;
    job.start([&](CancellationToken&) -> BackgroundJob::Completion {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return [&](bool) { applied = true; };
    });
    job.finishNow();
    QVERIFY(applied);
    QVERIFY(!job.isRunning());
    job.finishNow();  // idle: a no-op.
}

void BackgroundJobTest::aCompletionMayStartTheJobAgain() {
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    int runs = 0;
    std::function<void()> again;
    again = [&]() {
        job.start([&](CancellationToken&) -> BackgroundJob::Completion {
            return [&](bool) {
                if (++runs < 3) {
                    again();
                }
            };
        });
    };
    again();
    QTRY_COMPARE(runs, 3);
    QTRY_VERIFY(!job.isRunning());
}

void BackgroundJobTest::anExceptionReachesTheErrorHandler() {
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    QString error;
    job.start([](CancellationToken&) -> BackgroundJob::Completion { throw std::runtime_error("disk full"); },
              [&](const QString& message) { error = message; });
    QTRY_VERIFY(!job.isRunning());
    QCOMPARE(error, QStringLiteral("disk full"));
}

void BackgroundJobTest::destroyingARunningJobCancelsAndWaits() {
    auto job = std::make_unique<BackgroundJob>(nullptr, std::chrono::milliseconds(1));
    std::atomic<bool> never{false};
    std::atomic<bool> stopped{false};
    bool applied = false;
    job->start([&](CancellationToken& token) -> BackgroundJob::Completion {
        waitFor(token, never);
        stopped = true;
        return [&](bool) { applied = true; };
    });
    job.reset();
    QVERIFY(stopped.load());  // the work saw the cancel and ended before the destructor returned.
    QVERIFY(!applied);        // its completion was dropped.
}

void BackgroundJobTest::aCancelAfterTheWorkFinishedStillReachesTheCompletion() {
    BackgroundJob job(nullptr, std::chrono::hours(1));  // delivered by finishNow() below.
    std::atomic<bool> done{false};
    bool toldCancelled = false;
    job.start([&](CancellationToken&) -> BackgroundJob::Completion {
        done = true;
        return [&](bool cancelled) { toldCancelled = cancelled; };
    });
    QTRY_VERIFY(done.load());
    QTest::qWait(5);         // the work has returned; its result waits for delivery.
    job.requestCancel();     // e.g. the project was switched meanwhile.
    job.finishNow();
    QVERIFY(toldCancelled);
}

void BackgroundJobTest::stillRunningIsEmittedWhileTheWorkRuns() {
    BackgroundJob job(nullptr, std::chrono::milliseconds(1));
    QSignalSpy ticks(&job, &BackgroundJob::stillRunning);
    std::atomic<bool> release{false};
    job.start([&](CancellationToken& token) -> BackgroundJob::Completion {
        waitFor(token, release);
        return {};
    });
    QTRY_VERIFY(ticks.count() >= 2);
    release = true;
    QTRY_VERIFY(!job.isRunning());
}
