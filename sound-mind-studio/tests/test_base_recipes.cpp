#include "test_base_recipes.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include <QImage>
#include <QtTest/QtTest>

#include "sound_mind/core/generator.h"
#include "sound_mind/core/pooling.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/base_recipes.h"
#include "sound_mind/studio/import_export.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::runBaseRecipe;

namespace {

std::filesystem::path freshFolder(const char* name) {
    const auto folder = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return folder;
}

/// A project with a folder, so imports can keep their sources.
Project projectIn(const std::filesystem::path& folder) {
    ProjectSettings settings;
    settings.canvasWidth = 8;  // one snippet = 8 * 441 samples.
    Project project = Project::createNew(settings);
    project.setFolderForProjectFile(folder / "Recipes.smproj");
    return project;
}

void writeWav(const std::filesystem::path& path, std::size_t frames) {
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
        put16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>((i % 50) * 400 - 10000)));
    }
}

sound_mind::studio::SourcePathLookup sourcesOf(const Project& project) {
    return [&project](sound_mind::core::SourceId id) { return project.sourcePath(id); };
}

sound_mind::studio::RenderedContentLookup renderedOf(const Project& project) {
    return [&project](LayerId id) -> const sound_mind::codec::StreamImage* {
        const Layer* layer = project.layerById(id);
        return layer != nullptr && layer->content() ? &*layer->content() : nullptr;
    };
}

}  // namespace

// v0.1.43.2 - recipes, run against the project's own sources.
void BaseRecipesTest::importedAudioKeepsItsFileAndReplaysExactly() {
    const auto folder = freshFolder("sound-mind-test-recipe-audio");
    Project project = projectIn(folder);
    writeWav(folder / "tone.wav", 8 * 441 * 2);
    QString error;
    QCOMPARE(sound_mind::studio::importAudioSnippetsInto(project, folder / "tone.wav", {1}, &error), 1);
    const Layer& imported = project.layers()[project.layers().size() - 2];  // below the Equalizer.
    const auto& recipe = imported.baseRecipe();
    QCOMPARE(QString::fromStdString(recipe.at("kind").get<std::string>()), QStringLiteral("importAudio"));
    QCOMPARE(recipe.at("snippetIndex").get<int>(), 1);
    QVERIFY(recipe.contains("source"));
    QVERIFY(std::filesystem::exists(*project.sourcePath(recipe.at("source").get<sound_mind::core::SourceId>())));

    const auto base = runBaseRecipe(recipe, imported.id(), project, sourcesOf(project), renderedOf(project), &error);
    QVERIFY2(base.has_value(), qPrintable(error));
    QVERIFY(base->stream->leftMagnitudeDb == imported.baseContent()->leftMagnitudeDb);
    QVERIFY(base->stream->sharedPhaseRadians == imported.baseContent()->sharedPhaseRadians);
    std::filesystem::remove_all(folder);
}

void BaseRecipesTest::importedImageKeepsItsFileAndReplaysExactly() {
    const auto folder = freshFolder("sound-mind-test-recipe-image");
    Project project = projectIn(folder);
    QImage image(30, 20, QImage::Format_RGB32);
    image.fill(QColor(200, 40, 40));
    QVERIFY(image.save(QString::fromStdWString((folder / "red.png").wstring())));
    QString error;
    LayerId id = 0;
    QVERIFY(sound_mind::studio::importImageFileInto(project, folder / "red.png",
                                                   sound_mind::studio::ImageScalePickerDialog::Mode::RescaleToFitProject,
                                                   &error, &id));
    const Layer& imported = *project.layerById(id);
    QVERIFY(imported.baseRecipe().contains("source"));
    const auto base =
        runBaseRecipe(imported.baseRecipe(), id, project, sourcesOf(project), renderedOf(project), &error);
    QVERIFY2(base.has_value(), qPrintable(error));
    QVERIFY(base->stream->leftMagnitudeDb == imported.baseContent()->leftMagnitudeDb);
    std::filesystem::remove_all(folder);
}

void BaseRecipesTest::generatedLayerReplaysFromItsSeed() {
    const auto folder = freshFolder("sound-mind-test-recipe-generate");
    Project project = projectIn(folder);
    sound_mind::core::GeneratorConfiguration config;
    config.family = sound_mind::core::GeneratorFamily::Noise;
    config.seed = 1234;
    const auto made = sound_mind::core::generateContent(config, project.settings(),
                                                        [](sound_mind::core::MindWaveId) { return nullptr; });
    const nlohmann::json recipe{{"kind", "generate"}, {"configuration", config}};
    const auto base = runBaseRecipe(recipe, 0, project, sourcesOf(project), renderedOf(project));
    QVERIFY(base.has_value());
    QVERIFY(base->stream->leftMagnitudeDb == made.leftMagnitudeDb);
    std::filesystem::remove_all(folder);
}

