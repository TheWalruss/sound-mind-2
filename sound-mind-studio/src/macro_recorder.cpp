#include "sound_mind/studio/macro_recorder.h"

namespace sound_mind::studio {

MacroRecorder::MacroRecorder(QObject* parent) : QObject(parent) {}

void MacroRecorder::startRecording(std::size_t startUndoIndex) {
    if (recording_) {
        return;
    }
    events_.clear();
    startUndoIndex_ = startUndoIndex;
    recording_ = true;
    emit recordingStateChanged(true);
}

void MacroRecorder::stopRecording() {
    if (!recording_) {
        return;
    }
    recording_ = false;
    emit recordingStateChanged(false);
}

void MacroRecorder::recordEvent(double timestampSeconds, MacroEventType type, const QString& description,
                                 std::size_t undoStackIndexAfter, std::optional<sound_mind::core::LayerId> layerId,
                                 std::optional<sound_mind::core::MindWaveId> mindWaveId) {
    if (!recording_) {
        return;
    }
    MacroEvent event;
    event.timestampSeconds = timestampSeconds;
    event.type = type;
    event.description = description;
    event.layerId = layerId;
    event.mindWaveId = mindWaveId;
    event.undoStackIndexAfter = undoStackIndexAfter;
    events_.push_back(std::move(event));
}

void MacroRecorder::discardEvents() {
    events_.clear();
    startUndoIndex_ = 0;
    if (recording_) {
        recording_ = false;
        emit recordingStateChanged(false);
    }
}

}  // namespace sound_mind::studio
