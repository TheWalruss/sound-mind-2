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
    void editingTheGradientEditorUpdatesDefaultGradientAndEmits();
    void togglingShowBoundingBoxesEmitsItsOwnSignal();
    void togglingShowPathGeometryEmitsItsOwnSignal();
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

    // MindWaves v2 Installment A: Instrument vibrato/tremolo (v0.Y.39.1).
    void setAvailableMindWavesPopulatesBothVibratoAndTremoloCombos();
    void changingVibratoDepthEmitsToolConfigurationChanged();
    void changingTheVibratoComboEmitsToolConfigurationChangedWithTheNewBinding();
    void selectingNoneOnTheTremoloComboUnbindsAndEmits();
    void loadingAnInstrumentConfigurationSyncsVibratoAndTremoloControls();
    void switchingAwayFromAndBackToInstrumentPreservesVibratoAndTremoloBindings();

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

    // Smudge/Order-Chaos (v0.Y.34.1 Installment B).
    void switchingToolTypeToSmudgeHidesEveryOtherGroup();
    void loadingASmudgeConfigurationSyncsToolType();
    void switchingToolTypeToOrderChaosShowsItsOwnGroupAndHidesProcedural();
    void changingAmountEmitsToolConfigurationChanged();
    void loadingAnOrderChaosConfigurationSyncsToolTypeAndAmount();

    // Shared control visibility review (v0.Y.34.1 Installment C).
    void proceduralShowsEverySharedControl();
    void mindShotHidesFalloffSizeAndGradientEditorButKeepsStampControls();
    void mindGrainHidesFalloffSizeAndGradientEditorButKeepsStampControls();
    void healHidesIntensityAndStampControlsButKeepsFalloffSizeAndOpacity();
    void softenHidesIntensityAndStampControls();
    void smudgeHidesIntensityAndStampControls();
    void orderChaosHidesIntensityAndStampControls();
    void switchingFromHealBackToProceduralPreservesTheOriginalStampMode();

    // Blend Mode (v0.Y.37.1).
    void proceduralHidesTheBlendModeCombo();
    void mindShotShowsTheBlendModeComboDefaultedToOverwrite();
    void mindGrainShowsTheBlendModeComboDefaultedToOverwrite();
    void changingTheBlendModeComboEmitsToolConfigurationChangedWithTheNewMode();
    void loadingAMindShotConfigurationSyncsTheBlendModeCombo();
};
