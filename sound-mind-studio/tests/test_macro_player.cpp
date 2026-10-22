#include "test_macro_player.h"

#include <memory>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/macro_renderer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/macro_player.h"
#include "sound_mind/studio/playback_controller.h"

using sound_mind::core::AudioDeviceMode;
using sound_mind::core::Layer;
using sound_mind::core::LayerBase;
using sound_mind::core::LayerId;
using sound_mind::core::Macro;
using sound_mind::core::MacroEventKind;
using sound_mind::core::Project;
using sound_mind::studio::CanvasWidget;
using sound_mind::studio::MacroPlayer;
using sound_mind::studio::PlaybackController;

namespace {

sound_mind::core::ProjectSettings smallSettings() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.binCount = 32;
    settings.canvasHeight = 32;
    return settings;
}

/// A project with one imported layer, and a Macro that plays 3 s while the
/// import happens at 1 s. Recipes "known" remake the silent base.
struct Rig {
    Project project = Project::createNew(smallSettings());
    PlaybackController playback{nullptr, AudioDeviceMode::None};
    CanvasWidget canvas;
    MacroPlayer player{&playback, &canvas};
    Macro macro;

    explicit Rig(bool brokenRecipe = false) : broken(brokenRecipe) {
        canvas.setProject(&project);
        Layer layer(0, "Imported", sound_mind::core::LayerType::Normal);
        layer.setBaseContent(sound_mind::core::silentContentFor(project.settings()),
                             nlohmann::json{{"kind", brokenRecipe ? "unknown" : "known"}});
        project.addLayer(std::move(layer));
        project.history().commit(project, "Imported");
        macro.settings = project.settings();
        macro.historyStartsAtCreation = true;
        macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0},
                        {1.0, MacroEventKind::Entry, project.history().entry(0), 0.0}};
        macro.durationSeconds = 3.0;
    }

    std::unique_ptr<sound_mind::core::MacroRenderer> renderer() {
        sound_mind::core::MacroRenderOptions options;
        options.runRecipe = [](const nlohmann::json& recipe, LayerId, const Project& at,
                               const sound_mind::core::ReplayRenderedLookup&,
                               std::string* error) -> std::optional<LayerBase> {
            if (recipe.value("kind", "") != "known") {
                *error = "can't remake it";
                return std::nullopt;
            }
            return LayerBase{std::make_shared<const sound_mind::codec::StreamImage>(
                                 sound_mind::core::silentContentFor(at.settings())),
                             recipe, 0};
        };
        return std::make_unique<sound_mind::core::MacroRenderer>(project.settings(),
                                                                 std::vector<sound_mind::core::HistoryEntry>{}, macro,
                                                                 std::move(options));
    }

    bool waitUntil(const std::function<bool()>& condition) {
        for (int i = 0; i < 300 && !condition(); ++i) {
            player.tick();
            QTest::qWait(10);
        }
        return condition();
    }

    bool broken;
};

}  // namespace

// v0.1.44.3 - Play Macro renders ahead, then plays on the canvas.
void MacroPlayerTest::startsPlayingOnceBufferedAndShowsTheRenderedCanvas() {
    Rig rig;
    QSignalSpy active(&rig.player, &MacroPlayer::activeChanged);
    rig.player.start(rig.renderer());
    QVERIFY(rig.player.isActive());
    QCOMPARE(active.count(), 1);
    QVERIFY(rig.waitUntil([&]() { return rig.player.hasStartedPlaying(); }));
    QVERIFY(rig.player.bufferedSeconds() >= MacroPlayer::kStartBufferSeconds);
    QVERIFY(rig.canvas.hasFrameOverride());
    QVERIFY(rig.playback.isPlaying());
    QCOMPARE(rig.playback.totalSeconds(), 3.0);
}

void MacroPlayerTest::stopClearsTheCanvasAndSignalsFinished() {
    Rig rig;
    rig.player.start(rig.renderer());
    QVERIFY(rig.waitUntil([&]() { return rig.player.hasStartedPlaying(); }));
    QSignalSpy finished(&rig.player, &MacroPlayer::finished);
    rig.player.stop();
    QVERIFY(!rig.player.isActive());
    QVERIFY(!rig.canvas.hasFrameOverride());
    QVERIFY(!rig.playback.isPlaying());
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.takeFirst().at(0).toString().isEmpty());
}

void MacroPlayerTest::reachingTheEndFinishes() {
    Rig rig;
    rig.player.start(rig.renderer());
    QVERIFY(rig.waitUntil([&]() { return rig.player.bufferedSeconds() >= 3.0 - 1e-6; }));
    QSignalSpy finished(&rig.player, &MacroPlayer::finished);
    rig.playback.seek(3.0);  // no audio device here: jump to the end.
    QVERIFY(rig.waitUntil([&]() { return !rig.player.isActive(); }));
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.takeFirst().at(0).toString().isEmpty());
}

void MacroPlayerTest::aRenderingFailureIsReported() {
    Rig rig(/*brokenRecipe=*/true);
    QSignalSpy finished(&rig.player, &MacroPlayer::finished);
    rig.player.start(rig.renderer());
    QVERIFY(rig.waitUntil([&]() { return !rig.player.isActive(); }));
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.takeFirst().at(0).toString().contains(QStringLiteral("can't remake it")));
    QVERIFY(!rig.canvas.hasFrameOverride());
}

void MacroPlayerTest::theProjectIsNeverTouched() {
    Rig rig;
    const nlohmann::json before = rig.project;
    rig.player.start(rig.renderer());
    QVERIFY(rig.waitUntil([&]() { return rig.player.bufferedSeconds() >= 3.0 - 1e-6; }));
    rig.player.stop();
    QVERIFY(nlohmann::json(rig.project) == before);
}
