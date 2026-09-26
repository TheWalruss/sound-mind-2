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
    QCOMPARE(recorder.startUndoIndex(), std::size_t{0});
}

void MacroRecorderTest::startRecordingBeginsRecordingAndClearsPreviousEvents() {
    MacroRecorder recorder;
    recorder.startRecording(3);
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"), 3);
    recorder.stopRecording();
    QCOMPARE(recorder.events().size(), std::size_t{1});

    recorder.startRecording(5);

    QVERIFY(recorder.isRecording());
    QVERIFY(recorder.events().empty());
    QCOMPARE(recorder.startUndoIndex(), std::size_t{5});
}

void MacroRecorderTest::recordEventIsANoOpWhileNotRecording() {
    MacroRecorder recorder;
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"), 0);
    QVERIFY(recorder.events().empty());
}

void MacroRecorderTest::recordEventAppendsWhileRecording() {
    MacroRecorder recorder;
    recorder.startRecording(0);

    recorder.recordEvent(2.5, MacroEventType::LayerVisibilityChanged, QStringLiteral("Changed layer visibility"), 4,
                          static_cast<LayerId>(7));

    QCOMPARE(recorder.events().size(), std::size_t{1});
    QCOMPARE(recorder.events().front().timestampSeconds, 2.5);
    QCOMPARE(recorder.events().front().type, MacroEventType::LayerVisibilityChanged);
    QCOMPARE(recorder.events().front().description, QStringLiteral("Changed layer visibility"));
    QCOMPARE(recorder.events().front().undoStackIndexAfter, std::size_t{4});
    QVERIFY(recorder.events().front().layerId.has_value());
    QCOMPARE(*recorder.events().front().layerId, static_cast<LayerId>(7));
    QVERIFY(!recorder.events().front().mindWaveId.has_value());
}

void MacroRecorderTest::stopRecordingStopsButKeepsEventsAvailable() {
    MacroRecorder recorder;
    recorder.startRecording(0);
    recorder.recordEvent(1.0, MacroEventType::PaintCommitted, QStringLiteral("Painted content"), 1,
                          static_cast<LayerId>(1));

    recorder.stopRecording();

    QVERIFY(!recorder.isRecording());
    QCOMPARE(recorder.events().size(), std::size_t{1});
}

void MacroRecorderTest::startRecordingWhileAlreadyRecordingDoesNotClearEvents() {
    MacroRecorder recorder;
    recorder.startRecording(0);
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"), 0);

    recorder.startRecording(9);  // Already recording - a no-op per its own docs.

    QCOMPARE(recorder.events().size(), std::size_t{1});
    QCOMPARE(recorder.startUndoIndex(), std::size_t{0});
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

    recorder.startRecording(0);
    recorder.stopRecording();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void MacroRecorderTest::discardEventsClearsEventsAndStartUndoIndex() {
    MacroRecorder recorder;
    recorder.startRecording(2);
    recorder.recordEvent(1.0, MacroEventType::PlaybackStarted, QStringLiteral("Started playback"), 2);
    recorder.stopRecording();

    recorder.discardEvents();

    QVERIFY(recorder.events().empty());
    QCOMPARE(recorder.startUndoIndex(), std::size_t{0});
}

void MacroRecorderTest::discardEventsWhileRecordingAlsoStopsRecording() {
    MacroRecorder recorder;
    recorder.startRecording(0);
    QSignalSpy spy(&recorder, &MacroRecorder::recordingStateChanged);

    recorder.discardEvents();

    QVERIFY(!recorder.isRecording());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
}
