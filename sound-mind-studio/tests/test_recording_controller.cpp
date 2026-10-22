#include "test_recording_controller.h"

#include <cmath>
#include <optional>

#include <QComboBox>
#include <QLineEdit>
#include <QStatusBar>
#include <QtTest/QtTest>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/record_engine.h"
#include "sound_mind/studio/record_panel.h"
#include "sound_mind/studio/recording_controller.h"

using sound_mind::core::Project;
using sound_mind::studio::ProjectChange;
using sound_mind::studio::RecordingController;
using sound_mind::studio::RecordPanel;

namespace {

sound_mind::codec::AudioBuffer recordedTone(float amplitude) {
    sound_mind::codec::AudioBuffer audio;
    audio.sampleRateHz = 44100;
    audio.left.resize(8820);
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        audio.left[i] = amplitude * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f);
    }
    audio.right = audio.left;
    return audio;
}

/// The layer a take was added as: the topmost one under any Equalizer.
const sound_mind::core::Layer& topmostTake(const Project& project) {
    const auto& layers = project.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (it->type() != sound_mind::core::LayerType::Equalizer) {
            return *it;
        }
    }
    return layers.back();
}

/// A controller over its own project, panel and device-less engine,
/// counting what it asks of its window.
struct Fixture {
    Fixture()
        : project(Project::createNew(sound_mind::core::ProjectSettings{})),
          engine(sound_mind::codec::StreamCodecConfig{}.sampleRateHz, sound_mind::core::AudioDeviceMode::None),
          controller(
              RecordingController::Context{
                  [this]() -> Project* { return hasProject ? &project : nullptr; },
                  [this]() { return mayStart; },
                  [this]() { ++aboutToStartCount; },
                  [this]() { ++recordingChangedCount; },
                  [this](std::optional<sound_mind::codec::StreamImage>) {},
                  [this](std::optional<double> fraction) { playhead = fraction; },
                  [this](const QString& description) { history.append(description); },
                  [this](const ProjectChange&) {
                      ++changeCount;
                      controller.refreshTargets();
                  },
                  [this](const QString&, const QString& message) { errors.append(message); }},
              engine, &panel, &statusBar) {}

    Project project;
    bool hasProject = true;
    bool mayStart = true;
    int aboutToStartCount = 0;
    int recordingChangedCount = 0;
    int changeCount = 0;
    std::optional<double> playhead;
    QStringList history;
    QStringList errors;
    RecordPanel panel;
    QStatusBar statusBar;
    sound_mind::core::RecordEngine engine;
    RecordingController controller;
};

}  // namespace

// v0.1.48.6 - Record, split out of MainWindow.
void RecordingControllerTest::commitAddsANamedRecordedLayer() {
    Fixture fixture;
    fixture.panel.findChild<QLineEdit*>(QStringLiteral("recordNameEdit"))->setText(QStringLiteral("Vocals"));
    const std::size_t before = fixture.project.layers().size();

    QVERIFY(fixture.controller.commit(recordedTone(0.5f)));

    QCOMPARE(fixture.project.layers().size(), before + 1);
    const sound_mind::core::Layer& added = topmostTake(fixture.project);
    QCOMPARE(QString::fromStdString(added.name()), QStringLiteral("Vocals"));
    QVERIFY(added.recorded());
    QVERIFY(added.content().has_value());
    QCOMPARE(fixture.history.size(), 1);
    QCOMPARE(fixture.changeCount, 1);
    // Offered as a target next time: "New recording layer" + Vocals.
    QCOMPARE(fixture.panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"))->count(), 2);
}

void RecordingControllerTest::commitOverTheChosenTargetReplacesItsContent() {
    Fixture fixture;
    QVERIFY(fixture.controller.commit(recordedTone(0.5f)));
    const sound_mind::core::LayerId takeId = topmostTake(fixture.project).id();
    const auto firstContent = *fixture.project.layerById(takeId)->content();
    const std::size_t layerCount = fixture.project.layers().size();
    auto* combo = fixture.panel.findChild<QComboBox*>(QStringLiteral("recordTargetCombo"));
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<qulonglong>(takeId))));

    QVERIFY(fixture.controller.commit(recordedTone(0.05f)));

    QCOMPARE(fixture.project.layers().size(), layerCount);  // replaced, not added.
    QVERIFY(fixture.project.layerById(takeId)->content()->leftMagnitudeDb != firstContent.leftMagnitudeDb);
    QCOMPARE(fixture.history.size(), 2);
}

void RecordingControllerTest::commitAsyncAddsTheLayerWhenTheSaveIsDone() {
    Fixture fixture;
    const std::size_t before = fixture.project.layers().size();

    fixture.controller.commitAsync(recordedTone(0.5f));
    QVERIFY(fixture.controller.isSaving());
    QCOMPARE(fixture.project.layers().size(), before);  // not on the UI thread.

    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isSaving(), 60000);
    QCOMPARE(fixture.project.layers().size(), before + 1);
    QVERIFY(topmostTake(fixture.project).recorded());

    // flushSave() applies a pending save at once.
    fixture.controller.commitAsync(recordedTone(0.5f));
    fixture.controller.flushSave();
    QVERIFY(!fixture.controller.isSaving());
    QCOMPARE(fixture.project.layers().size(), before + 2);
    QVERIFY(fixture.errors.isEmpty());
}

void RecordingControllerTest::nothingCapturedChangesNothing() {
    Fixture fixture;
    const std::size_t before = fixture.project.layers().size();
    QVERIFY(!fixture.controller.commit(sound_mind::codec::AudioBuffer{}));
    fixture.controller.commitAsync(sound_mind::codec::AudioBuffer{});
    QVERIFY(!fixture.controller.isSaving());
    fixture.hasProject = false;
    QVERIFY(!fixture.controller.commit(recordedTone(0.5f)));
    QCOMPARE(fixture.project.layers().size(), before);
    QCOMPARE(fixture.changeCount, 0);
}

void RecordingControllerTest::toggleStartsAndStopsWithoutAddingALayerWhenNothingWasCaptured() {
    Fixture fixture;
    const std::size_t before = fixture.project.layers().size();

    fixture.controller.toggle();
    QVERIFY(fixture.controller.isRecording());
    QCOMPARE(fixture.aboutToStartCount, 1);  // Playback stopped first.
    QCOMPARE(fixture.recordingChangedCount, 1);

    fixture.controller.toggle();
    QVERIFY(!fixture.controller.isRecording());
    QCOMPARE(fixture.recordingChangedCount, 2);
    QVERIFY(!fixture.playhead.has_value());  // the position bar is cleared.
    QVERIFY(!fixture.controller.isSaving());  // no device: nothing captured.
    QCOMPARE(fixture.project.layers().size(), before);
}

void RecordingControllerTest::toggleRefusesWhileNotAllowedOrWithNoProject() {
    Fixture fixture;
    fixture.mayStart = false;  // e.g. Loop Mode is running.
    fixture.controller.toggle();
    QVERIFY(!fixture.controller.isRecording());
    fixture.mayStart = true;
    fixture.hasProject = false;
    fixture.controller.toggle();
    QVERIFY(!fixture.controller.isRecording());
    QCOMPARE(fixture.aboutToStartCount, 0);
}
