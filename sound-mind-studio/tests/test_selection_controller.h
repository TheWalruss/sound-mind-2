#pragma once

#include <QObject>

class SelectionControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerHasNoSelection();
    void beginSelectionDragShowsADegenerateRectAtTheAnchor();
    void continueSelectionDragNormalizesTheRectRegardlessOfDragDirection();
    void endSelectionDragWithRealMovementCommitsTheSelection();
    void endSelectionDragWithoutMovementClearsAnyExistingSelection();
    void cancelSelectionDragRevertsToThePriorCommittedSelection();
    void clearSelectionIsANoOpWhenNothingIsSelected();
    void fillAppendsAFillOperationOverTheCommittedSelection();
    void fillIsANoOpWithNoCommittedSelection();
    void setProjectClearsSelectionAndAnyInProgressDrag();
};
