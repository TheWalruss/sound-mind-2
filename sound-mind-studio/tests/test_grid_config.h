#pragma once

#include <QObject>

class GridConfigTest : public QObject {
    Q_OBJECT

private slots:
    void frequencyGridConfigIsNotActiveWithNothingEnabled();
    void frequencyGridConfigIsActiveWithNoteGridEnabled();
    void frequencyGridConfigIsNotActiveWithHarmonicSeriesEnabledButAZeroFundamental();
    void frequencyGridConfigIsNotActiveWithCustomFrequenciesEnabledButAnEmptyList();
    void timingGridConfigIsNotActiveWhenOff();
    void timingGridConfigIsActiveWithAPositiveInterval();
    void timingGridConfigIsNotActiveWithATempoModeButAZeroBeatFraction();

    void frequencyGridLinesHzReturnsNothingWhenNotActive();
    void frequencyGridLinesHzNoteGridProducesEverySemitoneInRange();
    void frequencyGridLinesHzHarmonicSeriesProducesIntegerMultiplesOfTheFundamental();
    void frequencyGridLinesHzCustomFrequenciesAreFilteredToTheProjectsOwnRange();
    void frequencyGridLinesHzCombinesAndDeduplicatesMultipleSources();

    void timingGridLinesSecondsReturnsNothingWhenNotActive();
    void timingGridLinesSecondsIntervalModeCoversTheWholeDuration();
    void timingGridLinesSecondsTempoModeDerivesStepFromTheProjectsOwnBpm();

    void nearestFrequencyGridLineHzReturnsNulloptWhenNotActive();
    void nearestFrequencyGridLineHzSnapsToTheNearestSemitone();
    void nearestFrequencyGridLineHzSnapsToTheNearestHarmonic();
    void nearestFrequencyGridLineHzSnapsToTheNearestCustomFrequency();
    void nearestFrequencyGridLineHzPicksTheGloballyNearestAcrossCombinedSources();

    void nearestTimingGridLineSecondsReturnsNulloptWhenNotActive();
    void nearestTimingGridLineSecondsSnapsToTheNearestIntervalMultiple();
    void nearestTimingGridLineSecondsSnapsToTheNearestTempoMultiple();

    void snapToGridLeavesAPointUnchangedWhenNeitherGridIsActive();
    void snapToGridSnapsOnlyFrequencyWhenOnlyTheFrequencyGridIsActive();
    void snapToGridSnapsOnlyTimeWhenOnlyTheTimingGridIsActive();
    void snapToGridSnapsBothAxesWhenBothGridsAreActive();
};
