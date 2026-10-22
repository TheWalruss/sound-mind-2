#include "test_import_controller.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include <QImage>
#include <QPushButton>
#include <QStatusBar>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/import_controller.h"

using sound_mind::core::Project;
using sound_mind::studio::ImportController;
using sound_mind::studio::ProjectChange;

namespace {

void appendLittleEndian(std::vector<char>& bytes, std::uint32_t value, int count) {
    for (int i = 0; i < count; ++i) {
        bytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

/// A mono 16-bit WAV of `frames` samples of a 440 Hz tone.
void writeWav(const std::filesystem::path& path, std::uint32_t frames) {
    std::vector<char> bytes;
    const std::uint32_t dataSize = frames * 2;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    appendLittleEndian(bytes, 36 + dataSize, 4);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    appendLittleEndian(bytes, 16, 4);
    appendLittleEndian(bytes, 1, 2);
    appendLittleEndian(bytes, 1, 2);
    appendLittleEndian(bytes, 44100, 4);
    appendLittleEndian(bytes, 88200, 4);
    appendLittleEndian(bytes, 2, 2);
    appendLittleEndian(bytes, 16, 2);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendLittleEndian(bytes, dataSize, 4);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const auto sample = static_cast<std::int16_t>(8000.0 * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0));
        appendLittleEndian(bytes, static_cast<std::uint16_t>(sample), 2);
    }
    std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/// A controller over `project` (which may be null), recording what it asks for.
struct Fixture {
    explicit Fixture(Project* project)
        : controller(ImportController::Context{[project]() { return project; },
                                               [this](const QString& description, nlohmann::json) {
                                                   commits.append(description);
                                               },
                                               [](sound_mind::core::LayerId) {},
                                               [this](const ProjectChange& change) { changes.push_back(change); },
                                               [this](const QString& title, const QString& message) {
                                                   errors.append(title + ": " + message);
                                               }},
                     &statusBar) {}

    QStatusBar statusBar;
    QStringList commits;
    std::vector<ProjectChange> changes;
    QStringList errors;
    ImportController controller;
};

/// A saved project, so it has a sources folder.
Project savedProject(const char* name) {
    Project project = Project::createNew(sound_mind::core::ProjectSettings{});
    project.save(std::filesystem::temp_directory_path() / name);
    return project;
}

}  // namespace

// v0.1.48.3 - imports, split out of MainWindow, testable on their own.
void ImportControllerTest::aBackgroundAudioImportAddsLayersWithTheirSourceAndOneEntry() {
    Project project = savedProject("sound-mind-test-import-controller.smproj");
    const auto layersBefore = project.layers().size();
    Fixture fixture(&project);
    const auto wav = std::filesystem::temp_directory_path() / "sound-mind-test-import-controller.wav";
    writeWav(wav, 4410);

    fixture.controller.importAudioSnippetsAsync(wav, {0});
    QVERIFY(fixture.controller.isRunning());
    QCOMPARE(project.layers().size(), layersBefore);  // nothing added on the UI thread yet.
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isRunning(), 60000);

    QCOMPARE(project.layers().size(), layersBefore + 1);
    QCOMPARE(project.sources().size(), std::size_t{1});
    QCOMPARE(fixture.commits.size(), 1);
    QCOMPARE(fixture.changes.size(), std::size_t{1});
    QVERIFY(fixture.changes.front().layerList && fixture.changes.front().audible);
    QVERIFY(fixture.errors.isEmpty());
    std::filesystem::remove(wav);
}

void ImportControllerTest::cancellingABackgroundImportAddsNothing() {
    Project project = savedProject("sound-mind-test-import-cancel.smproj");
    const auto layersBefore = project.layers().size();
    Fixture fixture(&project);
    const auto wav = std::filesystem::temp_directory_path() / "sound-mind-test-import-cancel.wav";
    writeWav(wav, 44100 * 20);  // long enough to still be running.

    fixture.controller.importAudioSnippetsAsync(wav, {0});
    fixture.statusBar.findChild<QPushButton*>(QStringLiteral("importCancelButton"))->click();
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isRunning(), 60000);
    QCOMPARE(project.layers().size(), layersBefore);
    QVERIFY(fixture.commits.isEmpty());
    QVERIFY(fixture.changes.empty());
    std::filesystem::remove(wav);
}

void ImportControllerTest::anImageImportAddsALayerAndNotifies() {
    Project project = Project::createNew(sound_mind::core::ProjectSettings{});
    const auto layersBefore = project.layers().size();
    Fixture fixture(&project);
    const auto png = std::filesystem::temp_directory_path() / "sound-mind-test-import-controller.png";
    QImage image(32, 16, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(QString::fromStdWString(png.wstring())));

    QVERIFY(fixture.controller.importImageFile(png, sound_mind::studio::ImageScalePickerDialog::Mode::RescaleToFitProject));
    QCOMPARE(project.layers().size(), layersBefore + 1);
    QCOMPARE(fixture.changes.size(), std::size_t{1});
    QVERIFY(fixture.statusBar.currentMessage().contains(QStringLiteral("Imported")));
    std::filesystem::remove(png);
}

void ImportControllerTest::withNoProjectNothingIsImported() {
    Fixture fixture(nullptr);
    QString error;
    QVERIFY(!fixture.controller.importAudioSnippets("missing.wav", {0}, &error));
    QCOMPARE(error, QStringLiteral("No project is open."));
    fixture.controller.importAudioSnippetsAsync("missing.wav", {0});
    QVERIFY(!fixture.controller.isRunning());
    QVERIFY(fixture.changes.empty());
}
