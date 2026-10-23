#include "test_macro_controller.h"

#include <optional>
#include <utility>

#include <QStatusBar>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/macro_controller.h"
#include "sound_mind/studio/project_history.h"

using sound_mind::core::Project;
using sound_mind::studio::MacroController;
using sound_mind::studio::ProjectChange;
using sound_mind::studio::ProjectHistory;

namespace {

Project smallProject() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 16;
    settings.canvasHeight = 8;
    return Project::createNew(settings);
}

/// A controller over its own project and History, with a playhead the test
/// moves by hand and no player.
struct Fixture {
    Fixture()
        : controller(
              MacroController::Context{
                  [this]() -> Project* { return hasProject ? &project : nullptr; },
                  [this]() {
                      ++playheadPolls;
                      return std::pair<double, bool>{position, playing};
                  },
                  []() { return std::optional<float>{}; },
                  [this]() { ++aboutToPlayCount; },
                  [this](const ProjectChange&) { ++changeCount; }},
              &history, nullptr, &statusBar) {
        history.setProject(&project);
    }

    Project project = smallProject();
    ProjectHistory history;
    bool hasProject = true;
    double position = 0.0;
    bool playing = false;
    int playheadPolls = 0;
    int aboutToPlayCount = 0;
    int changeCount = 0;
    QStatusBar statusBar;
    MacroController controller;
};

}  // namespace

// v0.1.48.8 - Macros, split out of MainWindow.
void MacroControllerTest::recordingAddsANamedMacroAndStopsWithItsDuration() {
    Fixture fixture;
    fixture.controller.setRecordingEnabled(true);
    QVERIFY(fixture.controller.isRecording());
    QCOMPARE(fixture.project.macros().size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(fixture.project.macros().front().name), QStringLiteral("Macro 1"));
    QVERIFY(fixture.project.macros().front().recording);

    fixture.controller.setRecordingEnabled(false);
    QVERIFY(!fixture.controller.isRecording());
    QCOMPARE(fixture.project.macros().size(), std::size_t{1});  // kept.
    QVERIFY(!fixture.project.macros().front().recording);
    QCOMPARE(fixture.changeCount, 2);
}

void MacroControllerTest::recordingNeedsAProjectAndIgnoresRepeats() {
    Fixture fixture;
    fixture.controller.setRecordingEnabled(false);  // not recording: nothing to stop.
    QCOMPARE(fixture.changeCount, 0);
    fixture.controller.setRecordingEnabled(true);
    fixture.controller.setRecordingEnabled(true);  // already recording.
    QCOMPARE(fixture.project.macros().size(), std::size_t{1});
    QCOMPARE(fixture.changeCount, 1);
    fixture.controller.setRecordingEnabled(false);

    fixture.hasProject = false;
    fixture.controller.setRecordingEnabled(true);
    QVERIFY(!fixture.controller.isRecording());
}

void MacroControllerTest::thePlayheadIsPolledWhileRecording() {
    Fixture fixture;
    fixture.controller.notePlayhead();  // not recording: not asked.
    QCOMPARE(fixture.playheadPolls, 0);
    fixture.controller.setRecordingEnabled(true);
    const int atStart = fixture.playheadPolls;
    QVERIFY(atStart >= 1);  // where the recording starts.
    QTRY_VERIFY_WITH_TIMEOUT(fixture.playheadPolls >= atStart + 2, 5000);  // the timer.
    fixture.controller.setRecordingEnabled(false);
    const int atStop = fixture.playheadPolls;
    QTest::qWait(100);
    QCOMPARE(fixture.playheadPolls, atStop);  // the timer stopped.
}

void MacroControllerTest::aRecordedMacroHasARenderer() {
    Fixture fixture;
    fixture.controller.setRecordingEnabled(true);
    fixture.controller.setRecordingEnabled(false);
    const auto id = fixture.project.macros().front().id;
    QString error;
    QVERIFY2(fixture.controller.rendererFor(id, &error) != nullptr, qPrintable(error));
}

void MacroControllerTest::aMissingMacroHasNoRendererAndDoesNotPlay() {
    Fixture fixture;
    QString error;
    QVERIFY(fixture.controller.rendererFor(sound_mind::core::MacroId{12345}, &error) == nullptr);
    QVERIFY(!error.isEmpty());
    QVERIFY(!fixture.controller.play(sound_mind::core::MacroId{12345}, &error));
    QCOMPARE(fixture.aboutToPlayCount, 0);  // Playback left alone.
}

void MacroControllerTest::anotherProjectForgetsARecordingInProgress() {
    Fixture fixture;
    fixture.controller.setRecordingEnabled(true);
    fixture.controller.projectChanged();
    QVERIFY(!fixture.controller.isRecording());
    const int polls = fixture.playheadPolls;
    QTest::qWait(100);
    QCOMPARE(fixture.playheadPolls, polls);  // the timer stopped.
}
