#include "test_macro_recorder.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/macro_manager_dialog.h"
#include "sound_mind/studio/macro_recorder.h"
#include "sound_mind/studio/project_history.h"

using sound_mind::core::Macro;
using sound_mind::core::MacroEventKind;
using sound_mind::core::Project;
using sound_mind::studio::MacroRecorder;
using sound_mind::studio::ProjectHistory;

namespace {

Project smallProject() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 16;
    settings.canvasHeight = 8;
    return Project::createNew(settings);
}

/// A recorder on a fake clock the test moves by hand.
struct Rig {
    Project project = smallProject();
    ProjectHistory history;
    MacroRecorder recorder{&history};
    double now = 100.0;

    Rig() {
        history.setProject(&project);
        recorder.setTimeSource([this]() { return now; });
    }

    void change(float opacity) {
        project.layers().front().setOpacity(opacity);
        history.commit(project, QStringLiteral("Opacity"));
    }

    const Macro& macro() { return project.macros().back(); }
};

}  // namespace

// v0.1.44.1 - the redesigned recorder (architecture Decision #262).
void MacroRecorderTest::startAddsARecordingMacroStartingAfterTheLatestEntry() {
    Rig rig;
    rig.change(0.9f);
    const auto latest = rig.project.history().entry(0).id;
    QSignalSpy state(&rig.recorder, &MacroRecorder::recordingStateChanged);
    const auto id = rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    QVERIFY(id.has_value());
    QVERIFY(rig.recorder.isRecording());
    QCOMPARE(state.count(), 1);
    QCOMPARE(rig.project.macros().size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(rig.macro().name), QStringLiteral("Take"));
    QCOMPARE(rig.macro().startAfterEntry, latest);
    QVERIFY(rig.macro().recording);
    QVERIFY(rig.macro().historyStartsAtCreation);
}

void MacroRecorderTest::startCommitsPendingChangesFirst() {
    Rig rig;
    rig.project.layers().front().setOpacity(0.3f);  // not committed.
    rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    QCOMPARE(rig.project.history().size(), std::size_t{1});
    QCOMPARE(rig.macro().startAfterEntry, rig.project.history().entry(0).id);
    QVERIFY(rig.macro().events.empty());  // the pending change is the start, not an event.
}

void MacroRecorderTest::theClockRunsOnlyWhilePlaying() {
    Rig rig;
    rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    rig.now += 5.0;  // stopped: doesn't count.
    QCOMPARE(rig.recorder.clockSeconds(), 0.0);
    rig.recorder.notePlayhead(1.0, true);
    rig.now += 2.0;
    rig.recorder.notePlayhead(3.0, true);
    QCOMPARE(rig.recorder.clockSeconds(), 2.0);
    rig.recorder.notePlayhead(3.0, false);
    rig.now += 10.0;
    QCOMPARE(rig.recorder.clockSeconds(), 2.0);

    const auto& events = rig.macro().events;
    QCOMPARE(events.size(), std::size_t{2});
    QCOMPARE(events[0].kind, MacroEventKind::Play);
    QCOMPARE(events[0].time, 0.0);
    QCOMPARE(events[0].position, 1.0);
    QCOMPARE(events[1].kind, MacroEventKind::Pause);
    QCOMPARE(events[1].time, 2.0);
    QCOMPARE(events[1].position, 3.0);
}

void MacroRecorderTest::changesWhilePausedLandAtTheFrozenTime() {
    Rig rig;
    rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    rig.recorder.notePlayhead(0.0, true);
    rig.now += 1.5;
    rig.recorder.notePlayhead(1.5, false);
    rig.now += 30.0;  // a long pause to set things up.
    rig.change(0.5f);
    rig.now += 5.0;
    rig.change(0.25f);

    const auto& events = rig.macro().events;
    QCOMPARE(events.size(), std::size_t{4});
    QCOMPARE(events[2].kind, MacroEventKind::Entry);
    QCOMPARE(events[2].time, 1.5);
    QCOMPARE(events[3].time, 1.5);
    QVERIFY(events[3].entry.has_value());
    QCOMPARE(QString::fromStdString(events[3].entry->description), QStringLiteral("Opacity"));
}

