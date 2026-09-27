#include "test_mind_capture_dialog.h"

#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QtTest/QtTest>

#include "sound_mind/studio/mind_capture_dialog.h"

using sound_mind::studio::MindCaptureDialog;

void MindCaptureDialogTest::freshDialogHasTheGivenNameAndZeroForBothNumericFields() {
    const MindCaptureDialog dialog(QStringLiteral("Capture Mind Shot"), QStringLiteral("Mind Shot 1"));
    QCOMPARE(dialog.name(), QStringLiteral("Mind Shot 1"));
    QCOMPARE(dialog.fundamentalFrequencyHz(), 0.0);
    QCOMPARE(dialog.startTimeOffsetSeconds(), 0.0);
}

void MindCaptureDialogTest::windowTitleMatchesWhatWasPassedIn() {
    const MindCaptureDialog dialog(QStringLiteral("Capture Mind Grain"), QStringLiteral("Mind Grain 1"));
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Capture Mind Grain"));
}

void MindCaptureDialogTest::nameIsTrimmedOfLeadingAndTrailingWhitespace() {
    MindCaptureDialog dialog(QStringLiteral("Capture Mind Shot"), QStringLiteral("Mind Shot 1"));
    auto* nameLineEdit = dialog.findChild<QLineEdit*>(QStringLiteral("nameLineEdit"));
    QVERIFY(nameLineEdit != nullptr);

    nameLineEdit->setText(QStringLiteral("  Piano Hit  "));

    QCOMPARE(dialog.name(), QStringLiteral("Piano Hit"));
}

void MindCaptureDialogTest::fundamentalFrequencyHzAndStartTimeOffsetSecondsReflectTheSpinBoxes() {
    MindCaptureDialog dialog(QStringLiteral("Capture Mind Shot"), QStringLiteral("Mind Shot 1"));
    auto* fundamentalFrequencySpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("fundamentalFrequencySpinBox"));
    auto* startTimeOffsetSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("startTimeOffsetSpinBox"));
    QVERIFY(fundamentalFrequencySpinBox != nullptr);
    QVERIFY(startTimeOffsetSpinBox != nullptr);

    fundamentalFrequencySpinBox->setValue(261.63);
    startTimeOffsetSpinBox->setValue(0.05);

    QCOMPARE(dialog.fundamentalFrequencyHz(), 261.63);
    QCOMPARE(dialog.startTimeOffsetSeconds(), 0.05);
}
