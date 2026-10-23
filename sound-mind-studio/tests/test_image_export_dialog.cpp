#include "test_image_export_dialog.h"

#include <filesystem>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QtTest/QtTest>

#include "sound_mind/studio/image_export_dialog.h"
#include "sound_mind/studio/numeric_field.h"

using sound_mind::studio::ImageExportDialog;
using sound_mind::studio::IntegerField;

namespace {

const std::filesystem::path kSuggested = std::filesystem::temp_directory_path() / "sound-mind-test-image.png";

/// The dialogs below are for a 1024 x 512 canvas, whose Sound Flower is 1024 x 1024.

}  // namespace

// v0.1.52.1 - File -> Export Image...
void ImageExportDialogTest::fullResolutionIsTheDefault() {
    const ImageExportDialog dialog(kSuggested, false, QSize(1024, 512), QSize(1024, 1024));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("imageFullResolutionRadio"))->isChecked());
    QVERIFY(!dialog.findChild<IntegerField*>(QStringLiteral("imageWidthField"))->isEnabled());
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("imageFullResolutionRadio"))->text().contains(
        QStringLiteral("1024 x 512")));
    const auto options = dialog.options();
    QCOMPARE(options.width, 0U);  // full resolution.
    QCOMPARE(options.height, 0U);
    QVERIFY(!options.polar);
    QCOMPARE(dialog.outputPath(), kSuggested);
    QCOMPARE(dialog.findChild<QComboBox*>(QStringLiteral("imageFormatCombo"))->currentText(), QStringLiteral("PNG"));
}

void ImageExportDialogTest::aCustomSizeKeepsTheCanvasProportions() {
    ImageExportDialog dialog(kSuggested, false, QSize(1024, 512), QSize(1024, 1024));
    dialog.findChild<QRadioButton*>(QStringLiteral("imageCustomSizeRadio"))->setChecked(true);
    auto* width = dialog.findChild<IntegerField*>(QStringLiteral("imageWidthField"));
    auto* height = dialog.findChild<IntegerField*>(QStringLiteral("imageHeightField"));
    QVERIFY(width->isEnabled());
    width->setValue(512);
    QCOMPARE(height->value(), 256);
    QCOMPARE(dialog.options().width, 512U);
    QCOMPARE(dialog.options().height, 256U);

    dialog.findChild<QCheckBox*>(QStringLiteral("imageKeepProportionsCheckBox"))->setChecked(false);
    height->setValue(100);
    QCOMPARE(width->value(), 512);  // no longer linked.
    QCOMPARE(dialog.options().height, 100U);
}

void ImageExportDialogTest::choosingAFormatChangesTheFileExtension() {
    ImageExportDialog dialog(kSuggested, false, QSize(1024, 512), QSize(1024, 1024));
    auto* format = dialog.findChild<QComboBox*>(QStringLiteral("imageFormatCombo"));
    format->setCurrentIndex(format->findText(QStringLiteral("JPEG")));
    QCOMPARE(dialog.outputPath().extension().string(), std::string(".jpg"));

    // And a typed extension picks the format.
    auto* path = dialog.findChild<QLineEdit*>(QStringLiteral("imagePathEdit"));
    path->setText(QString::fromStdWString((std::filesystem::temp_directory_path() / "x.webp").wstring()));
    emit path->editingFinished();
    QCOMPARE(format->currentText(), QStringLiteral("WebP"));
}

void ImageExportDialogTest::aSoundFlowerIsSquareAtItsOwnFullResolution() {
    ImageExportDialog dialog(kSuggested, true, QSize(1024, 512), QSize(1024, 1024));
    QVERIFY(dialog.findChild<QCheckBox*>(QStringLiteral("imagePolarCheckBox"))->isChecked());
    QVERIFY(dialog.options().polar);
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("imageFullResolutionRadio"))->text().contains(
        QStringLiteral("1024 x 1024")));
    dialog.findChild<QRadioButton*>(QStringLiteral("imageCustomSizeRadio"))->setChecked(true);
    dialog.findChild<IntegerField*>(QStringLiteral("imageWidthField"))->setValue(300);
    QCOMPARE(dialog.findChild<IntegerField*>(QStringLiteral("imageHeightField"))->value(), 300);  // square.
}

void ImageExportDialogTest::anUnknownExtensionCantBeAccepted() {
    ImageExportDialog dialog(kSuggested, false, QSize(1024, 512), QSize(1024, 1024));
    auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QVERIFY(ok->isEnabled());
    auto* path = dialog.findChild<QLineEdit*>(QStringLiteral("imagePathEdit"));
    path->setText(QStringLiteral("picture.gif"));
    QVERIFY(!ok->isEnabled());
    path->setText(QString());
    QVERIFY(!ok->isEnabled());
}
