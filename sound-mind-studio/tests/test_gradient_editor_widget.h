#pragma once

#include <QObject>

class GradientEditorWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void freshEditorHasTheDefaultTwoStopTransparentGradient();
    void spinBoxesReflectTheInitiallySelectedStop();
    void editingASpinBoxUpdatesTheSelectedStopAndEmits();
    void linkChannelsMirrorsLeftEditsToRight();
    void linkChannelsMirrorsRightEditsToLeft();
    void selectingADifferentStopOnTheBarUpdatesTheSpinBoxes();
    void deleteStopButtonIsDisabledForEndpointsAndEnabledForInterior();
    void deleteStopButtonRemovesTheSelectedInteriorStop();
    void setGradientSyncsWithoutEmittingIncludingLinkChannels();
};
