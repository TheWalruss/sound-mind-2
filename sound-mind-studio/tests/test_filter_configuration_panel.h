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
    void selectingToneCurveShowsItsOwnGroup();
    void editingTheToneCurveEditorUpdatesConfigAndEmits();
    void setFilterConfigurationSyncsTheToneCurveEditorWithoutEmitting();
    void equalizerModeHidesTheFilterTypeComboAndShowsTheCutGroup();
    void equalizerModeOffRestoresTheNormalPerTypeGroup();
    void editingACutSpinBoxWritesOpacityAndForcesIntensityToTheSilenceFloor();
    void setFilterConfigurationSyncsTheCutSpinBoxesFromOpacityWithoutEmitting();
    void freshCombosOfferOnlyNoneUntilSetAvailableMindWavesIsCalled();
    void setAvailableMindWavesPopulatesEveryCombo();
    void changingABindCombosEmitsFilterConfigurationChangedWithTheNewBinding();
    void selectingNoneUnbindsAndEmits();
    void setFilterConfigurationSyncsAllFiveCombosWithoutEmitting();

    // --- v0.Y.36.1 Installment A: Noise & distortion ---
    void freshPanelHasAllEightNoiseGroupsHidden();
    void selectingEachNoiseTypeShowsOnlyItsOwnGroup();
    void changingSpeckleAddDensityAndIntensityUpdateConfigAndEmit();
    void changingSpeckleThresholdUpdatesConfigAndEmits();
    void changingDenoiseNoiseFloorAndReductionUpdateConfigAndEmit();
    void changingCrushAmountUpdatesConfigAndEmits();
    void changingGrainSizeAndAmountUpdateConfigAndEmit();
    void changingDynamicSpeckleDensityAndIntensityUpdateTheSameFieldsAsSpeckleAdd();
    void changingFeedbackAmountUpdatesConfigAndEmits();
    void changingFoldGainUpdatesConfigAndEmits();
    void setFilterConfigurationSyncsAllNoiseSpinBoxesWithoutEmitting();
};
