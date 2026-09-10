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
    void lockedLayersHaveNoDeleteButton();
    void lockedLayersHaveALockIconInsteadOfADragHandle();
    void nonNormalLayersShowATypeTag();
    void freshPanelHasNoSelection();
    void clickingANameSelectsItsLayer();
    void selectionSurvivesASetLayersRefreshOfTheSameLayers();
    void selectionIsDroppedWhenTheSelectedLayerIsGoneFromANewSetLayersCall();
    void clearSelectionDropsTheSelectionAndItsHighlight();
};
