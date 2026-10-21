#pragma once

#include <QObject>

class MacroRecorderTest : public QObject {
    Q_OBJECT

private slots:
    void startAddsARecordingMacroStartingAfterTheLatestEntry();
    void startCommitsPendingChangesFirst();
    void theClockRunsOnlyWhilePlaying();
    void changesWhilePausedLandAtTheFrozenTime();
    void aJumpOfThePlayheadIsASeek();
    void undoAndRedoAreRecordedAsEvents();
    void stopKeepsTheMacroWithItsDuration();
    void startingWhilePlayingBeginsWithPlay();
    void startWhileRecordingIsRefusedAndDiscardForgets();
    void managerDialogRenamesAndDeletesButNotWhileRecording();
};
