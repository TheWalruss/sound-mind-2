#pragma once

#include <QObject>

class PathControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerHasNoPlacementInProgress();
    void placeNodeStartsPlacementAndAppendsToThePath();
    void placeNodeIsANoOpWithNoProject();
    void placeNodeASecondTimeAppendsToTheSameInProgressPathRegardlessOfLayerArgument();
    void defaultNodeTypeDefaultsToCorner();
    void setDefaultNodeTypeAffectsSubsequentlyPlacedNodesOnly();
    void placingASmoothNodeCollapsesBothHandlesOntoItsAnchor();
    void updateCursorAddsATransientNodeToThePreviewPathButNotTheRealPath();
    void updateCursorIsANoOpWithNoPlacementInProgress();
    void finishPathAppendsANewPaintOperationAndRebuildsTheLayer();
    void finishPathClearsThePlacementAndThePreview();
    void finishPathIsANoOpWithNoPlacementInProgress();
    void cancelPathDiscardsWithoutCommittingAnything();
    void cancelPathIsANoOpWithNoPlacementInProgress();
    void setProjectCancelsAnyInProgressPlacement();
};
