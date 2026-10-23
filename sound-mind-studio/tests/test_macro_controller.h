#pragma once

#include <QObject>

class MacroControllerTest : public QObject {
    Q_OBJECT

private slots:
    void recordingAddsANamedMacroAndStopsWithItsDuration();
    void recordingNeedsAProjectAndIgnoresRepeats();
    void thePlayheadIsPolledWhileRecording();
    void aRecordedMacroHasARenderer();
    void aMissingMacroHasNoRendererAndDoesNotPlay();
    void anotherProjectForgetsARecordingInProgress();
};
