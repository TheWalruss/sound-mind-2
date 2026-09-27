#include "test_image_scale_picker_dialog.h"

#include <QCheckBox>
#include <QImage>
#include <QPushButton>
#include <QRadioButton>
#include <QtTest/QtTest>

#include "sound_mind/studio/image_scale_picker_dialog.h"

using sound_mind::studio::ImageScalePickerDialog;

namespace {

QImage makeTestImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGB888);
    image.fill(Qt::gray);
    return image;
}

}  // namespace

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

void ImageScalePickerDialogTest::polarRadioAndOriginButtonAreAbsentWithoutASourceImage() {
    // The default (null QImage) polarSourceImage - every pre-v0.Y.53.1
    // call site, and a real multi-file import (see the constructor's own
    // docs on why that never has a single image to preview).
    const ImageScalePickerDialog dialog;
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("polarRadio")) == nullptr);
    QVERIFY(dialog.findChild<QPushButton*>(QStringLiteral("polarOriginButton")) == nullptr);
}

void ImageScalePickerDialogTest::polarParamsIsNulloptWithoutASourceImage() {
    const ImageScalePickerDialog dialog;
    QVERIFY(!dialog.polarParams().has_value());
}

void ImageScalePickerDialogTest::polarRadioExistsWithASourceImage() {
    const ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/false, makeTestImage(100, 60));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("polarRadio")) != nullptr);
}

void ImageScalePickerDialogTest::selectingPolarUpdatesSelectedMode() {
    ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/false, makeTestImage(100, 60));
    auto* radio = dialog.findChild<QRadioButton*>(QStringLiteral("polarRadio"));
    QVERIFY(radio != nullptr);

    radio->setChecked(true);

    QCOMPARE(dialog.selectedMode(), ImageScalePickerDialog::Mode::Polar);
}

void ImageScalePickerDialogTest::polarOriginButtonIsHiddenUntilPolarIsSelected() {
    // isHidden(), not isVisible() - see CreateProjectWizardTest::
    // advancedFieldsAreHiddenUntilToggled()'s own identical comment on why
    // (isVisible() also requires the whole ancestor chain, the dialog
    // itself included, to be shown - never true in this headless test).
    ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/false, makeTestImage(100, 60));
    auto* polarRadio = dialog.findChild<QRadioButton*>(QStringLiteral("polarRadio"));
    auto* rescaleRadio = dialog.findChild<QRadioButton*>(QStringLiteral("rescaleToFitRadio"));
    auto* originButton = dialog.findChild<QPushButton*>(QStringLiteral("polarOriginButton"));
    QVERIFY(polarRadio != nullptr);
    QVERIFY(rescaleRadio != nullptr);
    QVERIFY(originButton != nullptr);
    QVERIFY(originButton->isHidden());

    polarRadio->setChecked(true);
    QVERIFY(!originButton->isHidden());

    rescaleRadio->setChecked(true);
    QVERIFY(originButton->isHidden());
}

void ImageScalePickerDialogTest::polarParamsDefaultsToTheImagesOwnCentreRadiusAndFullCircle() {
    const ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/false, makeTestImage(100, 60));

    const auto params = dialog.polarParams();
    QVERIFY(params.has_value());
    QCOMPARE(params->originX, 50.0);
    QCOMPARE(params->originY, 30.0);
    QVERIFY(qAbs(params->radius - 27.0) < 0.01);  // min(100,60)/2 * 0.9.
    QCOMPARE(params->arcStartRadians, 0.0);
    QCOMPARE(params->arcEndRadians, 0.0);
    // floor(2*pi * 27.0) - no project width was given.
    QCOMPARE(params->outputWidth, static_cast<std::uint32_t>(169));
}

void ImageScalePickerDialogTest::polarParamsUsesTheGivenDefaultOutputWidth() {
    const ImageScalePickerDialog dialog(nullptr, /*allowSequential=*/false, makeTestImage(100, 60),
                                         /*polarDefaultOutputWidth=*/1024);

    const auto params = dialog.polarParams();
    QVERIFY(params.has_value());
    QCOMPARE(params->outputWidth, static_cast<std::uint32_t>(1024));
}
