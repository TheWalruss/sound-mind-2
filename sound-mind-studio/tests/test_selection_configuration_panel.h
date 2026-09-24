#pragma once

#include <QObject>

class SelectionConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelDefaultsToRectangle();
    void changingTheSelectionTypeComboEmitsSelectionShapeChangedAndUpdatesSelectionShape();

    // Wand (v0.Y.35.1 Installment B).
    void freshPanelHasTheWandGroupHiddenAndDefaultWandSettings();
    void switchingToWandShowsTheWandGroupAndSwitchingAwayHidesItAgain();
    void changingToleranceEmitsWandToleranceChangedAndUpdatesWandTolerance();
    void togglingHarmonicsAwareEmitsWandHarmonicsAwareChangedAndUpdatesWandHarmonicsAware();

    // Paste Blend Mode (v0.Y.37.1).
    void freshPanelDefaultsPasteBlendModeToOverwrite();
    void changingThePasteBlendModeComboUpdatesPasteBlendMode();
    void changingThePasteBlendModeComboEmitsPasteBlendModeChanged();
    void setPasteBlendModeUpdatesTheComboWithoutEmittingPasteBlendModeChanged();
    void clickingDeselectEmitsDeselectRequested();
};
