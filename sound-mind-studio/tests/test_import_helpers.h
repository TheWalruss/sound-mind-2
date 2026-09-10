#pragma once

#include <QObject>

class ImportHelpersTest : public QObject {
    Q_OBJECT

private slots:
    void lowercasedExtensionLowersCase();
    void isImageExtensionRecognizesEachSupportedExtension();
    void isImageExtensionRejectsNonImageExtensions();
    void audioFormatFromExtensionMapsRecognizedExtensions();
    void audioFormatFromExtensionReturnsNulloptForUnrecognized();
    void formatSnippetIndexZeroPadsToFourDigits();
    void scaleImageForImportRescaleToFitProjectStretchesBothAxes();
    void scaleImageForImportScaleVerticalKeepHorizontalKeepsNativeWidth();
    void scaleImageForImportScaleHorizontalKeepVerticalKeepsNativeHeight();
    void scaleImageForImportScaleVerticalProportionalPreservesAspectRatio();
    void scaleImageForImportKeepNativeResolutionDoesNotRescale();
    void toRgbImageAndToQImageViewRoundTrip();
};
