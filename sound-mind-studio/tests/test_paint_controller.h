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
    void endStrokeSynthesizesASilentBaseForAContentLessLayer();
    void cancelStrokeDiscardsTheStrokeWithoutAppendingAnOperation();
    void undoRevertsTheLayersContentAndRedoReappliesIt();
    void setProjectClearsAnyInProgressStroke();

    // Mind Grains (v0.Y.33.1 Installment B).
    void beginStrokeRefusesSilentlyWhenTheMindGrainToolIsNotAllowedOnTheTargetLayer();
    void beginStrokeStartsNormallyWhenTheMindGrainToolIsAllowedOnTheTargetLayer();
    void endStrokeWithAMindGrainToolPaintsFromTheSourceLayersCurrentContent();
    void rebuildLayerContentRereadsTheSourceLayersCurrentContentEachTime();
    void paintingOnASourceLayerImmediatelyCascadesToDependentMindGrainLayers();
    void cascadeVisitsEachDependentLayerOnlyOncePerRebuild();

    // MindWaves v2 Installment A: Instrument vibrato/tremolo (v0.Y.39.1).
    void endStrokeWithAnInstrumentToolBoundToATremoloMindWaveResolvesItAgainstTheProject();
    void rebuildLayerContentRereadsTheTremoloMindWaveLibraryEachTime();
};