void BaseRecipesTest::recordingReplaysFromItsFlacSource() {
    const auto folder = freshFolder("sound-mind-test-recipe-record");
    Project project = projectIn(folder);
    sound_mind::codec::AudioBuffer captured;
    captured.sampleRateHz = 44100;
    for (int i = 0; i < 8 * 441; ++i) {
        captured.left.push_back(0.25f * static_cast<float>((i % 40) - 20) / 20.0f);
        captured.right.push_back(captured.left.back());
    }
    const auto id = project.addSource(
        sound_mind::core::storeSourceAudio(*project.sourcesFolder(), captured, "recording", "Take"));
    const nlohmann::json recipe{{"kind", "record"}, {"source", id}};
    const auto config = sound_mind::core::streamCodecConfigFor(project.settings());
    const auto original = sound_mind::codec::encode(captured, config);

    const auto base = runBaseRecipe(recipe, 0, project, sourcesOf(project), renderedOf(project));
    QVERIFY(base.has_value());
    QCOMPARE(base->stream->frameCount, original.frameCount);
    // Through a 24-bit FLAC: the same sound, within quantization.
    for (std::size_t i = 0; i < original.leftMagnitudeDb.size(); i += 97) {
        QVERIFY(std::abs(base->stream->leftMagnitudeDb[i] - original.leftMagnitudeDb[i]) < 0.5f);
    }
    std::filesystem::remove_all(folder);
}

void BaseRecipesTest::emptyDuplicatePoolAndPhaseCleanupStartFromTheRenderedContent() {
    const auto folder = freshFolder("sound-mind-test-recipe-derived");
    Project project = projectIn(folder);
    Layer layer(0, "Painted", LayerType::Normal);
    auto content = sound_mind::core::silentContentFor(project.settings());
    for (std::size_t i = 0; i < content.leftMagnitudeDb.size(); i += 7) {
        content.leftMagnitudeDb[i] = -20.0f;
        content.rightMagnitudeDb[i] = -20.0f;
    }
    layer.setBaseContent(content);
    const LayerId id = project.addLayer(std::move(layer));

    const auto empty = runBaseRecipe({{"kind", "empty"}}, id, project, sourcesOf(project), renderedOf(project));
    QVERIFY(empty.has_value());
    QCOMPARE(empty->stream->frameCount, content.frameCount);

    const auto duplicate = runBaseRecipe({{"kind", "duplicate"}, {"sourceLayer", id}}, 99, project,
                                         sourcesOf(project), renderedOf(project));
    QVERIFY(duplicate.has_value());
    QVERIFY(duplicate->stream->leftMagnitudeDb == content.leftMagnitudeDb);

    const auto pooled = runBaseRecipe({{"kind", "pool"}}, id, project, sourcesOf(project), renderedOf(project));
    QVERIFY(pooled.has_value());
    QVERIFY(pooled->pool != nullptr);
    const auto expected = sound_mind::core::computePooledContent(content);
    QVERIFY(pooled->stream->leftMagnitudeDb == expected.streamContent.leftMagnitudeDb);

    const auto cleaned =
        runBaseRecipe({{"kind", "phaseCleanup"}}, id, project, sourcesOf(project), renderedOf(project));
    QVERIFY(cleaned.has_value());
    QCOMPARE(cleaned->stream->frameCount, content.frameCount);
    std::filesystem::remove_all(folder);
}

void BaseRecipesTest::aMissingSourceOrUnknownRecipeFailsWithAReason() {
    const auto folder = freshFolder("sound-mind-test-recipe-missing");
    Project project = projectIn(folder);
    QString error;
    QVERIFY(!runBaseRecipe({{"kind", "importAudio"}, {"source", 42}}, 0, project, sourcesOf(project),
                           renderedOf(project), &error)
                 .has_value());
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!runBaseRecipe({{"kind", "somethingNew"}}, 0, project, sourcesOf(project), renderedOf(project), &error)
                 .has_value());
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!runBaseRecipe(nullptr, 0, project, sourcesOf(project), renderedOf(project), &error).has_value());
    QVERIFY(!error.isEmpty());
    std::filesystem::remove_all(folder);
}
