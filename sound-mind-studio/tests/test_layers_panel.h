#pragma once

#include <QObject>

class LayersPanelTest : public QObject {
    Q_OBJECT

private slots:
    void setLayersCreatesOneRowPerLayerTopFirst();
    void setLayersReplacesThePreviousRows();
    void visibilityButtonEmitsVisibilityCycleRequestedAndShowsTheCorrectIconPerState();
    void backgroundVisibilityButtonIsDisabled();
    void backgroundLayerHasNoOpacityOrTransformControls();
    void opacitySliderEmitsOpacityChanged();
    void balanceSliderEmitsBalanceChanged();
    void translationSpinBoxEmitsTranslationChanged();
    void rescaleSpinBoxEmitsRescaleChanged();
    void doubleClickingNameEmitsRenameRequested();
    void deleteButtonEmitsDeleteRequestedForNormalLayers();
    void duplicateButtonEmitsDuplicateRequestedForNormalLayers();
    void cleanUpPhaseButtonOnlyAppearsForASelectedRowWithContentAndEmitsCleanUpPhaseRequested();
    void poolButtonOnlyAppearsForASelectedRowWithContentAndEmitsPoolRequested();
    void poolButtonAppearsForASelectedBackgroundRowWithContentUnlikeDuplicateOrCleanUpPhase();
    void filterAndEqualizerRowsShowNoPoolButtonEvenWhenSelected();
    void lockedLayersHaveNoDeleteButton();
    void lockedLayersHaveALockIconInsteadOfADragHandle();
    void nonNormalLayersShowATypeTag();
    void filterAndEqualizerRowsShowAnEditFilterButtonUnconditionally();
    void normalAndBackgroundRowsShowNoEditFilterButton();
    void editFilterButtonEmitsEditFilterRequestedWithTheRowsOwnId();
    void freshPanelHasNoSelection();
    void clickingANameSelectsItsLayer();
    void selectionSurvivesASetLayersRefreshOfTheSameLayers();
    void selectionIsDroppedWhenTheSelectedLayerIsGoneFromANewSetLayersCall();
    void clearSelectionDropsTheSelectionAndItsHighlight();
    void addLayerButtonEmitsAddLayerRequested();
    void addFilterLayerButtonEmitsAddFilterLayerRequested();
    void generateLayerButtonEmitsGenerateLayerRequested();
    void selectLayerSelectsAMatchingRow();
    void selectLayerIsANoOpForAnUnknownId();
    void selectLayerEmitsSelectionChanged();
    void clearSelectionEmitsSelectionChangedWithNullopt();
    void setLayersEmitsSelectionChangedWhenTheSelectedLayerIsGone();
    void freshRowsOfferOnlyNoneUntilSetAvailableMindWavesIsCalled();
    void selectingCreateNewMindWaveCallsTheCallbackAndRewritesTheItemInPlace();
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
    void unselectedRowsShowNoOpacityOrTransformOrBlendModeControls();
    void selectingARowRevealsItsOwnControlsAndDeselectingHidesThemAgain();
    void hoveringAnUnselectedRowRevealsItsDeleteButtonAndLeavingHidesItAgain();
    void aRowsThumbnailIsShownWhenGivenAndAPlainBackgroundWhenNot();
    void mindWaveChildRowAppearsOnlyWhenBoundAndAPreviewImageExists();
    void mindWaveChildRowDisappearsWhenTheBindingIsCleared();

    // Per-layer loudness indicator (v0.Y.52.1, Analysis Tools v1).
    void aRowsLoudnessLabelShowsItsOwnLoudnessDisplayTextWhenNonEmpty();
    void aRowWithNoLoudnessDisplayTextHasNoLoudnessLabelAtAll();
    void setLiveLoudnessDisplayUpdatesOnlyTheGivenRowsLabelWithoutARebuild();
    void setLiveLoudnessDisplayIsANoOpForAnUnknownId();
};
