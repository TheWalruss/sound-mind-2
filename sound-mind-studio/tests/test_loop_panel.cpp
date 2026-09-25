#include "test_loop_panel.h"

#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/loop_panel.h"

using sound_mind::studio::LoopPanel;

void LoopPanelTest::toggleButtonEmitsToggleRequested() {
    LoopPanel panel;
    QSignalSpy spy(&panel, &LoopPanel::toggleRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void LoopPanelTest::setRunningUpdatesButtonLabelAndCheckedState() {
    LoopPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("loopToggleButton"));
    QVERIFY(button != nullptr);

    panel.setRunning(true);
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Loop"));

    panel.setRunning(false);
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Loop"));
}

void LoopPanelTest::keepLoopingCheckBoxEmitsKeepLoopingChanged() {
    LoopPanel panel;
    QSignalSpy spy(&panel, &LoopPanel::keepLoopingChanged);

    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);
    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}

void LoopPanelTest::keepLoopingCheckBoxIsLabeledFreezeLoop() {
    // The checkbox's internal name (keepLoopingCheckBox) and the
    // setKeepLooping()/keepLoopingChanged() API it drives are unchanged -
    // only the user-visible label was renamed, "Keep Looping" not being
    // descriptive of what the checkbox actually does (confirmed with the
    // user - "Freeze Loop" matches the standard loop-pedal term for
    // freezing the current loop rather than recording over it).
    LoopPanel panel;
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);
    QCOMPARE(checkBox->text(), QStringLiteral("Freeze Loop"));
}

void LoopPanelTest::setKeepLoopingCheckedDoesNotEmitKeepLoopingChanged() {
    LoopPanel panel;
    QSignalSpy spy(&panel, &LoopPanel::keepLoopingChanged);

    panel.setKeepLoopingChecked(true);

    QCOMPARE(spy.count(), 0);
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(checkBox->isChecked());
}
