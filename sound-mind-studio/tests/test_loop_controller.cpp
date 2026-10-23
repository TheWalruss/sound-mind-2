#include "test_loop_controller.h"

#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include <QCheckBox>
#include <QStatusBar>
#include <QtTest/QtTest>

#include "sound_mind/core/loop_engine.h"
#include "sound_mind/core/playback_engine.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/loop_controller.h"
#include "sound_mind/studio/loop_panel.h"

using sound_mind::core::LoopEngine;
using sound_mind::core::Project;
using sound_mind::studio::LoopController;
using sound_mind::studio::LoopPanel;
using sound_mind::studio::ProjectChange;

namespace {

/// A short project, so a loop is a fraction of a second.
Project shortProject() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 16;
    return Project::createNew(settings);
}

/// A device-less engine for `project`, as MainWindow::setProject() makes one.
std::unique_ptr<LoopEngine> engineFor(const Project& project) {
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    return std::make_unique<LoopEngine>(config, static_cast<std::size_t>(project.settings().canvasWidth) *
                                                    config.hopLength,
                                        sound_mind::core::AudioDeviceMode::None);
}

std::size_t loopInputLayerCount(const Project& project) {
    std::size_t count = 0;
    for (const sound_mind::core::Layer& layer : project.layers()) {
        count += layer.name() == "Loop Input" ? 1 : 0;
    }
    return count;
}

/// A controller over its own project and panel, counting what it asks of
/// its window.
struct Fixture {
    Fixture()
        : project(shortProject()),
          controller(
              LoopController::Context{
                  [this]() -> Project* { return &project; },
                  [this]() { return mayStart; },
                  [this]() { ++aboutToStartCount; },
                  [this]() { ++runningChangedCount; },
                  [](std::optional<sound_mind::codec::StreamImage>) {},
                  [this](std::optional<double> fraction) { playhead = fraction; },
                  [this](const QString& description) { history.append(description); },
                  [this](const ProjectChange&) { ++changeCount; },
                  [this](sound_mind::core::LayerId) { ++takesApplied; }},
              &panel, &statusBar) {
        controller.setEngine(engineFor(project));
    }

    ~Fixture() {
        if (controller.isRunning()) {
            controller.toggle();
        }
    }

    Project project;
    bool mayStart = true;
    int aboutToStartCount = 0;
    int runningChangedCount = 0;
    int changeCount = 0;
    int takesApplied = 0;
    std::optional<double> playhead;
    QStringList history;
    LoopPanel panel;
    QStatusBar statusBar;
    LoopController controller;
};

}  // namespace

// v0.1.48.7 - Loop Mode, split out of MainWindow.
void LoopControllerTest::toggleAddsALoopInputLayerAndStartsOnceTheBackingIsReady() {
    Fixture fixture;
    fixture.controller.toggle();

    QVERIFY(fixture.controller.isRunning());  // preparing counts as running.
    QCOMPARE(fixture.aboutToStartCount, 1);   // Playback stopped first.
    QCOMPARE(fixture.runningChangedCount, 1);
    QCOMPARE(loopInputLayerCount(fixture.project), std::size_t{1});
    QCOMPARE(fixture.history.size(), 1);
    QCOMPARE(fixture.changeCount, 1);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.engine()->isRunning(), 60000);
    QVERIFY(fixture.playhead.has_value());  // the position bar follows the loop.
}

void LoopControllerTest::toggleStopsAndARestartReusesTheLoopInputLayer() {
    Fixture fixture;
    fixture.controller.toggle();
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.engine()->isRunning(), 60000);

    fixture.controller.toggle();
    QVERIFY(!fixture.controller.isRunning());
    QVERIFY(!fixture.controller.engine()->isRunning());
    QVERIFY(!fixture.playhead.has_value());
    QCOMPARE(fixture.runningChangedCount, 2);

    fixture.controller.toggle();  // again: the same layer, no new History entry.
    QCOMPARE(loopInputLayerCount(fixture.project), std::size_t{1});
    QCOMPARE(fixture.history.size(), 1);

    fixture.controller.toggle();  // stopping while still preparing works too.
    QVERIFY(!fixture.controller.isRunning());
}

