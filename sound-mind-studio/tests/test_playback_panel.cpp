#include "test_playback_panel.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest/QtTest>

#include "sound_mind/studio/playback_panel.h"

using sound_mind::studio::PlaybackPanel;

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

void PlaybackPanelTest::setOutputDevicesListsSystemDefaultFirst() {
    PlaybackPanel panel;
    panel.setOutputDevices({QStringLiteral("Speakers A"), QStringLiteral("Speakers B")});

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemData(0).toString(), QString());
    QCOMPARE(combo->itemData(1).toString(), QStringLiteral("Speakers A"));
    QCOMPARE(combo->itemData(2).toString(), QStringLiteral("Speakers B"));
}

void PlaybackPanelTest::changingTheOutputDeviceEmitsOutputDeviceChanged() {
    PlaybackPanel panel;
    panel.setOutputDevices({QStringLiteral("Speakers A")});
    QSignalSpy spy(&panel, &PlaybackPanel::outputDeviceChanged);

    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("outputDeviceCombo"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Speakers A"));
}

void PlaybackPanelTest::volumeSliderStartsAtUnityAndAllowsAboveIt() {
    PlaybackPanel panel;
    auto* slider = panel.findChild<QSlider*>(QStringLiteral("volumeSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 100);
    QCOMPARE(slider->maximum(), PlaybackPanel::kMaxVolumePercent);
    QVERIFY(PlaybackPanel::kMaxVolumePercent > 100);  // a real boost past unity is allowed.
}

void PlaybackPanelTest::movingTheVolumeSliderEmitsVolumePercentChanged() {
    PlaybackPanel panel;
    QSignalSpy spy(&panel, &PlaybackPanel::volumePercentChanged);

    auto* slider = panel.findChild<QSlider*>(QStringLiteral("volumeSlider"));
    QVERIFY(slider != nullptr);
    slider->setValue(150);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 150);
}

void PlaybackPanelTest::setVolumePercentDoesNotEmitVolumePercentChanged() {
    PlaybackPanel panel;
    QSignalSpy spy(&panel, &PlaybackPanel::volumePercentChanged);

    panel.setVolumePercent(150);

    QCOMPARE(spy.count(), 0);
    auto* slider = panel.findChild<QSlider*>(QStringLiteral("volumeSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), 150);
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

void PlaybackPanelTest::setPositionSecondsUpdatesTheTimeLabel() {
    PlaybackPanel panel;
    panel.setDuration(65.0);  // 1:05.

    panel.setPositionSeconds(5.0);  // 0:05.

    auto* label = panel.findChild<QLabel*>(QStringLiteral("positionLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), QStringLiteral("0:05 / 1:05"));
}
