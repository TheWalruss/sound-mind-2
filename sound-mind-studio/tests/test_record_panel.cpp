#include "test_record_panel.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/record_panel.h"

using sound_mind::studio::RecordPanel;

void RecordPanelTest::toggleButtonEmitsToggleRequested() {
    RecordPanel panel;
    QSignalSpy spy(&panel, &RecordPanel::toggleRequested);

    auto* button = panel.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void RecordPanelTest::setRecordingUpdatesButtonLabelAndCheckedState() {
    RecordPanel panel;
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("recordToggleButton"));
    QVERIFY(button != nullptr);

    panel.setRecording(true);
    QVERIFY(button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Stop Recording"));

    panel.setRecording(false);
    QVERIFY(!button->isChecked());
    QCOMPARE(button->text(), QStringLiteral("Start Recording"));
}
