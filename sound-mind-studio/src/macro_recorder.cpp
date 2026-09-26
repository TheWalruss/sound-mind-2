#include "sound_mind/studio/macro_recorder.h"

namespace sound_mind::studio {

MacroRecorder::MacroRecorder(QObject* parent) : QObject(parent) {}

void MacroRecorder::startRecording() {
    if (recording_) {
        return;
    }
    events_.clear();
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
                                 std::optional<sound_mind::core::LayerId> layerId,
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
    events_.push_back(std::move(event));
}

}  // namespace sound_mind::studio