void LoopControllerTest::toggleRefusesWithoutAnEngineOrWhileNotAllowed() {
    Fixture fixture;
    fixture.mayStart = false;  // e.g. Record is running.
    fixture.controller.toggle();
    QVERIFY(!fixture.controller.isRunning());
    QCOMPARE(fixture.aboutToStartCount, 0);

    fixture.mayStart = true;
    fixture.controller.setEngine(nullptr);  // no project opened yet.
    fixture.controller.toggle();
    QVERIFY(!fixture.controller.isRunning());
    QCOMPARE(loopInputLayerCount(fixture.project), std::size_t{0});
}

void LoopControllerTest::aFinishedTakeBecomesTheLayersContent() {
    Fixture fixture;
    fixture.controller.toggle();
    LoopEngine& engine = *fixture.controller.engine();
    QTRY_VERIFY_WITH_TIMEOUT(engine.isRunning(), 60000);

    // One whole pass of input, as the audio callback would deliver it.
    const std::size_t length = engine.loopLengthSamples();
    std::vector<float> inLeft(length);
    for (std::size_t i = 0; i < length; ++i) {
        inLeft[i] = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f);
    }
    std::vector<float> inRight = inLeft;
    std::vector<float> outLeft(length);
    std::vector<float> outRight(length);
    const float* inputs[2] = {inLeft.data(), inRight.data()};
    float* outputs[2] = {outLeft.data(), outRight.data()};
    engine.processBlock(inputs, 2, outputs, 2, static_cast<int>(length));
    engine.processPendingAudio();  // the worker's encode.
    QCOMPARE(engine.takesCompleted(), std::uint64_t{1});

    QTRY_COMPARE_WITH_TIMEOUT(fixture.takesApplied, 1, 5000);
    const sound_mind::core::Layer* layer = nullptr;
    for (const sound_mind::core::Layer& candidate : fixture.project.layers()) {
        if (candidate.name() == "Loop Input") {
            layer = &candidate;
        }
    }
    QVERIFY(layer != nullptr);
    QCOMPARE(QString::fromStdString(layer->baseRecipe().value("kind", std::string{})), QStringLiteral("loopTake"));
    QVERIFY(fixture.history.contains(QStringLiteral("Loop take 1")));
}

void LoopControllerTest::anEditWhileRunningPreparesTheBackingAgain() {
    Fixture fixture;
    fixture.controller.projectEdited();  // not running: nothing to prepare.
    QVERIFY(!fixture.controller.isPreparingBacking());

    fixture.controller.toggle();
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.engine()->isRunning() && !fixture.controller.isPreparingBacking(),
                             60000);
    fixture.controller.projectEdited();
    QVERIFY(fixture.controller.isPreparingBacking());
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isPreparingBacking(), 60000);
    QVERIFY(fixture.controller.engine()->isRunning());  // never interrupted.
}

void LoopControllerTest::keepLoopingFreezesTheEngineAndSyncsThePanel() {
    Fixture fixture;
    fixture.controller.setKeepLooping(true);
    QVERIFY(fixture.controller.keepLooping());
    QVERIFY(fixture.controller.engine()->frozen());
    auto* checkBox = fixture.panel.findChild<QCheckBox*>(QStringLiteral("keepLoopingCheckBox"));
    QVERIFY(checkBox != nullptr);
    QVERIFY(checkBox->isChecked());

    fixture.controller.setKeepLooping(false);
    QVERIFY(!fixture.controller.keepLooping());
    QVERIFY(!checkBox->isChecked());
}

void LoopControllerTest::aNewEngineAbandonsTheSession() {
    Fixture fixture;
    fixture.controller.toggle();
    QVERIFY(fixture.controller.isRunning());
    fixture.controller.setEngine(engineFor(fixture.project));  // another project opened.
    QVERIFY(!fixture.controller.isRunning());
    QVERIFY(!fixture.controller.engine()->isRunning());
}
