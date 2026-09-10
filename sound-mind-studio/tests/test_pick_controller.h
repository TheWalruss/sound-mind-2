#pragma once

#include <QObject>

class PickControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerHasNoSelection();
    void pickSelectsAnOperationUnderThePoint();
    void pickReturnsFalseAndClearsSelectionWhenNothingIsUnderThePoint();
    void pickPrefersTheMostRecentOverlappingOperation();
    void pickOnAnAlreadySelectedOperationCyclesToTheOccludedOneUnderneath();
    void pickOnADifferentUnselectedOperationDoesNotCycle();
    void pickPadsHitTestingByTheOperationsOwnBrushSize();
    void continueMoveUpdatesTheLivePreviewPath();
    void endMoveCommitsATranslatedSupersedingOperationAndKeepsItSelected();
    void endMoveWithoutAnyRealMovementDoesNotCommitAnything();
    void applyToolConfigurationCommitsANewOperationWithTheSameGeometry();
    void deleteSelectionCommitsATombstoneAndClearsSelection();
    void clearSelectionEmitsSelectionChangedOnlyWhenSomethingWasSelected();
    void setProjectClearsSelection();
};
