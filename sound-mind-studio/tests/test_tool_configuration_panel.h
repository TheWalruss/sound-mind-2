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
};
