#pragma once

#include <QObject>

class ToolConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelIsAnOpaqueCircularBrush();
    void freshPanelHasBothOverlayCheckboxesOff();
    void changingTheTipShapeEmitsToolConfigurationChanged();
    void changingFalloffEmitsToolConfigurationChanged();
    void changingSizeEmitsToolConfigurationChanged();
    void changingOpacitySetsBothGradientStopsOpacity();
    void togglingShowBoundingBoxesEmitsItsOwnSignal();
    void togglingShowPathGeometryEmitsItsOwnSignal();
    void setColorSetsBothGradientStopsIntensityAndEmitsChange();
    void colorRoundTripsThroughSetColor();
    void colorButtonExistsForOpeningTheRealDialog();
    void freshPanelHasStampModeStrokeAndTheIntervalSpinBoxDisabled();
    void changingTheStampModeEmitsToolConfigurationChangedAndEnablesTheIntervalSpinBox();
    void changingTheStampIntervalEmitsToolConfigurationChanged();
    void loadingAConfigurationSyncsTheStampModeAndIntervalControls();

    // Sound Mind Instruments (v0.Y.32.1).
    void freshPanelDefaultsToProceduralWithTheProceduralGroupVisible();
    void switchingToolTypeToInstrumentShowsItsOwnGroupAndHidesProcedural();
    void switchingToolTypeToInstrumentPreservesSharedFields();
    void switchingToolTypeBackToProceduralRestoresTheProceduralGroup();
    void changingHarmonicCountResizesTheStrengthRows();
    void changingAHarmonicStrengthEmitsToolConfigurationChanged();
    void changingInharmonicityEmitsToolConfigurationChanged();
    void loadingAnInstrumentConfigurationSyncsToolTypeAndHarmonicControls();

    // Mind Shots (v0.Y.33.1 Installment A).
    void switchingToolTypeToMindShotShowsItsOwnGroupAndHidesProcedural();
    void setProjectPopulatesTheMindShotCombo();
    void refreshMindShotsAddsNewEntriesAndPreservesTheCurrentSelection();
    void refreshMindShotsShowsThePlaceholderWhenTheLibraryIsEmpty();
    void selectingAMindShotEmitsToolConfigurationChangedWithItsClip();
    void loadingAMindShotConfigurationSyncsToolTypeAndThePickerSelection();

    // Mind Grains (v0.Y.33.1 Installment B).
    void switchingToolTypeToMindGrainShowsItsOwnGroupAndHidesProcedural();
    void setProjectPopulatesTheMindGrainCombo();
    void refreshMindGrainsAddsNewEntriesAndPreservesTheCurrentSelection();
    void refreshMindGrainsShowsThePlaceholderWhenTheLibraryIsEmpty();
    void selectingAMindGrainEmitsToolConfigurationChangedWithItsReference();
    void loadingAMindGrainConfigurationSyncsToolTypeAndThePickerSelection();
    void setActiveLayerHighlightsTheGroupWhenTheActiveLayerIsNotAboveTheSource();
    void setActiveLayerClearsTheHighlightWhenTheActiveLayerIsAboveTheSource();

    // Heal/Soften (v0.Y.34.1 Installment A).
    void switchingToolTypeToHealHidesEveryOtherGroup();
    void switchingToolTypeToSoftenHidesEveryOtherGroup();
    void switchingToolTypeToHealPreservesSharedFields();
    void loadingAHealConfigurationSyncsToolType();
    void loadingASoftenConfigurationSyncsToolType();
};
