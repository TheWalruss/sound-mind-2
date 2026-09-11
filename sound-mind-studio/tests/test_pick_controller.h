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
    void pickSelectsAFillOperation();
    void pickSelectsAPasteOperation();
    void selectedConfigurationIsNullForAFillOrPasteSelection();
    void endMoveOnAFillOperationCommitsATranslatedSupersedingFillOperation();
    void endMoveOnAPasteOperationCommitsATranslatedSupersedingPasteOperation();
    void continueMoveOnAFillOperationShowsARectangularOutlinePreview();
    void applyToolConfigurationIsANoOpWhenAFillOperationIsSelected();
    void deleteSelectionOnAFillOperationCommitsASilenceFillTombstone();
    void deleteSelectionOnAPasteOperationCommitsASilenceFillTombstone();
    void endMovePreservesTheMovedOperationsOwnStackPosition();
    void deleteSelectionPreservesStackPositionOfOperationsAboveIt();
    void bringToFrontMovesTheSelectionToTheTopOfItsStack();
    void sendToBackMovesTheSelectionToTheBottomOfItsStack();
    void bringForwardSwapsTheSelectionWithTheOneAboveIt();
    void sendBackwardSwapsTheSelectionWithTheOneBelowIt();
    void reorderMethodsAreNoOpsWithNoSelection();
    void reorderMethodsEmitNoContentChangedWhenAlreadyAtTheRequestedEnd();
};
