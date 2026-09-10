#pragma once

#include <QObject>

class PaintControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerHasNoStrokeInProgress();
    void freshControllerCannotUndoOrRedo();
    void beginStrokeDoesNothingWithNoProjectSet();
    void beginStrokeStartsAStroke();
    void beginStrokeDoesNothingWhileAStrokeIsAlreadyInProgress();
    void continueStrokeEmitsPathChangedOnceThereAreTwoPoints();
    void continueStrokeDoesNothingWithNoStrokeInProgress();
    void endStrokeAppendsAPaintOperationAndEmitsContentChanged();
    void endStrokeWithOnlyOnePointStillPaintsATap();
    void endStrokeActuallyChangesTheLayersStoredContent();
    void cancelStrokeDiscardsTheStrokeWithoutAppendingAnOperation();
    void undoRevertsTheLayersContentAndRedoReappliesIt();
    void setProjectClearsAnyInProgressStroke();
};
