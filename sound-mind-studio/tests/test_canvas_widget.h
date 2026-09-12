#pragma once

#include <QObject>

class CanvasWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void sizeHintFallsBackWithNoProject();
    void sizeHintMatchesProjectCanvasDimensions();
    void rendersALayersContentInsteadOfThePlaceholder();
    void skipsAHiddenTopmostLayerInFavorOfTheOneBelowIt();
    void reflectsALayersTranslationColumns();
    void drawsAPlayheadLineAtTheGivenFraction();
    void drawsNoPlayheadByDefault();
    void toolModeDefaultsToNone();
    void setToolModeChangesTheMode();
    void mousePressDoesNothingInNoneMode();
    void mousePressInPaintModeEmitsPaintStrokeStartedWithAConvertedPoint();
    void mouseMoveWithoutAPriorPressDoesNothingInPaintMode();
    void mouseMoveAfterPressEmitsPaintStrokeContinued();
    void mouseReleaseEmitsPaintStrokeEndedAndEndsTheStroke();
    void changingToolModeAwayFromPaintCancelsAnyActiveStroke();
    void setPaintPreviewPathDrawsItOverTheCanvas();
    void setPaintPreviewPathWithNoNodesDrawsNothing();
    void setPreviewSelectedNodeIndexHighlightsThatNodeDistinctly();
    void showBoundingBoxesDrawsNothingWhenOff();
    void showBoundingBoxesDrawsAnActiveOperationsBoundingBox();
    void showPathGeometryDrawsAnActiveOperationsPath();
    void mouseMoveEmitsCursorMovedRegardlessOfToolMode();
    void leavingTheCanvasEmitsCursorLeft();
    void mousePressInPickModeEmitsPickStrokeStartedWithAConvertedPoint();
    void mouseMoveAfterPressInPickModeEmitsPickStrokeContinued();
    void mouseReleaseInPickModeEmitsPickStrokeEndedAndEndsTheGesture();
    void changingToolModeAwayFromPickCancelsAnyActiveGesture();
    void setPickSelectionBoundsDrawsAHighlight();
    void setPickSelectionBoundsIsHiddenWhileALivePreviewIsShowing();
    void setPickSelectionBoundsWithNoValueDrawsNothing();
    void mousePressInSelectModeEmitsSelectStrokeStartedWithAConvertedPoint();
    void mouseMoveAfterPressInSelectModeEmitsSelectStrokeContinued();
    void mouseReleaseInSelectModeEmitsSelectStrokeEndedAndEndsTheGesture();
    void changingToolModeAwayFromSelectCancelsAnyActiveGesture();
    void setSelectionBoundsDrawsAHighlight();
    void setSelectionBoundsWithNoValueDrawsNothing();
    void mousePressInPathModeEmitsPathNodePlacedWithAConvertedPoint();
    void mouseMoveAfterPressInPathModeDoesNotEmitPathNodePlacedAgain();
    void mouseReleaseInPathModeDoesNotEmitPathNodePlacedAgain();
    void setVerticalAxisLabelModeChangesWhatsDrawnNearTheLeftEdge();
    void setHorizontalAxisLabelModeChangesWhatsDrawnNearTheBottomEdge();
    void setFrequencyGridConfigDrawsHorizontalLines();
    void setTimingGridConfigDrawsVerticalLines();
};
