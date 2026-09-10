#include "test_import_export.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include <QColor>
#include <QImage>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/import_export.h"

using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::AudioSnippetPickerDialog;
using sound_mind::studio::ImageScalePickerDialog;

namespace {

void appendUint32(std::vector<char>& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

void appendUint16(std::vector<char>& bytes, std::uint16_t value) {
    for (int i = 0; i < 2; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

/// @brief Writes a valid 16-bit PCM mono WAV file with exactly
/// `frameCount` samples at 44100 Hz - a local copy of
/// test_main_window.cpp's own helper (a different test binary/translation
/// unit - QTest classes don't share fixtures across files).
void writeTestWavFileWithFrameCount(const std::filesystem::path& path, std::size_t frameCount) {
    const std::uint32_t dataSize = static_cast<std::uint32_t>(frameCount * sizeof(std::int16_t));

    std::vector<char> bytes;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    appendUint32(bytes, 36 + dataSize);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});
    bytes.insert(bytes.end(), {'f', 'm', 't', ' '});
    appendUint32(bytes, 16);
    appendUint16(bytes, 1);  // PCM
    appendUint16(bytes, 1);  // mono
    appendUint32(bytes, 44100);
    appendUint32(bytes, 44100 * 2);
    appendUint16(bytes, 2);
    appendUint16(bytes, 16);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendUint32(bytes, dataSize);
    for (std::size_t i = 0; i < frameCount; ++i) {
        appendUint16(bytes, static_cast<std::uint16_t>((i % 2 == 0) ? 1000 : -1000));
    }

    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/// @brief A small canvasWidth - fast snippet-splitting math, matching
/// test_main_window.cpp's own smallCanvasProjectSettings(): loop length =
/// 8 * 441 = 3528 samples at the default 44100 Hz/10 ms timestep.
ProjectSettings smallCanvasProjectSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 8;
    return settings;
}

/// @brief A 30x20 red PNG - matching test_main_window.cpp's own
/// writeImageScalingTestImage(), paired with imageScalingTestProjectSettings().
void writeImageScalingTestImage(const std::filesystem::path& path) {
    QImage image(30, 20, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY2(image.save(QString::fromStdString(path.string())), "failed to write the test PNG");
}

ProjectSettings imageScalingTestProjectSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    return settings;
}

}  // namespace

void ImportExportTest::audioSnippetsForFileReturnsOneSnippetForAudioNoLongerThanTheProject() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-ie-snippets-one.wav";
    writeTestWavFileWithFrameCount(path, 100);  // far shorter than one loop length (3528).
    Project project = Project::createNew(smallCanvasProjectSettings());

    const auto snippets = sound_mind::studio::audioSnippetsForFile(project, path);
    std::filesystem::remove(path);

    QCOMPARE(snippets.size(), static_cast<std::size_t>(1));
}

void ImportExportTest::audioSnippetsForFileSplitsLongerAudioIntoProjectLengthSegments() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-ie-snippets-split.wav";
    writeTestWavFileWithFrameCount(path, 3528 * 3);  // exactly 3 whole snippets.
    Project project = Project::createNew(smallCanvasProjectSettings());

    const auto snippets = sound_mind::studio::audioSnippetsForFile(project, path);
    std::filesystem::remove(path);

    QCOMPARE(snippets.size(), static_cast<std::size_t>(3));
}

void ImportExportTest::audioSnippetsForFileFailsGracefullyForAnUnreadableFile() {
    Project project = Project::createNew(smallCanvasProjectSettings());
    QString errorMessage;

    const auto snippets = sound_mind::studio::audioSnippetsForFile(
        project, std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.wav", &errorMessage);

    QVERIFY(snippets.empty());
    QVERIFY(!errorMessage.isEmpty());
}

void ImportExportTest::importAudioSnippetsIntoImportsOnlyTheRequestedSubset() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-ie-import-subset.wav";
    writeTestWavFileWithFrameCount(path, 3528 * 4);  // 4 whole snippets: 0, 1, 2, 3.
    Project project = Project::createNew(smallCanvasProjectSettings());
    const std::size_t layerCountBefore = project.layers().size();

    const int importedCount = sound_mind::studio::importAudioSnippetsInto(project, path, {0, 2});
    std::filesystem::remove(path);

    QCOMPARE(importedCount, 2);
    QCOMPARE(project.layers().size(), layerCountBefore + 2);
}

void ImportExportTest::importAudioSnippetsIntoReturnsZeroWhenNothingWasImported() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-ie-import-none.wav";
    writeTestWavFileWithFrameCount(path, 100);
    Project project = Project::createNew(smallCanvasProjectSettings());

    QString errorMessage;
    const int importedCount = sound_mind::studio::importAudioSnippetsInto(project, path, {}, &errorMessage);
    std::filesystem::remove(path);

    QCOMPARE(importedCount, 0);
    QVERIFY(!errorMessage.isEmpty());
}

void ImportExportTest::importImageFileIntoAddsANewLayer() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-test-ie-image-add.png";
    writeImageScalingTestImage(path);
    Project project = Project::createNew(imageScalingTestProjectSettings());
    const std::size_t layerCountBefore = project.layers().size();

    const bool ok =
        sound_mind::studio::importImageFileInto(project, path, ImageScalePickerDialog::Mode::RescaleToFitProject);
    std::filesystem::remove(path);

    QVERIFY(ok);
    QCOMPARE(project.layers().size(), layerCountBefore + 1);
    const auto& content = *project.layers().back().content();
    QCOMPARE(content.frameCount, static_cast<std::uint32_t>(100));
    QCOMPARE(content.config.binCount, static_cast<std::uint32_t>(50));
}

