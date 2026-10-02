#pragma once

#include <QObject>

class MindWaveEditorTest : public QObject {
    Q_OBJECT

private slots:
    void freshEditorHasTheDefaultMindWave();
    void freshEditorShowsOnlyThePeriodicGroup();
    void changingGeneratorTypeShowsOnlyThatTypesOwnGroupAndEmits();
    void spatialHidesTheAxisCombo();
    void changingPeriodPhaseSeedNoiseFieldsUpdateAndEmit();
    void changingPeriodicWaveformAndDutyCycleUpdateAndEmit();
    void changingEnvelopeFieldsUpdateAndEmit();
    void changingSteppedNoiseFieldsUpdateAndEmit();
    void changingSpatialFieldsUpdateAndEmit();
    void changingFractalFieldsUpdateAndEmit();
    void setMindWaveSyncsEveryControlWithoutEmitting();
    void setMindWavePreservesSuperpositionStackAndBlendMode();

    // MindWaves v2 Installment B: Drawn generator (v0.Y.39.1).
    void switchingToDrawnShowsOnlyItsOwnGroup();
    void aFreshlyLoadedDrawnMindWaveWithNoPathShowsTheUncapturedStatus();
    void aLoadedDrawnMindWaveWithACapturedPathShowsTheNodeCountAndIsPreserved();

    // MindWaves v2 Installment C: StepGrid generator (v0.Y.39.1).
    void switchingToStepGridShowsOnlyItsOwnGroupWithTheDefaultFourSteps();
    void changingStepGridCountResizesTheValueRows();
    void changingAStepGridValueEmitsWithTheUpdatedValues();
    void loadingAStepGridMindWaveSyncsTheCountAndEachStepsOwnValue();

    // MindWaves v2 Installment D: Continuous generator (v0.Y.39.1).
    void switchingToContinuousShowsOnlyItsOwnGroupWithTheDefaultKnobPositions();
    void changingShapeSkewOrCharacterUpdatesAndEmits();
    void loadingAContinuousMindWaveSyncsAllThreeKnobs();

    // MindWave UI/UX uplift - real rotary dials + a live shape preview (v0.Y.58.1).
    void draggingAContinuousDialUpdatesItsOwnSpinBoxAndEmits();
    void changingContinuousFieldsUpdatesTheLivePreview();

    // Resonant Instruments Installment E: Resonance generator (v0.Y.59.1).
    void switchingToResonantShowsOnlyItsOwnGroup();
    void settingAvailableResonantProfilesWithNoneShowsThePlaceholder();
    void selectingAResonantProfileUpdatesTheWaveAndEmits();
    void loadingAResonantMindWaveSyncsThePickersSelection();
    void loadingAResonantMindWaveWithNoSourceIdSelectsThePlaceholder();
};
