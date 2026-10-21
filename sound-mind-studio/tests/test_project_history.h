#pragma once

#include <QObject>

class ProjectHistoryTest : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithoutAProject();
    void commitRecordsAnEntryAndSignals();
    void commitWithNoChangeRecordsNothing();
    void undoRedoRebuildContentBeforeSignalling();
    void jumpToReachesThePositionAndSignalsOnce();
    void undoWithoutARecoverableBaseFailsCleanly();
    void undoUsesTheBaseResolver();
};
