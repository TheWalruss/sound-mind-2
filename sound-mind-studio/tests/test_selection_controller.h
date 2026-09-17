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
};
