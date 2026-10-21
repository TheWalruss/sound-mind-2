#include "test_remaster.h"

#include <cstdint>
#include <filesystem>
#include <fstream>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QtTest/QtTest>

#include "sound_mind/core/generator.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/import_export.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/project_history.h"
#include "sound_mind/studio/remaster.h"
#include "sound_mind/studio/remaster_dialog.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::ProjectHistory;
using sound_mind::studio::RemasterRequest;

namespace {

std::filesystem::path freshFolder(const char* name) {
    const auto folder = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return folder;
}

void writeWav(const std::filesystem::path& path, std::size_t frames, int period) {
    const auto put32 = [](std::ofstream& out, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out.put(static_cast<char>((v >> (8 * i)) & 0xFF));
    };
    const auto put16 = [](std::ofstream& out, std::uint16_t v) {
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>(v >> 8));
    };
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    put32(out, static_cast<std::uint32_t>(36 + frames * 2));
    out.write("WAVEfmt ", 8);
    put32(out, 16);
    put16(out, 1);
    put16(out, 1);
    put32(out, 44100);
    put32(out, 88200);
    put16(out, 2);
    put16(out, 16);
    out.write("data", 4);
    put32(out, static_cast<std::uint32_t>(frames * 2));
    for (std::size_t i = 0; i < frames; ++i) {
        put16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>((static_cast<int>(i) % period) * 300 - 6000)));
    }
}

/// A small project with a folder and a typical history: an imported audio
/// layer painted on, a generated layer, a property change.
struct Session {
    std::filesystem::path folder;
    std::filesystem::path file;
    Project project;
    LayerId imported = 0;
    LayerId generated = 0;

    explicit Session(const char* name) : folder(freshFolder(name)), file(folder / "Live.smproj") {
        ProjectSettings settings;
        settings.canvasWidth = 16;
        settings.binCount = 128;
        settings.canvasHeight = 128;
        project = Project::createNew(settings);
        project.save(file);
        ProjectHistory history;
        history.setProject(&project);

        writeWav(folder / "a.wav", 16 * 441, 50);
        QString error;
        QCOMPARE_EQ(sound_mind::studio::importAudioSnippetsInto(project, folder / "a.wav", {0}, &error), 1);
        history.commit(project, QStringLiteral("Imported"));
        imported = project.layers()[project.layers().size() - 2].id();

        sound_mind::studio::PaintController painter(&history);
        painter.setProject(&project);
        auto tool = std::make_unique<sound_mind::core::ProceduralConfiguration>();
        painter.setToolConfiguration(std::move(tool));
        painter.beginStroke(imported, sound_mind::core::TimeFrequencyPoint{0.05, 800.0});
        painter.continueStroke(sound_mind::core::TimeFrequencyPoint{0.1, 1200.0});
        painter.endStroke();

        sound_mind::core::GeneratorConfiguration config;
        config.family = sound_mind::core::GeneratorFamily::Noise;
        config.seed = 77;
        Layer layer(0, "Noise", sound_mind::core::LayerType::Normal);
        layer.setBaseContent(sound_mind::core::generateContent(config, project.settings(),
                                                               [](sound_mind::core::MindWaveId) { return nullptr; }),
                             nlohmann::json{{"kind", "generate"}, {"configuration", config}});
        generated = project.addLayer(std::move(layer));
        project.layerById(generated)->setOpacity(0.4f);
        history.commit(project, QStringLiteral("Generated"));
        project.save(file);
    }

    ~Session() { std::filesystem::remove_all(folder); }

    std::optional<Project> remaster(const RemasterRequest& request, QString* error = nullptr) const {
        const auto input = sound_mind::studio::remasterInputFrom(project);
        if (!input) {
            return std::nullopt;
        }
        return sound_mind::studio::remasterProject(*input, request, {}, {}, error);
    }
};

RemasterRequest sameSettings(const Session& session, const char* name) {
    RemasterRequest request;
    request.settings = session.project.settings();
    request.outputPath = session.folder / name;
    return request;
}

}  // namespace

// v0.1.43.4 - the History replays to exactly what was built live.
void RemasterTest::remasteringAtTheSameSettingsReproducesTheLiveProject() {
    Session session("sound-mind-test-remaster-same");
    QString error;
    const auto result = session.remaster(sameSettings(session, "Same.smproj"), &error);
    QVERIFY2(result.has_value(), qPrintable(error));
    QCOMPARE(result->layers().size(), session.project.layers().size());
    for (const Layer& live : session.project.layers()) {
        const Layer* rebuilt = result->layerById(live.id());
        QVERIFY(rebuilt != nullptr);
        QVERIFY(nlohmann::json(*rebuilt) == nlohmann::json(live));
        QCOMPARE(rebuilt->content().has_value(), live.content().has_value());
        if (live.content()) {
            QVERIFY(rebuilt->content()->leftMagnitudeDb == live.content()->leftMagnitudeDb);
            QVERIFY(rebuilt->content()->sharedPhaseRadians == live.content()->sharedPhaseRadians);
        }
    }
    QVERIFY(std::filesystem::exists(session.folder / "Same.smproj"));
}

