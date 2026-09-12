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

    void changingTheTimingGridModeToIntervalEnablesTheIntervalSpinBoxAndEmits();
    void changingTheTimingGridModeToTempoEnablesTheSubdivisionComboAndEmits();
    void changingTheTimingGridIntervalEmitsTimingGridConfigChanged();
    void changingTheTimingGridSubdivisionEmitsTimingGridConfigChanged();

    void togglingSnapToGridEmitsSnapToGridChanged();
};
