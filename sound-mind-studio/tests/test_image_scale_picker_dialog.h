#pragma once

#include <QObject>

class ImageScalePickerDialogTest : public QObject {
    Q_OBJECT

private slots:
    void defaultsToRescaleToFitProject();
    void selectingScaleVerticalKeepHorizontalUpdatesSelectedMode();
    void selectingScaleHorizontalKeepVerticalUpdatesSelectedMode();
    void selectingScaleVerticalProportionalUpdatesSelectedMode();
    void selectingKeepNativeResolutionUpdatesSelectedMode();

    // Image Sequence Import (v0.Y.22.1)
    void sequentialCheckBoxIsAbsentByDefault();
    void sequentialCheckBoxExistsWhenAllowed();
    void sequentialCheckBoxStartsUnchecked();
    void checkingSequentialCheckBoxSetsImportAsSequence();
    void checkingSequentialCheckBoxDisablesTheModeRadios();

    // Polar-form image import (v0.Y.53.1 Installment B).
    void polarRadioAndOriginButtonAreAbsentWithoutASourceImage();
    void polarParamsIsNulloptWithoutASourceImage();
    void polarRadioExistsWithASourceImage();
    void selectingPolarUpdatesSelectedMode();
    void polarOriginButtonIsHiddenUntilPolarIsSelected();
    void polarParamsDefaultsToTheImagesOwnCentreRadiusAndFullCircle();
    void polarParamsUsesTheGivenDefaultOutputWidth();
};
