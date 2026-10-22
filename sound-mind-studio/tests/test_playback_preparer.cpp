#include "test_playback_preparer.h"

#include <algorithm>
#include <memory>

#include <QPushButton>
#include <QStatusBar>
#include <QtTest/QtTest>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/playback_preparer.h"

using sound_mind::core::Project;
using sound_mind::studio::PlaybackController;
using sound_mind::studio::PlaybackPreparer;
using sound_mind::studio::PlaybackScope;

namespace {

/// A project `canvasWidth` columns long whose Background layer has sound.
Project projectWithContent(std::uint32_t canvasWidth) {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = canvasWidth;
    settings.canvasHeight = 64;
    settings.binCount = 64;
    Project project = Project::createNew(settings);
    auto content = sound_mind::core::silentContentFor(project.settings());
    std::fill(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(), -30.0f);
    std::fill(content.rightMagnitudeDb.begin(), content.rightMagnitudeDb.end(), -30.0f);
    project.layers().front().setBaseContent(content);
    return project;
}

/// A preparer over `project` with a device-less playback.
struct Fixture {
    explicit Fixture(Project* project)
        : preparer(PlaybackPreparer::Context{[project]() -> const Project* { return project; },
                                             [this]() { return playback.positionSeconds(); },
                                             [this](double) { ++loudnessReports; }, [this]() { ++starts; },
                                             [this](const QString& title, const QString&) { errors.append(title); }},
                   &playback, &statusBar) {}

    QStatusBar statusBar;
    PlaybackController playback{nullptr, sound_mind::core::AudioDeviceMode::None};
    int loudnessReports = 0;
    int starts = 0;
    QStringList errors;
    PlaybackPreparer preparer;
};

}  // namespace

// v0.1.48.4 - playback preparation, split out of MainWindow.
void PlaybackPreparerTest::preparingLoadsAndPlaysTheWholeTrack() {
    Project project = projectWithContent(100);
    Fixture fixture(&project);
    fixture.preparer.prepareAndPlay();
    QVERIFY(fixture.preparer.isCompositing());
    QVERIFY(!fixture.playback.isLoaded());  // nothing on the UI thread yet.
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.preparer.isCompositing(), 60000);
    QVERIFY(fixture.playback.isLoaded());
    QVERIFY(fixture.playback.isPlaying());
    QCOMPARE(fixture.starts, 1);
    QCOMPARE(fixture.loudnessReports, 1);
    fixture.playback.stop();
}

void PlaybackPreparerTest::withNothingToPlayNothingStarts() {
    Project project = Project::createNew(sound_mind::core::ProjectSettings{});
    Fixture fixture(&project);
    fixture.preparer.prepareAndPlay();
    QVERIFY(!fixture.preparer.isCompositing());
    QVERIFY(!fixture.playback.isLoaded());
}

void PlaybackPreparerTest::cancellingPreparationLeavesNothingLoaded() {
    Project project = projectWithContent(4000);
    Fixture fixture(&project);
    fixture.preparer.prepareAndPlay();
    fixture.statusBar.findChild<QPushButton*>(QStringLiteral("compositeCancelButton"))->click();
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.preparer.isCompositing(), 60000);
    QVERIFY(!fixture.playback.isLoaded());
    QCOMPARE(fixture.starts, 0);
    QVERIFY(fixture.statusBar.currentMessage().contains(QStringLiteral("cancelled")));
}

void PlaybackPreparerTest::deltaPlaysTheEditedRegionEvenFromAStop() {
    Project project = projectWithContent(200);  // 2 s at 10 ms per column.
    const auto layer = project.layers().front().id();
    sound_mind::core::Gradient gradient;
    auto& log = project.operationLog();
    log.append(std::make_unique<sound_mind::core::FillOperation>(
        log.reserveId(), layer, sound_mind::core::TimeFrequencyRect{1.5, 1.6, 100.0, 1000.0}, gradient));
    Fixture fixture(&project);
    fixture.preparer.setScope(PlaybackScope::Delta);

    fixture.preparer.contentChanged(layer);
    QVERIFY(fixture.preparer.isPreparingEdit());
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.preparer.isPreparingEdit(), 60000);
    QVERIFY(fixture.playback.isPlaying());
    QVERIFY(fixture.playback.positionSeconds() >= 1.49);  // at the edit, not the start.
    fixture.playback.stop();
}

void PlaybackPreparerTest::trackScopeWithoutRepeatIgnoresEdits() {
    Project project = projectWithContent(100);
    Fixture fixture(&project);
    fixture.preparer.contentChanged(project.layers().front().id());
    QVERIFY(!fixture.preparer.isPreparingEdit());
    QVERIFY(!fixture.playback.isLoaded());
}