void RemasterTest::aFinerTimestepKeepsDurationEditsAndPlacement() {
    Session session("sound-mind-test-remaster-finer");
    RemasterRequest request = sameSettings(session, "Finer.smproj");
    request.settings.timestepMs = session.project.settings().timestepMs / 2.0;
    QString error;
    const auto result = session.remaster(request, &error);
    QVERIFY2(result.has_value(), qPrintable(error));
    QCOMPARE(result->settings().canvasWidth, session.project.settings().canvasWidth * 2);
    const Layer* imported = result->layerById(session.imported);
    QVERIFY(imported != nullptr && imported->content().has_value());
    QVERIFY(imported->content()->frameCount >= session.project.layerById(session.imported)->content()->frameCount * 2 - 2);
    QCOMPARE(result->operationLog().activeOperationsTargeting(session.imported).size(), std::size_t{1});
    QCOMPARE(result->layerById(session.generated)->opacity(), 0.4f);
}

void RemasterTest::aReplacedSourceIsUsedInsteadOfTheOriginal() {
    Session session("sound-mind-test-remaster-replace");
    writeWav(session.folder / "b.wav", 16 * 441, 13);
    RemasterRequest request = sameSettings(session, "Replaced.smproj");
    const auto sourceId =
        session.project.layerById(session.imported)->baseRecipe().at("source").get<sound_mind::core::SourceId>();
    request.replacements[sourceId] = session.folder / "b.wav";
    QString error;
    const auto result = session.remaster(request, &error);
    QVERIFY2(result.has_value(), qPrintable(error));

    const auto expected = sound_mind::studio::encodeAudioSnippets(session.project.settings(), session.folder / "b.wav",
                                                                  {0}, nullptr, nullptr, 0.0);
    QVERIFY(result->layerById(session.imported)->baseContent()->leftMagnitudeDb ==
            expected.front().baseContent()->leftMagnitudeDb);
    QVERIFY(result->layerById(session.imported)->baseRecipe().at("source").get<sound_mind::core::SourceId>() !=
            sourceId);
}

void RemasterTest::theResultHasItsOwnCompleteHistory() {
    Session session("sound-mind-test-remaster-history");
    RemasterRequest request = sameSettings(session, "Again.smproj");
    request.settings.binCount = 256;
    request.settings.canvasHeight = 256;
    QString error;
    const auto result = session.remaster(request, &error);
    QVERIFY2(result.has_value(), qPrintable(error));
    QVERIFY(result->history().startsAtCreation());
    QCOMPARE(result->history().size(), session.project.history().size());

    const Project reopened = Project::load(session.folder / "Again.smproj");
    QCOMPARE(reopened.history().size(), session.project.history().size());
    const auto input = sound_mind::studio::remasterInputFrom(reopened);
    QVERIFY(input.has_value());  // and can be remastered in turn.
    RemasterRequest back;
    back.settings = session.project.settings();
    back.outputPath = session.folder / "Back.smproj";
    QVERIFY2(sound_mind::studio::remasterProject(*input, back, {}, {}, &error).has_value(), qPrintable(error));
}

void RemasterTest::aHistoryNotStartingAtCreationCantBeRemastered() {
    Session session("sound-mind-test-remaster-incomplete");
    session.project.history().markIncomplete();
    QVERIFY(!sound_mind::studio::remasterInputFrom(session.project).has_value());
}

void RemasterTest::dialogPrefillsFromTheProjectAndNeedsANewOutputPath() {
    Session session("sound-mind-test-remaster-dialog");
    sound_mind::studio::RemasterDialog dialog(session.project, session.file);
    auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QVERIFY(!ok->isEnabled());
    dialog.setOutputPath(session.file);  // the original itself: refused.
    QVERIFY(!ok->isEnabled());
    dialog.setOutputPath(session.folder / "New.smproj");
    QVERIFY(ok->isEnabled());
    dialog.setReplacement(1, session.folder / "x.wav");

    const RemasterRequest request = dialog.request();
    QCOMPARE(request.settings.timestepMs, session.project.settings().timestepMs);
    QCOMPARE(request.settings.binCount, session.project.settings().binCount);
    QCOMPARE(request.outputPath, session.folder / "New.smproj");
    QCOMPARE(request.replacements.size(), std::size_t{1});
}
