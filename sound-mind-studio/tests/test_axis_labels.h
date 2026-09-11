#pragma once

#include <QObject>

class AxisLabelsTest : public QObject {
    Q_OBJECT

private slots:
    void verticalAxisTicksOffModeReturnsNothing();
    void verticalAxisTicksHertzModeStaysWithinTheProjectsOwnFrequencyRange();
    void verticalAxisTicksHertzModeThinsTicksThatWouldCrowdTogether();
    void verticalAxisTicksNotesModeLabelsWithNoteNames();
    void verticalAxisTicksNotesModeRespectsARetunedReference();
    void verticalAxisTicksBinIndexModeCoversTheFullBinRange();
    void horizontalAxisTicksOffModeReturnsNothing();
    void horizontalAxisTicksSecondsModeStartsAtZeroAndStaysWithinTheCanvassOwnDuration();
    void horizontalAxisTicksMillisecondsModeLabelsInWholeMilliseconds();
    void horizontalAxisTicksFrameIndexModeLabelsWithRoundedFrameNumbers();
    void horizontalAxisTicksChoosesACoarserStepForANarrowerAxis();
};
