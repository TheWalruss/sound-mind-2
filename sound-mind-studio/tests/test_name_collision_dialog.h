#pragma once

#include <QObject>

class NameCollisionDialogTest : public QObject {
    Q_OBJECT

private slots:
    void promptSaveNameWithNoCollisionReturnsImmediatelyWithNoDialog();
    void promptSaveNameReplaceReturnsSavedWithReplacingExistingTrue();
    void promptSaveNameRenameLoopsUntilACollisionFreeNameIsEntered();
    void promptSaveNameCancelOnTheCollisionDialogReturnsCancelled();
    void promptSaveNameCancellingTheInitialNamePromptReturnsCancelled();
    void resolveNameCollisionWithACollisionFreeCandidateReturnsSavedImmediately();

    void resolveImportNameWithNoCollisionReturnsAddNewImmediatelyWithNoDialog();
    void resolveImportNameOverwriteReturnsOverwriteExisting();
    void resolveImportNameKeepExistingReturnsSkip();
    void resolveImportNameKeepBothReturnsAFreshlySuffixedName();
    void resolveImportNameKeepBothSkipsAnAlreadyTakenSuffix();
};
