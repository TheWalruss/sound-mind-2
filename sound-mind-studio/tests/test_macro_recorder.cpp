#include "test_macro_recorder.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/macro_recorder.h"

using sound_mind::core::LayerId;
using sound_mind::core::MindWaveId;
using sound_mind::studio::MacroEventType;
using sound_mind::studio::MacroRecorder;

void MacroRecorderTest::freshRecorderIsNotRecordingAndHasNoEvents() {
    MacroRecorder recorder;
    QVERIFY(!recorder.isRecording());
    QVERIFY(recorder.events().empty());
}

void MacroRecorderTest::startRecordingBeginsRecordingAndClearsPreviousEvents() {
    MacroRecorder recorder;
    recorder.startRecording();
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"));
    recorder.stopRecording();
    QCOMPARE(recorder.events().size(), std::size_t{1});

    recorder.startRecording();

    QVERIFY(recorder.isRecording());
    QVERIFY(recorder.events().empty());
}

void MacroRecorderTest::recordEventIsANoOpWhileNotRecording() {
    MacroRecorder recorder;
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"));
    QVERIFY(recorder.events().empty());
}

void MacroRecorderTest::recordEventAppendsWhileRecording() {
    MacroRecorder recorder;
    recorder.startRecording();

    recorder.recordEvent(2.5, MacroEventType::LayerVisibilityChanged, QStringLiteral("Changed layer visibility"),
                          static_cast<LayerId>(7));

    QCOMPARE(recorder.events().size(), std::size_t{1});
    QCOMPARE(recorder.events().front().timestampSeconds, 2.5);
    QCOMPARE(recorder.events().front().type, MacroEventType::LayerVisibilityChanged);
    QCOMPARE(recorder.events().front().description, QStringLiteral("Changed layer visibility"));
    QVERIFY(recorder.events().front().layerId.has_value());
    QCOMPARE(*recorder.events().front().layerId, static_cast<LayerId>(7));
    QVERIFY(!recorder.events().front().mindWaveId.has_value());
}

void MacroRecorderTest::stopRecordingStopsButKeepsEventsAvailable() {
    MacroRecorder recorder;
    recorder.startRecording();
    recorder.recordEvent(1.0, MacroEventType::PaintCommitted, QStringLiteral("Painted content"),
                          static_cast<LayerId>(1));

    recorder.stopRecording();

    QVERIFY(!recorder.isRecording());
    QCOMPARE(recorder.events().size(), std::size_t{1});
}

void MacroRecorderTest::startRecordingWhileAlreadyRecordingDoesNotClearEvents() {
    MacroRecorder recorder;
    recorder.startRecording();
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"));

    recorder.startRecording();  // Already recording - a no-op per its own docs.

    QCOMPARE(recorder.events().size(), std::size_t{1});
}

void MacroRecorderTest::stopRecordingWhileNotRecordingIsANoOp() {
    MacroRecorder recorder;
    QSignalSpy spy(&recorder, &MacroRecorder::recordingStateChanged);

    recorder.stopRecording();

    QVERIFY(!recorder.isRecording());
    QCOMPARE(spy.count(), 0);
}

void MacroRecorderTest::recordingStateChangedEmitsOnStartAndStop() {
    MacroRecorder recorder;
    QSignalSpy spy(&recorder, &MacroRecorder::recordingStateChanged);

    recorder.startRecording();
    recorder.stopRecording();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}
