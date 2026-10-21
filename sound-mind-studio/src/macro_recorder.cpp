#include "sound_mind/studio/macro_recorder.h"

#include <chrono>
#include <cmath>

#include "sound_mind/studio/project_history.h"

namespace sound_mind::studio {

namespace {

double steadyNowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

MacroRecorder::MacroRecorder(ProjectHistory* history, QObject* parent)
    : QObject(parent), history_(history), now_(steadyNowSeconds) {
    connect(history_, &ProjectHistory::entryCommitted, this, [this](sound_mind::core::HistoryEntryId id) {
        addEntryEvent(sound_mind::core::MacroEventKind::Entry, id);
    });
    connect(history_, &ProjectHistory::entryUndone, this, [this](sound_mind::core::HistoryEntryId id) {
        addEntryEvent(sound_mind::core::MacroEventKind::Undo, id);
    });
    connect(history_, &ProjectHistory::entryRedone, this, [this](sound_mind::core::HistoryEntryId id) {
        addEntryEvent(sound_mind::core::MacroEventKind::Redo, id);
    });
}

std::optional<sound_mind::core::MacroId> MacroRecorder::start(sound_mind::core::Project& project, const QString& name,
                                                              double position, bool playing) {
    if (isRecording()) {
        return std::nullopt;
    }
    // Start from a recorded state - anything pending becomes an entry first.
    history_->commit(project, tr("Edit"));

    sound_mind::core::Macro macro;
    macro.name = name.toStdString();
    macro.settings = project.settings();
    const auto& history = project.history();
    macro.startAfterEntry = history.position() > 0 ? history.entry(history.position() - 1).id : 0;
    macro.historyStartsAtCreation = history.startsAtCreation();
    macro.recording = true;
    project_ = &project;
    macroId_ = project.addMacro(std::move(macro));

    accumulated_ = 0.0;
    running_ = false;
    lastPosition_ = position;
    lastPositionAt_ = now_();
    emit recordingStateChanged(true);
    notePlayhead(position, playing);
    return macroId_;
}

void MacroRecorder::stop() {
    if (!isRecording()) {
        return;
    }
    if (sound_mind::core::Macro* recorded = macro()) {
        recorded->durationSeconds = clockSeconds();
        recorded->recording = false;
    }
    macroId_.reset();
    project_ = nullptr;
    running_ = false;
    emit recordingStateChanged(false);
}

void MacroRecorder::discard() {
    if (!isRecording()) {
        return;
    }
    macroId_.reset();
    project_ = nullptr;
    running_ = false;
    emit recordingStateChanged(false);
}

double MacroRecorder::clockSeconds() const {
    return accumulated_ + (running_ ? now_() - runningSince_ : 0.0);
}

void MacroRecorder::notePlayhead(double positionSeconds, bool playing) {
    if (!isRecording()) {
        return;
    }
    const double now = now_();
    if (playing && !running_) {
        running_ = true;
        runningSince_ = now;
        add({clockSeconds(), sound_mind::core::MacroEventKind::Play, std::nullopt, positionSeconds});
    } else if (!playing && running_) {
        accumulated_ += now - runningSince_;
        running_ = false;
        add({clockSeconds(), sound_mind::core::MacroEventKind::Pause, std::nullopt, positionSeconds});
    } else if (playing) {
        const double expected = lastPosition_ + (now - lastPositionAt_);
        if (std::abs(positionSeconds - expected) > kSeekToleranceSeconds) {
            add({clockSeconds(), sound_mind::core::MacroEventKind::Seek, std::nullopt, positionSeconds});
        }
    }
    lastPosition_ = positionSeconds;
    lastPositionAt_ = now;
}

void MacroRecorder::addEntryEvent(sound_mind::core::MacroEventKind kind, sound_mind::core::HistoryEntryId id) {
    if (!isRecording() || project_ == nullptr) {
        return;
    }
    const auto& history = project_->history();
    for (std::size_t i = 0; i < history.size(); ++i) {
        if (history.entry(i).id != id) {
            continue;
        }
        sound_mind::core::HistoryEntry copy = history.entry(i);
        for (auto& change : copy.layers) {
            change.baseBefore.reset();
            change.baseAfter.reset();
        }
        add({clockSeconds(), kind, std::move(copy), 0.0});
        return;
    }
}

void MacroRecorder::add(sound_mind::core::MacroEvent event) {
    if (sound_mind::core::Macro* recorded = macro()) {
        recorded->events.push_back(std::move(event));
        emit eventRecorded();
    }
}

sound_mind::core::Macro* MacroRecorder::macro() {
    return project_ != nullptr && macroId_ ? project_->macroById(*macroId_) : nullptr;
}

}  // namespace sound_mind::studio
