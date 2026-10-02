#pragma once

#include <QObject>

class GridPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasBothAxisLabelsOff();
    void changingTheVerticalAxisComboEmitsVerticalAxisLabelModeChanged();
    void changingTheHorizontalAxisComboEmitsHorizontalAxisLabelModeChanged();

    void freshPanelHasNoFrequencyGridSourceActiveAndTimingGridOffAndSnapToGridOff();
    void checkingNoteGridEmitsFrequencyGridConfigChanged();
    void checkingHarmonicSeriesEnablesTheFundamentalSpinBoxAndEmits();
    void changingTheHarmonicFundamentalEmitsFrequencyGridConfigChanged();
    void checkingCustomFrequenciesEnablesTheLineEditAndEmits();
    void typingCustomFrequenciesParsesACommaSeparatedListIntoTheConfig();
    void changingTheFrequencyGridWidthEmitsFrequencyGridConfigChanged();
    void changingTheFrequencyGridDashStyleEmitsFrequencyGridConfigChanged();

    // Note Grid Key/Scale/Temperament/Octave filtering.
    void freshPanelDefaultsToEqual12ChromaticWithKeyAndScaleEnabled();
    void changingTemperamentToANonKeyScaleOneDisablesKeyAndScaleCombosAndEmits();
    void changingTemperamentBackToAKeyScaleOneReEnablesTheCombos();
    void changingScaleToMajorExcludesEveryNonScaleStepAndEmits();
    void changingKeyWithAMajorScaleRecomputesTheExcludedStepsForTheNewRoot();
    void changingScaleToChromaticClearsEveryExclusion();
    void changingTemperamentResetsExcludedStepsButLeavesExcludedOctavesAlone();
    void applyNoteSelectionExcludesUncheckedStepsAndEmits();
    void applyOctaveSelectionExcludesUncheckedOctavesAndEmits();

    void changingTheTimingGridModeToIntervalEnablesTheIntervalSpinBoxAndEmits();
    void changingTheTimingGridModeToTempoEnablesTheSubdivisionComboAndEmits();
    void changingTheTimingGridIntervalEmitsTimingGridConfigChanged();
    void changingTheTimingGridSubdivisionEmitsTimingGridConfigChanged();

    void togglingSnapToGridEmitsSnapToGridChanged();
};
