#pragma once

#include <QObject>

class GradientBarWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void freshBarHasTheDefaultTwoStopTransparentGradient();
    void sizeHintReturnsAReasonableDefault();
    void clickingNearAnExistingStopSelectsItWithoutInserting();
    void clickingEmptyAreaInsertsANewSortedStopAndSelectsIt();
    void draggingAnInteriorStopClampsTBetweenItsNeighbors();
    void theFirstAndLastStopsStayPutWhileDragging();
    void doubleClickingAnInteriorStopRemovesIt();
    void doubleClickingAnEndpointDoesNothing();
    void setGradientSyncsWithoutEmittingAndSelectsTheFirstStop();
    void removeSelectedStopRefusesAnEndpoint();
    void setSelectedStopValuesAppliesWithoutMovingOrChangingSelection();
    void setLinkChannelsEmitsWithTheFlagSet();
};
