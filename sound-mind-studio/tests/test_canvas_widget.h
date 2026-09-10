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
};
