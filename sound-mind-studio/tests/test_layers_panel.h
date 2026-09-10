#pragma once

#include <QObject>

class LayersPanelTest : public QObject {
    Q_OBJECT

private slots:
    void setLayersCreatesOneRowPerLayerTopFirst();
    void setLayersReplacesThePreviousRows();
    void visibilityButtonEmitsVisibilityToggled();
    void backgroundVisibilityButtonIsDisabled();
    void opacitySliderEmitsOpacityChanged();
    void translationSpinBoxEmitsTranslationChanged();
    void rescaleSpinBoxEmitsRescaleChanged();
    void doubleClickingNameEmitsRenameRequested();
    void deleteButtonEmitsDeleteRequestedForNormalLayers();
    void lockedLayersHaveNoDeleteButton();
    void lockedLayersHaveALockIconInsteadOfADragHandle();
    void nonNormalLayersShowATypeTag();
};
