#pragma once

#include <QObject>

class ToneCurveEditorTest : public QObject {
    Q_OBJECT

private slots:
    void freshEditorHasTheDefaultTwoPointIdentityCurve();
    void sizeHintReturnsAReasonableDefault();
    void clickingNearAnExistingPointBeginsDraggingItInstead();
    void clickingEmptyAreaInsertsANewSortedPoint();
    void draggingAnInteriorPointClampsXBetweenItsNeighbors();
    void theFirstAndLastPointsKeepXPinnedWhileDragging();
    void doubleClickingAnInteriorPointRemovesIt();
    void doubleClickingAnEndpointDoesNothing();
    void setPointsSyncsWithoutEmitting();
};
