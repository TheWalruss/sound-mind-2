#pragma once

#include <QObject>

class MacroRecorderTest : public QObject {
    Q_OBJECT

private slots:
    void freshRecorderIsNotRecordingAndHasNoEvents();
    void startRecordingBeginsRecordingAndClearsPreviousEvents();
    void recordEventIsANoOpWhileNotRecording();
    void recordEventAppendsWhileRecording();
    void stopRecordingStopsButKeepsEventsAvailable();
    void startRecordingWhileAlreadyRecordingDoesNotClearEvents();
    void stopRecordingWhileNotRecordingIsANoOp();
    void recordingStateChangedEmitsOnStartAndStop();
};
