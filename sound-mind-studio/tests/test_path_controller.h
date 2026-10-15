#pragma once

#include <QObject>

class PathControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerHasNoPathInProgress();
    void beginPathIsANoOpWithNoProject();
    void continuePathFitsALivePreview();
    void endPathCommitsASilentGuidePathOnTheTargetLayer();
    void endPathAfterAPressWithNoDragCommitsNothing();
    void endPathIsUndoableAsOneStep();
    void cancelPathDiscardsWithoutCommittingAnything();
    void setProjectCancelsAnyPathInProgress();
};
