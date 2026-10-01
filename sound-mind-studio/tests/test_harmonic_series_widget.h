#pragma once

#include <QObject>

class HarmonicSeriesWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void freshWidgetHasNoBars();
    void sizeHintReturnsAReasonableDefault();
    void setHarmonicStrengthsSyncsWithoutEmitting();
    void clickingABarSetsItsValueAndEmits();
    void draggingKeepsUpdatingTheOriginalColumnEvenIfTheCursorLeavesIt();
    void aClickAboveTheTopClampsToTheDisplayCeiling();
    void renderThumbnailProducesAnImageOfTheRequestedSizeWithVisibleContent();
    void renderThumbnailOfAnEmptySeriesIsFullyTransparent();
};
