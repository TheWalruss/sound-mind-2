#pragma once

#include <QObject>

class ImageExportDialogTest : public QObject {
    Q_OBJECT

private slots:
    void fullResolutionIsTheDefault();
    void aCustomSizeKeepsTheCanvasProportions();
    void choosingAFormatChangesTheFileExtension();
    void aSoundFlowerIsSquareAtItsOwnFullResolution();
    void anUnknownExtensionCantBeAccepted();
};
