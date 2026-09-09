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
};
