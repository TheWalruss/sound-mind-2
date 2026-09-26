#pragma once

#include <QObject>

class ToolPaletteControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerHasNoSelection();
    void setProjectDoesNotCrash();
    void beginAndCancelEachToolDoesNotCrash();
    void undoAndRedoOnEmptyHistoryDoNotCrash();
    void setPathPlacesSmoothNodesDoesNotCrash();
    void setGridSnappingDoesNotCrash();
    void pasteIntoWithNothingCopiedReturnsNullopt();
    void copyAndCutWithNoSelectionDoNotCrash();
    void fillWithNoSelectionDoesNotCrash();
    void applyFilterToSelectionWithNoSelectionDoesNotCrash();
    void contentChangedAggregatesAPaintStroke();
};
