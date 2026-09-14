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
};
