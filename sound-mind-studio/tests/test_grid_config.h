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

    // Note Grid Key/Scale/Temperament/Octave filtering.
    void frequencyGridLinesHzNoteGridExcludedStepsRemovesThatPitchClassInEveryOctave();
    void frequencyGridLinesHzNoteGridExcludedOctavesRemovesEveryStepInThatOctave();
    void frequencyGridLinesHzNoteGridRespectsAlternateEqualTemperamentStepCount();
    void frequencyGridLinesHzNoteGridRespectsNamedHistoricalTuning();

    void timingGridLinesSecondsReturnsNothingWhenNotActive();
    void timingGridLinesSecondsIntervalModeCoversTheWholeDuration();
    void timingGridLinesSecondsTempoModeDerivesStepFromTheProjectsOwnBpm();

    void nearestFrequencyGridLineHzReturnsNulloptWhenNotActive();
    void nearestFrequencyGridLineHzSnapsToTheNearestSemitone();
    void nearestFrequencyGridLineHzSnapsToTheNearestHarmonic();
    void nearestFrequencyGridLineHzSnapsToTheNearestCustomFrequency();
    void nearestFrequencyGridLineHzPicksTheGloballyNearestAcrossCombinedSources();
    void nearestFrequencyGridLineHzSkipsAnExcludedStepInFavorOfTheNextNearest();

    void nearestTimingGridLineSecondsReturnsNulloptWhenNotActive();
    void nearestTimingGridLineSecondsSnapsToTheNearestIntervalMultiple();
    void nearestTimingGridLineSecondsSnapsToTheNearestTempoMultiple();

    void snapToGridLeavesAPointUnchangedWhenNeitherGridIsActive();
    void snapToGridSnapsOnlyFrequencyWhenOnlyTheFrequencyGridIsActive();
    void snapToGridSnapsOnlyTimeWhenOnlyTheTimingGridIsActive();
    void snapToGridSnapsBothAxesWhenBothGridsAreActive();

    // Grid Preset conversion (Qt <-> Qt-independent sound_mind::core types).
    void toFrequencyGridPresetConfigRoundTripsEveryField();
    void toTimingGridPresetConfigRoundTripsEveryField();
    void frequencyGridPresetConfigDashStylesRoundTripThroughAllThreeOptions();
};