void MacroRecorderTest::aJumpOfThePlayheadIsASeek() {
    Rig rig;
    rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    rig.recorder.notePlayhead(4.0, true);
    rig.now += 1.0;
    rig.recorder.notePlayhead(5.0, true);  // steady: nothing.
    rig.now += 1.0;
    rig.recorder.notePlayhead(0.0, true);  // a Repeat wrap / new Loop pass.
    const auto& events = rig.macro().events;
    QCOMPARE(events.size(), std::size_t{2});
    QCOMPARE(events[1].kind, MacroEventKind::Seek);
    QCOMPARE(events[1].time, 2.0);
    QCOMPARE(events[1].position, 0.0);
}

void MacroRecorderTest::undoAndRedoAreRecordedAsEvents() {
    Rig rig;
    rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    rig.change(0.5f);
    rig.change(0.4f);
    QVERIFY(rig.history.jumpTo(0));  // one Undo event per entry.
    QVERIFY(rig.history.redo());
    const auto& events = rig.macro().events;
    QCOMPARE(events.size(), std::size_t{5});
    QCOMPARE(events[2].kind, MacroEventKind::Undo);
    QCOMPARE(events[3].kind, MacroEventKind::Undo);
    QCOMPARE(events[2].entry->id, rig.project.history().entry(1).id);  // newest first.
    QCOMPARE(events[4].kind, MacroEventKind::Redo);
}

void MacroRecorderTest::stopKeepsTheMacroWithItsDuration() {
    Rig rig;
    rig.recorder.start(rig.project, QStringLiteral("Take"), 0.0, false);
    rig.recorder.notePlayhead(0.0, true);
    rig.now += 7.0;
    QSignalSpy state(&rig.recorder, &MacroRecorder::recordingStateChanged);
    rig.recorder.stop();
    QVERIFY(!rig.recorder.isRecording());
    QCOMPARE(state.count(), 1);
    QCOMPARE(rig.macro().durationSeconds, 7.0);
    QVERIFY(!rig.macro().recording);
    rig.change(0.1f);  // after stopping: not recorded.
    QCOMPARE(rig.macro().events.size(), std::size_t{1});
}

void MacroRecorderTest::startingWhilePlayingBeginsWithPlay() {
    Rig rig;
    rig.recorder.start(rig.project, QStringLiteral("Take"), 12.0, true);
    QCOMPARE(rig.macro().events.size(), std::size_t{1});
    QCOMPARE(rig.macro().events[0].kind, MacroEventKind::Play);
    QCOMPARE(rig.macro().events[0].position, 12.0);
}

void MacroRecorderTest::startWhileRecordingIsRefusedAndDiscardForgets() {
    Rig rig;
    QVERIFY(rig.recorder.start(rig.project, QStringLiteral("A"), 0.0, false).has_value());
    QVERIFY(!rig.recorder.start(rig.project, QStringLiteral("B"), 0.0, false).has_value());
    rig.recorder.discard();
    QVERIFY(!rig.recorder.isRecording());
    rig.change(0.2f);
    QVERIFY(rig.macro().events.empty());
}

void MacroRecorderTest::managerDialogRenamesAndDeletesButNotWhileRecording() {
    Rig rig;
    const auto first = *rig.recorder.start(rig.project, QStringLiteral("First"), 0.0, false);
    rig.recorder.stop();
    const auto second = *rig.recorder.start(rig.project, QStringLiteral("Second"), 0.0, false);

    sound_mind::studio::MacroManagerDialog dialog(rig.project);
    QVERIFY(!dialog.renameMacro(second, QStringLiteral("Nope")));  // still recording.
    QVERIFY(!dialog.deleteMacro(second));
    QVERIFY(!dialog.renameMacro(first, QStringLiteral("  ")));
    QVERIFY(dialog.renameMacro(first, QStringLiteral("Opening")));
    QCOMPARE(QString::fromStdString(rig.project.macroById(first)->name), QStringLiteral("Opening"));
    QVERIFY(dialog.deleteMacro(first));
    QVERIFY(rig.project.macroById(first) == nullptr);
    QVERIFY(dialog.changed());
}
