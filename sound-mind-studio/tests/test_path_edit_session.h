#pragma once

#include <QObject>

class PathEditSessionTest : public QObject {
    Q_OBJECT

private slots:
    void freshSessionIsNotActive();
    void beginCopiesTheInitialPathAndSelectsNothing();
    void endClearsActiveStateAndThePreview();
    void selectNodeNearFindsTheClosestAnchorWithinTolerance();
    void selectNodeNearFindsNothingBeyondTolerance();
    void selectNodeNearPrefersAHandleOverANearbyAnchor();
    void continueDragMovesTheAnchorAndBothHandles();
    void continueDragWithNothingSelectedDoesNothing();
    void deleteSelectedNodeRemovesItButRefusesToEmptyThePath();
    void toggleSelectedNodeTypeSmoothsAndUnsmooths();
};
