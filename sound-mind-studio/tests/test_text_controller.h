#pragma once

#include <QObject>

class TextControllerTest : public QObject {
    Q_OBJECT

private slots:
    void renderGlyphsIsEmptyForBlankText();
    void renderGlyphsDrawsTheTextWithSomeCoverage();
    void rasterizeStretchesToTheBox();
    void draggingPlacesABoxBetweenTheTwoCorners();
    void aClickPlacesABoxTheTextsOwnSizeCentredOnIt();
    void applyWritesTheTextAsOneUndoStepAndClearsTheBox();
    void applyDoesNothingWithoutABoxOrText();
    void resetDiscardsTheBox();
    void appliedTextKeepsItsSettingsAndCanBeEditedWhenPicked();
};
