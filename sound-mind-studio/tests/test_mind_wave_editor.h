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
};
