#include "test_warp_dialog.h"

#include <QRadioButton>
#include <QtTest/QtTest>

#include "sound_mind/studio/warp_dialog.h"

using sound_mind::core::WarpAxis;
using sound_mind::core::WarpMode;
using sound_mind::studio::WarpDialog;

void WarpDialogTest::freshDialogDefaultsToFrequencyAxisAndDisplaceMode() {
    const WarpDialog dialog;
    QCOMPARE(dialog.selectedAxis(), WarpAxis::Frequency);
    QCOMPARE(dialog.selectedMode(), WarpMode::Displace);
}

void WarpDialogTest::selectingTimeAxisAndStretchModeUpdatesTheAccessors() {
    WarpDialog dialog;
    auto* timeRadio = dialog.findChild<QRadioButton*>(QStringLiteral("timeRadio"));
    auto* stretchRadio = dialog.findChild<QRadioButton*>(QStringLiteral("stretchRadio"));
    QVERIFY(timeRadio != nullptr);
    QVERIFY(stretchRadio != nullptr);

    timeRadio->setChecked(true);
    stretchRadio->setChecked(true);

    QCOMPARE(dialog.selectedAxis(), WarpAxis::Time);
    QCOMPARE(dialog.selectedMode(), WarpMode::Stretch);
}
