#pragma once

#include <QObject>

class MacroPlayerTest : public QObject {
    Q_OBJECT

private slots:
    void startsPlayingOnceBufferedAndShowsTheRenderedCanvas();
    void stopClearsTheCanvasAndSignalsFinished();
    void reachingTheEndFinishes();
    void aRenderingFailureIsReported();
    void theProjectIsNeverTouched();
};
