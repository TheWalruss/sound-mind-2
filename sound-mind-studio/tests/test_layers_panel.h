#pragma once

#include <QObject>

class LayersPanelTest : public QObject {
    Q_OBJECT

private slots:
    void setLayersCreatesOneRowPerLayerTopFirst();
    void setLayersReplacesThePreviousRows();
    void visibilityButtonEmitsVisibilityToggled();
    void backgroundVisibilityButtonIsDisabled();
    void backgroundLayerHasNoOpacityOrTransformControls();
    void opacitySliderEmitsOpacityChanged();
    void translationSpinBoxEmitsTranslationChanged();
    void rescaleSpinBoxEmitsRescaleChanged();
    void doubleClickingNameEmitsRenameRequested();
    void deleteButtonEmitsDeleteRequestedForNormalLayers();
    void duplicateButtonEmitsDuplicateRequestedForNormalLayers();
    void cleanUpPhaseButtonOnlyAppearsForASelectedRowWithContentAndEmitsCleanUpPhaseRequested();
    void lockedLayersHaveNoDeleteButton();
    void lockedLayersHaveALockIconInsteadOfADragHandle();
    void nonNormalLayersShowATypeTag();
    void freshPanelHasNoSelection();
    void clickingANameSelectsItsLayer();
    void selectionSurvivesASetLayersRefreshOfTheSameLayers();
    void selectionIsDroppedWhenTheSelectedLayerIsGoneFromANewSetLayersCall();
    void clearSelectionDropsTheSelectionAndItsHighlight();
    void addLayerButtonEmitsAddLayerRequested();
    void addFilterLayerButtonEmitsAddFilterLayerRequested();
    void selectLayerSelectsAMatchingRow();
    void selectLayerIsANoOpForAnUnknownId();
    void selectLayerEmitsSelectionChanged();
    void clearSelectionEmitsSelectionChangedWithNullopt();
    void setLayersEmitsSelectionChangedWhenTheSelectedLayerIsGone();
    void freshRowsOfferOnlyNoneUntilSetAvailableMindWavesIsCalled();
    void setAvailableMindWavesPopulatesEveryRowsComboImmediately();
    void aRowsComboPreselectsItsOwnCurrentBinding();
    void changingARowsMindWaveComboEmitsOpacityMindWaveChanged();
    void selectingNoneEmitsOpacityMindWaveChangedWithNullopt();
    void contentIsInAResizableScrollAreaSoThePanelCanShrinkBelowItsFullHeight();

    // Mind Grains ordering-rule guardrail (v0.Y.33.1 Installment B).
    void setDisallowedLayersMarksTheGivenRowsWithARedX();
    void setDisallowedLayersLeavesOtherRowsUnmarked();
    void setDisallowedLayersWithAnEmptyListClearsEveryMark();

    // Blend Mode (v0.Y.37.1).
    void backgroundLayerHasNoBlendModeCombo();
    void aRowsBlendModeComboDefaultsToNormalAndPreselectsItsOwnValue();
    void changingARowsBlendModeComboEmitsBlendModeChanged();

    // Layers Panel Redesign (v0.Y.44.1).
    void unselectedRowsShowNoOpacityOrTransformOrBlendModeOrDeleteControls();
    void selectingARowRevealsItsOwnControlsAndDeselectingHidesThemAgain();
    void aRowsThumbnailIsShownWhenGivenAndAPlainBackgroundWhenNot();
    void mindWaveChildRowAppearsOnlyWhenBoundAndAPreviewImageExists();
    void mindWaveChildRowDisappearsWhenTheBindingIsCleared();
};
