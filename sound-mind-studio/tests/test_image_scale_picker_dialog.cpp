#include "test_image_scale_picker_dialog.h"

#include <QRadioButton>
#include <QtTest/QtTest>

#include "sound_mind/studio/image_scale_picker_dialog.h"

using sound_mind::studio::ImageScalePickerDialog;

void ImageScalePickerDialogTest::defaultsToRescaleToFitProject() {
    ImageScalePickerDialog dialog;
    QCOMPARE(dialog.selectedMode(), ImageScalePickerDialog::Mode::RescaleToFitProject);
}

void ImageScalePickerDialogTest::selectingScaleVerticalKeepHorizontalUpdatesSelectedMode() {
    ImageScalePickerDialog dialog;
    auto* radio = dialog.findChild<QRadioButton*>(QStringLiteral("scaleVerticalKeepHorizontalRadio"));
    QVERIFY(radio != nullptr);
    radio->setChecked(true);
    QCOMPARE(dialog.selectedMode(), ImageScalePickerDialog::Mode::ScaleVerticalKeepHorizontal);
}

void ImageScalePickerDialogTest::selectingScaleHorizontalKeepVerticalUpdatesSelectedMode() {
    ImageScalePickerDialog dialog;
    auto* radio = dialog.findChild<QRadioButton*>(QStringLiteral("scaleHorizontalKeepVerticalRadio"));
    QVERIFY(radio != nullptr);
    radio->setChecked(true);
    QCOMPARE(dialog.selectedMode(), ImageScalePickerDialog::Mode::ScaleHorizontalKeepVertical);
}

void ImageScalePickerDialogTest::selectingScaleVerticalProportionalUpdatesSelectedMode() {
    ImageScalePickerDialog dialog;
    auto* radio = dialog.findChild<QRadioButton*>(QStringLiteral("scaleVerticalProportionalRadio"));
    QVERIFY(radio != nullptr);
    radio->setChecked(true);
    QCOMPARE(dialog.selectedMode(), ImageScalePickerDialog::Mode::ScaleVerticalProportional);
}

void ImageScalePickerDialogTest::selectingKeepNativeResolutionUpdatesSelectedMode() {
    ImageScalePickerDialog dialog;
    auto* radio = dialog.findChild<QRadioButton*>(QStringLiteral("keepNativeResolutionRadio"));
    QVERIFY(radio != nullptr);
    radio->setChecked(true);
    QCOMPARE(dialog.selectedMode(), ImageScalePickerDialog::Mode::KeepNativeResolution);
}
