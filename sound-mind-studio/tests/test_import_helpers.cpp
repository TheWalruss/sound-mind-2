#include "test_import_helpers.h"

#include <QtTest/QtTest>

#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/import_helpers.h"
#include "sound_mind/studio/qt_image_conversion.h"

using sound_mind::codec::CompressedAudioFormat;
using sound_mind::studio::ImageScalePickerDialog;

void ImportHelpersTest::lowercasedExtensionLowersCase() {
    QCOMPARE(QString::fromStdString(sound_mind::studio::lowercasedExtension("C:/some/path/FOO.PNG")),
             QStringLiteral(".png"));
}

void ImportHelpersTest::isImageExtensionRecognizesEachSupportedExtension() {
    QVERIFY(sound_mind::studio::isImageExtension(".png"));
    QVERIFY(sound_mind::studio::isImageExtension(".jpg"));
    QVERIFY(sound_mind::studio::isImageExtension(".jpeg"));
    QVERIFY(sound_mind::studio::isImageExtension(".bmp"));
    QVERIFY(sound_mind::studio::isImageExtension(".tga"));
    QVERIFY(sound_mind::studio::isImageExtension(".webp"));
}

void ImportHelpersTest::isImageExtensionRejectsNonImageExtensions() {
    QVERIFY(!sound_mind::studio::isImageExtension(".wav"));
    QVERIFY(!sound_mind::studio::isImageExtension(".smproj"));
    QVERIFY(!sound_mind::studio::isImageExtension(""));
}

void ImportHelpersTest::audioFormatFromExtensionMapsRecognizedExtensions() {
    QCOMPARE(sound_mind::studio::audioFormatFromExtension("out.flac"), CompressedAudioFormat::Flac);
    QCOMPARE(sound_mind::studio::audioFormatFromExtension("out.ogg"), CompressedAudioFormat::Ogg);
    QCOMPARE(sound_mind::studio::audioFormatFromExtension("out.mp3"), CompressedAudioFormat::Mp3);
    // Case-insensitive, matching lowercasedExtension()'s own normalization.
    QCOMPARE(sound_mind::studio::audioFormatFromExtension("OUT.FLAC"), CompressedAudioFormat::Flac);
}

void ImportHelpersTest::audioFormatFromExtensionReturnsNulloptForUnrecognized() {
    QVERIFY(!sound_mind::studio::audioFormatFromExtension("out.wav").has_value());
    QVERIFY(!sound_mind::studio::audioFormatFromExtension("out").has_value());
}

void ImportHelpersTest::formatSnippetIndexZeroPadsToFourDigits() {
    QCOMPARE(QString::fromStdString(sound_mind::studio::formatSnippetIndex(0)), QStringLiteral("0000"));
    QCOMPARE(QString::fromStdString(sound_mind::studio::formatSnippetIndex(42)), QStringLiteral("0042"));
    QCOMPARE(QString::fromStdString(sound_mind::studio::formatSnippetIndex(12345)), QStringLiteral("12345"));
}

void ImportHelpersTest::scaleImageForImportRescaleToFitProjectStretchesBothAxes() {
    const QImage source(30, 20, QImage::Format_RGB32);
    const QImage scaled =
        sound_mind::studio::scaleImageForImport(source, ImageScalePickerDialog::Mode::RescaleToFitProject, 100, 50);
    QCOMPARE(scaled.width(), 100);
    QCOMPARE(scaled.height(), 50);
}

void ImportHelpersTest::scaleImageForImportScaleVerticalKeepHorizontalKeepsNativeWidth() {
    const QImage source(30, 20, QImage::Format_RGB32);
    const QImage scaled = sound_mind::studio::scaleImageForImport(
        source, ImageScalePickerDialog::Mode::ScaleVerticalKeepHorizontal, 100, 50);
    QCOMPARE(scaled.width(), 30);
    QCOMPARE(scaled.height(), 50);
}

void ImportHelpersTest::scaleImageForImportScaleHorizontalKeepVerticalKeepsNativeHeight() {
    const QImage source(30, 20, QImage::Format_RGB32);
    const QImage scaled = sound_mind::studio::scaleImageForImport(
        source, ImageScalePickerDialog::Mode::ScaleHorizontalKeepVertical, 100, 50);
    QCOMPARE(scaled.width(), 100);
    QCOMPARE(scaled.height(), 20);
}

void ImportHelpersTest::scaleImageForImportScaleVerticalProportionalPreservesAspectRatio() {
    const QImage source(30, 20, QImage::Format_RGB32);
    const QImage scaled = sound_mind::studio::scaleImageForImport(
        source, ImageScalePickerDialog::Mode::ScaleVerticalProportional, 100, 50);
    QCOMPARE(scaled.height(), 50);
    QCOMPARE(scaled.width(), 75);  // 30 * 50 / 20, preserving the 3:2 aspect ratio.
}

void ImportHelpersTest::scaleImageForImportKeepNativeResolutionDoesNotRescale() {
    const QImage source(30, 20, QImage::Format_RGB32);
    const QImage scaled =
        sound_mind::studio::scaleImageForImport(source, ImageScalePickerDialog::Mode::KeepNativeResolution, 100, 50);
    QCOMPARE(scaled.width(), 30);
    QCOMPARE(scaled.height(), 20);
}

void ImportHelpersTest::toRgbImageAndToQImageViewRoundTrip() {
    QImage source(4, 3, QImage::Format_RGB32);
    source.fill(QColor(10, 20, 30));

    const auto rgbImage = sound_mind::studio::toRgbImage(source);
    QCOMPARE(rgbImage.width, static_cast<std::uint32_t>(4));
    QCOMPARE(rgbImage.height, static_cast<std::uint32_t>(3));

    const QImage roundTripped = sound_mind::studio::toQImageView(rgbImage);
    const QColor pixel = roundTripped.pixelColor(1, 1);
    QCOMPARE(pixel.red(), 10);
    QCOMPARE(pixel.green(), 20);
    QCOMPARE(pixel.blue(), 30);
}
