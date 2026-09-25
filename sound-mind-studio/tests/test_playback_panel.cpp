#include "test_playback_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QVariant>
#include <QtTest/QtTest>

#include "sound_mind/studio/playback_panel.h"

using sound_mind::studio::PlaybackPanel;
using sound_mind::studio::PlaybackScope;

void PlaybackPanelTest::playPauseStopButtonsEmitTheirSignals() {
    PlaybackPanel panel;
    QSignalSpy playSpy(&panel, &PlaybackPanel::playRequested);
    QSignalSpy pauseSpy(&panel, &PlaybackPanel::pauseRequested);
    QSignalSpy stopSpy(&panel, &PlaybackPanel::stopRequested);

    auto* playButton = panel.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* pauseButton = panel.findChild<QPushButton*>(QStringLiteral("pauseButton"));
    auto* stopButton = panel.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(pauseButton != nullptr);
    QVERIFY(stopButton != nullptr);

    playButton->click();
    pauseButton->click();
    stopButton->click();

    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(pauseSpy.count(), 1);
    QCOMPARE(stopSpy.count(), 1);
}

void PlaybackPanelTest::positionSliderStartsAtZero() {
    PlaybackPanel panel;
    auto* slider = panel.findChild<QSlider*>(QStringLiteral("positionSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 0);
}

void PlaybackPanelTest::movingThePositionSliderEmitsSeekRequested() {
    PlaybackPanel panel;
    panel.setDuration(10.0);
    QSignalSpy spy(&panel, &PlaybackPanel::seekRequested);

    auto* slider = panel.findChild<QSlider*>(QStringLiteral("positionSlider"));
    QVERIFY(slider != nullptr);
    slider->setValue(slider->maximum() / 2);  // halfway.

    QCOMPARE(spy.count(), 1);
    QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 5.0) < 0.01);
}

void PlaybackPanelTest::setPositionSecondsDoesNotEmitSeekRequested() {
    PlaybackPanel panel;
    panel.setDuration(10.0);
    QSignalSpy spy(&panel, &PlaybackPanel::seekRequested);

    panel.setPositionSeconds(5.0);

    QCOMPARE(spy.count(), 0);
    auto* slider = panel.findChild<QSlider*>(QStringLiteral("positionSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), slider->maximum() / 2);
}

void PlaybackPanelTest::repeatCheckBoxEmitsRepeatChanged() {
    PlaybackPanel panel;
    QSignalSpy spy(&panel, &PlaybackPanel::repeatChanged);

    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("repeatCheckBox"));
    QVERIFY(checkBox != nullptr);
    checkBox->setChecked(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
}

void PlaybackPanelTest::setRepeatCheckedDoesNotEmitRepeatChanged() {
    PlaybackPanel panel;
    QSignalSpy spy(&panel, &PlaybackPanel::repeatChanged);

    panel.setRepeatChecked(true);

    QCOMPARE(spy.count(), 0);
    auto* checkBox = panel.findChild<QCheckBox*>(QStringLiteral("repeatCheckBox"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(checkBox->isChecked());
}

void PlaybackPanelTest::scopeComboDefaultsToTrackAndEmitsScopeChanged() {
    PlaybackPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("scopeCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(static_cast<PlaybackScope>(combo->currentData().toInt()), PlaybackScope::Track);

    int callCount = 0;
    PlaybackScope capturedScope = PlaybackScope::Track;
    QObject::connect(&panel, &PlaybackPanel::scopeChanged, [&](PlaybackScope scope) {
        ++callCount;
        capturedScope = scope;
    });
    const int deltaIndex = combo->findData(QVariant::fromValue(static_cast<int>(PlaybackScope::Delta)));
    combo->setCurrentIndex(deltaIndex);

    QCOMPARE(callCount, 1);
    QCOMPARE(capturedScope, PlaybackScope::Delta);
}

void PlaybackPanelTest::setPositionSecondsUpdatesTheTimeLabel() {
    PlaybackPanel panel;
    panel.setDuration(65.0);  // 1:05.

    panel.setPositionSeconds(5.0);  // 0:05.

    auto* label = panel.findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:05 / 1:05"));
}
