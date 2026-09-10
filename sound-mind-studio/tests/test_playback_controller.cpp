#include "test_playback_controller.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/playback_controller.h"

using sound_mind::codec::AudioBuffer;
using sound_mind::studio::PlaybackController;

namespace {

/// @brief One second of silence at 44100 Hz - enough to exercise
/// load()/play()/seek() without needing a real audio file, matching
/// sound-mind-core's own test_playback_engine.cpp's makeTestAudio() (not
/// reusable directly - that one lives in sound-mind-core's own test
/// binary).
AudioBuffer makeTestAudio() {
    AudioBuffer audio;
    audio.sampleRateHz = 44100;
    audio.left.assign(44100, 0.0f);
    audio.right.assign(44100, 0.0f);
    return audio;
}

}  // namespace

void PlaybackControllerTest::freshControllerIsNotLoadedOrPlaying() {
    const PlaybackController controller;
    QVERIFY(!controller.isLoaded());
    QVERIFY(!controller.isPlaying());
}

void PlaybackControllerTest::loadMarksItLoaded() {
    PlaybackController controller;
    controller.load(makeTestAudio());
    QVERIFY(controller.isLoaded());
}

void PlaybackControllerTest::invalidateClearsLoadedWithoutStoppingPlayback() {
    PlaybackController controller;
    controller.load(makeTestAudio());
    controller.play();

    controller.invalidate();

    QVERIFY(!controller.isLoaded());
    QVERIFY(controller.isPlaying());  // uninterrupted - see invalidate()'s own docs.

    controller.stop();  // cleanup.
}

void PlaybackControllerTest::loadEmitsDurationChanged() {
    PlaybackController controller;
    QSignalSpy spy(&controller, &PlaybackController::durationChanged);

    controller.load(makeTestAudio());

    QCOMPARE(spy.count(), 1);
    QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 1.0) < 0.01);  // one second.
    QVERIFY(qAbs(controller.totalSeconds() - 1.0) < 0.01);
}

void PlaybackControllerTest::playStartsPlayback() {
    PlaybackController controller;
    controller.load(makeTestAudio());

    controller.play();

    QVERIFY(controller.isPlaying());

    controller.stop();  // cleanup.
}

void PlaybackControllerTest::pauseStopsPlaybackButKeepsItLoaded() {
    PlaybackController controller;
    controller.load(makeTestAudio());
    controller.play();

    controller.pause();

    QVERIFY(!controller.isPlaying());
    QVERIFY(controller.isLoaded());
}

void PlaybackControllerTest::stopClearsLoadedAndPlaying() {
    PlaybackController controller;
    controller.load(makeTestAudio());
    controller.play();

    controller.stop();

    QVERIFY(!controller.isPlaying());
    QVERIFY(!controller.isLoaded());
}

void PlaybackControllerTest::seekEmitsPositionChangedImmediately() {
    PlaybackController controller;
    controller.load(makeTestAudio());
    QSignalSpy spy(&controller, &PlaybackController::positionChanged);

    controller.seek(0.5);

    QCOMPARE(spy.count(), 1);
    QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 0.5) < 0.01);
}

void PlaybackControllerTest::seekDoesNothingWhenNotLoaded() {
    PlaybackController controller;
    QSignalSpy spy(&controller, &PlaybackController::positionChanged);

    controller.seek(0.5);  // nothing loaded - should be a harmless no-op.

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.isPlaying());
}

void PlaybackControllerTest::setVolumeChangesVolume() {
    PlaybackController controller;
    controller.setVolume(150);
    QVERIFY(qFuzzyCompare(controller.volume(), 1.5f));
}

void PlaybackControllerTest::outputDeviceMethodsAreCallableWithoutCrashing() {
    PlaybackController controller;
    // Populate the device manager's device types first - see
    // PlaybackEngine::setPreferredOutputDevice()'s own docs for why this
    // real JUCE quirk needs a real device query before a bogus name is
    // actually rejected rather than trivially "succeeding".
    (void)controller.availableOutputDeviceNames();
    QVERIFY(!controller.setOutputDevice(QStringLiteral("definitely not a real device name")));
    (void)controller.currentOutputDeviceName();  // no real device guaranteed in CI - just confirm it's well-formed.
}
