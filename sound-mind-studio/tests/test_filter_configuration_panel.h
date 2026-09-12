#pragma once

#include <QObject>

class FilterConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasAFullyTransparentDefaultConfiguration();
    void changingAStartSpinBoxUpdatesStop0AndEmitsFilterConfigurationChanged();
    void changingAnEndSpinBoxUpdatesStop1AndEmitsFilterConfigurationChanged();
    void setFilterConfigurationSyncsAllEightSpinBoxesWithoutEmitting();
    void freshPanelShowsOnlyTheFrequencyAxisGradientGroup();
    void selectingAFilterTypeShowsOnlyThatTypesOwnGroupAndEmitsTheNewType();
    void changingBlurSigmaUpdatesConfigAndEmits();
    void changingMedianSizeUpdatesConfigAndEmits();
    void changingDirectionalBlurLengthAndAngleUpdateConfigAndEmit();
    void changingSharpenAmountUpdatesConfigAndEmits();
    void setFilterConfigurationSyncsTheTypeComboAndNewSpinBoxesWithoutEmitting();
};
