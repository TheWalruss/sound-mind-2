#pragma once

#include <QObject>

class BackgroundJobTest : public QObject {
    Q_OBJECT

private slots:
    void theCompletionRunsOnTheUiThreadAfterTheWork();
    void itIsRunningUntilTheCompletionHasRun();
    void aSecondStartWhileRunningIsRefused();
    void theCancelButtonShowsWhileRunningAndCancels();
    void finishNowWaitsAndAppliesAtOnce();
    void aCompletionMayStartTheJobAgain();
    void anExceptionReachesTheErrorHandler();
    void destroyingARunningJobCancelsAndWaits();
    void aCancelAfterTheWorkFinishedStillReachesTheCompletion();
    void stillRunningIsEmittedWhileTheWorkRuns();
};
