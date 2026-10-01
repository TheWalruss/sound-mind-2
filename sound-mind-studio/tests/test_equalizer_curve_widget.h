#pragma once

#include <QObject>

class EqualizerCurveWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void freshWidgetHasTheDefaultTwoStopTransparentGradientAndSelectsTheFirstStop();
    void sizeHintReturnsAReasonableDefault();
    void clickingNearAnExistingStopSelectsItWithoutInserting();
    void clickingEmptyAreaInsertsANewSortedStopAndSelectsIt();
    void draggingAnInteriorStopClampsTBetweenItsNeighborsAndPreservesCutAmount();
    void theFirstAndLastStopsStayPutWhileDragging();
    void doubleClickingAnInteriorStopRemovesIt();
    void doubleClickingAnEndpointDoesNothing();
    void setGradientSyncsWithoutEmittingAndSelectsTheFirstStop();
    void setProjectSettingsAcceptsAValueAndNulloptWithoutCrashing();
};
