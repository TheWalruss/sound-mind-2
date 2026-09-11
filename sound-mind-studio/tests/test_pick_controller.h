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
    void pickPadsAFillOperationsHitTestingByAMinimumForgivenessMargin();
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
    void beginPathEditIsANoOpWithNoSelection();
    void beginPathEditIsANoOpForAFillSelection();
    void beginPathEditSucceedsForAPaintSelectionAndCopiesItsPath();
    void selectPathNodeNearSelectsTheClosestNodeWithinTolerance();
    void selectPathNodeNearDeselectsWhenNothingIsClose();
    void draggingASelectedNodesAnchorMovesItAndItsHandles();
    void draggingASelectedSmoothNodesHandleMirrorsTheOppositeHandle();
    void deleteSelectedPathNodeRemovesItButRefusesToEmptyThePath();
    void toggleSelectedPathNodeTypeConvertsCornerToSmoothAndBack();
    void commitPathEditSupersedesTheOriginalWithEditedGeometryKeepingItsGradient();
    void cancelPathEditDiscardsChangesAndLeavesTheOriginalSelected();
    void deleteSelectionDeletesTheSelectedNodeWhileEditingInsteadOfTheWholeObject();
    void clearSelectionExitsAnActivePathEditSessionWithoutCommitting();
};
