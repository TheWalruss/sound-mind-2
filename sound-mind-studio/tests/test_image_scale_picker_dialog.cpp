#include "test_image_scale_picker_dialog.h"

#include <QCheckBox>
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

void ImageScalePickerDialogTest::sequentialCheckBoxIsAbsentByDefault() {
    // allowSequential defaults to false - single-file importImage() calls
    // (and every pre-v0.Y.22.1 test) still construct the dialog with just a
    // parent, so this must keep behaving exactly as before.
    const ImageScalePickerDialog dialog;
    QVERIFY(dialog.findChild<QCheckBox*>(QStringLiteral("sequentialCheckBox")) == nullptr);
}

void ImageScalePickerDialogTest::sequentialCheckBoxExistsWhenAllowed() {
    const ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/true);
    QVERIFY(dialog.findChild<QCheckBox*>(QStringLiteral("sequentialCheckBox")) != nullptr);
}

void ImageScalePickerDialogTest::sequentialCheckBoxStartsUnchecked() {
    const ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/true);
    QVERIFY(!dialog.importAsSequence());
}

void ImageScalePickerDialogTest::checkingSequentialCheckBoxSetsImportAsSequence() {
    ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/true);
    auto* checkBox = dialog.findChild<QCheckBox*>(QStringLiteral("sequentialCheckBox"));
    QVERIFY(checkBox != nullptr);

    checkBox->setChecked(true);

    QVERIFY(dialog.importAsSequence());
}

void ImageScalePickerDialogTest::checkingSequentialCheckBoxDisablesTheModeRadios() {
    ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/true);
    auto* checkBox = dialog.findChild<QCheckBox*>(QStringLiteral("sequentialCheckBox"));
    auto* radio = dialog.findChild<QRadioButton*>(QStringLiteral("rescaleToFitRadio"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(radio != nullptr);
    QVERIFY(radio->isEnabled());

    checkBox->setChecked(true);
    QVERIFY(!radio->isEnabled());

    checkBox->setChecked(false);
    QVERIFY(radio->isEnabled());
}