void ImportExportTest::importImageFileIntoFailsForAnUnreadableFile() {
    Project project = Project::createNew(imageScalingTestProjectSettings());
    QString errorMessage;

    const bool ok = sound_mind::studio::importImageFileInto(
        project, std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.png",
        ImageScalePickerDialog::Mode::RescaleToFitProject, &errorMessage);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
}

void ImportExportTest::importImageFilesIntoImportsEachFileIndependentlyWhenNotSequential() {
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-ie-images-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-ie-images-b.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    Project project = Project::createNew(imageScalingTestProjectSettings());
    const std::size_t layerCountBefore = project.layers().size();

    const int importedCount = sound_mind::studio::importImageFilesInto(
        project, {pathA, pathB}, ImageScalePickerDialog::Mode::KeepNativeResolution, /*importAsSequence=*/false);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);

    QCOMPARE(importedCount, 2);
    QCOMPARE(project.layers().size(), layerCountBefore + 2);
    QCOMPARE(project.layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(project.layers()[layerCountBefore + 1].translationColumns(), static_cast<std::int64_t>(0));
}

void ImportExportTest::importImageFilesIntoAppliesCumulativeTranslationWhenSequential() {
    // Same math as test_main_window.cpp's own equivalent test: 30x20 into
    // a 100x50 canvas proportionally scales to width 75.
    const auto pathA = std::filesystem::temp_directory_path() / "sound-mind-test-ie-images-seq-a.png";
    const auto pathB = std::filesystem::temp_directory_path() / "sound-mind-test-ie-images-seq-b.png";
    writeImageScalingTestImage(pathA);
    writeImageScalingTestImage(pathB);
    Project project = Project::createNew(imageScalingTestProjectSettings());
    const std::size_t layerCountBefore = project.layers().size();

    const int importedCount = sound_mind::studio::importImageFilesInto(
        project, {pathB, pathA}, ImageScalePickerDialog::Mode::RescaleToFitProject, /*importAsSequence=*/true);
    std::filesystem::remove(pathA);
    std::filesystem::remove(pathB);

    QCOMPARE(importedCount, 2);
    // Sorted alphabetically regardless of input order - A first.
    QCOMPARE(project.layers()[layerCountBefore].name(), pathA.filename().string());
    QCOMPARE(project.layers()[layerCountBefore].translationColumns(), static_cast<std::int64_t>(0));
    QCOMPARE(project.layers()[layerCountBefore + 1].name(), pathB.filename().string());
    QCOMPARE(project.layers()[layerCountBefore + 1].translationColumns(), static_cast<std::int64_t>(75));
}

void ImportExportTest::importImageFilesIntoReturnsZeroWhenNothingWasImported() {
    Project project = Project::createNew(imageScalingTestProjectSettings());
    QString errorMessage;

    const int importedCount = sound_mind::studio::importImageFilesInto(
        project, {std::filesystem::temp_directory_path() / "sound-mind-does-not-exist.png"},
        ImageScalePickerDialog::Mode::RescaleToFitProject, /*importAsSequence=*/false, &errorMessage);

    QCOMPARE(importedCount, 0);
    QVERIFY(!errorMessage.isEmpty());
}

void ImportExportTest::exportLayerAudioNowWritesARealFile() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-ie-export-audio-src.wav";
    writeTestWavFileWithFrameCount(wavPath, 4410);  // 0.1s.
    Project project = Project::createNew(smallCanvasProjectSettings());
    QVERIFY(sound_mind::studio::importAudioSnippetsInto(project, wavPath, {0}) > 0);
    std::filesystem::remove(wavPath);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-ie-export-audio.flac";
    const bool ok = sound_mind::studio::exportLayerAudioNow(project.layers().back(), exportPath);
    const bool exists = std::filesystem::exists(exportPath);
    std::filesystem::remove(exportPath);

    QVERIFY(ok);
    QVERIFY(exists);
}

void ImportExportTest::exportLayerAudioNowFailsForAnUnrecognizedExtension() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-ie-export-audio-bad-src.wav";
    writeTestWavFileWithFrameCount(wavPath, 4410);
    Project project = Project::createNew(smallCanvasProjectSettings());
    QVERIFY(sound_mind::studio::importAudioSnippetsInto(project, wavPath, {0}) > 0);
    std::filesystem::remove(wavPath);

    QString errorMessage;
    const bool ok = sound_mind::studio::exportLayerAudioNow(
        project.layers().back(), std::filesystem::temp_directory_path() / "sound-mind-test-ie-export-audio.xyz",
        &errorMessage);

    QVERIFY(!ok);
    QVERIFY(!errorMessage.isEmpty());
}

void ImportExportTest::exportLayerVideoNowWritesARealFile() {
    const auto wavPath = std::filesystem::temp_directory_path() / "sound-mind-test-ie-export-video-src.wav";
    writeTestWavFileWithFrameCount(wavPath, 4410);
    Project project = Project::createNew(smallCanvasProjectSettings());
    QVERIFY(sound_mind::studio::importAudioSnippetsInto(project, wavPath, {0}) > 0);
    std::filesystem::remove(wavPath);

    const auto exportPath = std::filesystem::temp_directory_path() / "sound-mind-test-ie-export-video.mp4";
    const bool ok =
        sound_mind::studio::exportLayerVideoNow(project.layers().back(), exportPath, project.settings().canvasWidth);
    const bool exists = std::filesystem::exists(exportPath);
    std::filesystem::remove(exportPath);

    QVERIFY(ok);
    QVERIFY(exists);
}
