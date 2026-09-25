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
    void freshControllerHasNoClipboard();
    void copySelectionCapturesTheSelectionOntoTheClipboard();
    void copySelectionIsANoOpWithNoCommittedSelection();
    void cutSelectionCopiesThenSilencesTheSourceRegion();
    void cutSelectionIsANoOpWithNoCommittedSelection();
    void pasteIntoWritesTheClipboardOntoTheGivenLayer();
    void pasteIntoCanTargetADifferentLayerThanItWasCopiedFrom();
    void pasteIntoIsANoOpWithNoClipboard();
    void pasteIntoUpdatesTheCommittedSelectionToThePastedRegion();
    void setProjectClearsTheClipboardToo();

    void continueSelectionDragSnapsToTheNearestGridLineWhenSnapToGridIsEnabled();
    void continueSelectionDragIgnoresGridConfigurationWhenSnapToGridIsDisabled();

    void captureMindShotAddsANamedEntryToTheProjectsMindShotLibrary();
    void captureMindShotIsANoOpWithNoCommittedSelection();
    void captureMindShotDoesNotTouchTheClipboardOrSourcePixels();
    void captureMindShotEmitsMindShotCapturedWithTheNewId();

    void captureMindGrainAddsANamedEntryToTheProjectsMindGrainLibrary();
    void captureMindGrainIsANoOpWithNoCommittedSelection();
    void captureMindGrainDoesNotTouchTheClipboardOrSourceLayerContent();
    void captureMindGrainEmitsMindGrainCapturedWithTheNewId();

    // Lasso (v0.Y.35.1 Installment A).
    void freshControllerDefaultsToRectangleShape();
    void lassoDragCommitsABoundaryAndItsOwnBoundingBox();
    void lassoDragShowsALiveBoundaryOnceEnoughPointsExist();
    void lassoDragWithFewerThanThreePointsClearsAnyExistingSelection();
    void cancelSelectionDragDuringALassoDragRevertsToThePriorCommittedSelection();
    void setSelectionShapeCancelsAnInProgressDrag();
    void rectangleSelectionHasNoDisplayBoundary();
    void fillWithALassoSelectionCarriesItsBoundary();
    void fillWithARectangleSelectionCarriesNoBoundary();
    void copySelectionThenPasteIntoCarriesTheLassoBoundaryForward();

    // Wand and boolean combination (v0.Y.35.1 Installment B).
    void freshControllerHasDefaultWandSettingsAndReplaceCombineMode();
    void wandClickCommitsAMaskShapedSelectionAtTheClickedBlob();
    void wandIgnoresSubsequentDragMovement();
    void wandClickOnAnUnmatchedIsolatedCellStillSelectsJustThatCell();
    void hasMaskShapedSelectionIsTrueOnlyForWandOrCombinedSelections();
    void addCombinesANewRectangleIntoTheExistingSelection();
    void subtractCarvesTheNewRectangleOutOfTheExistingSelection();
    void intersectKeepsOnlyTheOverlapBetweenOldAndNewSelections();
    void aWhiffedCombineGestureLeavesTheExistingSelectionUntouched();

    // Rectangle's own rotate handle (v0.Y.35.1 Installment C).
    void freshControllerCannotRotate();
    void canRotateSelectionIsTrueOnlyAfterAPlainRectangleCommit();
    void rotatingAppliesARotatedPathBoundaryAndBackToNulloptAtZero();
    void cancelRotateDragRevertsToThePriorRotation();
    void displayRotationHandleIsPresentOnlyWhenRotatable();
};
