#include "test_export_controller.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include <QImage>
#include <QPushButton>
#include <QStatusBar>
#include <QtTest/QtTest>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/export_controller.h"
#include "sound_mind/studio/import_export.h"

using sound_mind::core::Project;
using sound_mind::studio::ExportController;

namespace {

/// A project whose Background layer holds a steady tone-ish level.
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

/// A controller over `project`, recording reported errors.
struct Fixture {
    explicit Fixture(Project* project)
        : controller(
              ExportController::Context{
                  [project]() -> const Project* { return project; }, []() { return std::optional<float>(); },
                  [](sound_mind::core::MacroId, QString* error) -> std::unique_ptr<sound_mind::core::MacroRenderer> {
                      *error = QStringLiteral("no such macro");
                      return nullptr;
                  },
                  [this](const QString& title, const QString& message) { errors.append(title + ": " + message); }},
              &statusBar) {}

    QStatusBar statusBar;
    QStringList errors;
    ExportController controller;
};

std::filesystem::path tempPath(const char* name) { return std::filesystem::temp_directory_path() / name; }

}  // namespace

// v0.1.48.2 - exports, split out of MainWindow, testable on their own.
void ExportControllerTest::anAudioExportWritesTheFileAndSaysSo() {
    Project project = projectWithContent(64);
    Fixture fixture(&project);
    const auto path = tempPath("sound-mind-test-export-controller.flac");
    std::filesystem::remove(path);

    fixture.controller.exportAudioAsync(path, sound_mind::codec::CompressedAudioFormat::Flac);
    QVERIFY(fixture.controller.isRunning());
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isRunning(), 60000);
    QVERIFY(std::filesystem::exists(path));
    QVERIFY(fixture.statusBar.currentMessage().contains(QStringLiteral("Exported audio")));
    QVERIFY(fixture.errors.isEmpty());
    std::filesystem::remove(path);
}

void ExportControllerTest::nothingToExportIsReportedAndNothingStarts() {
    Project project = Project::createNew(sound_mind::core::ProjectSettings{});  // no content anywhere.
    Fixture fixture(&project);
    fixture.controller.exportVideoAsync(tempPath("sound-mind-test-export-nothing.mp4"));
    QVERIFY(!fixture.controller.isRunning());
    QCOMPARE(fixture.errors.size(), 1);
    QVERIFY(fixture.errors.front().contains(QStringLiteral("Nothing to export")));
}

void ExportControllerTest::aSecondExportWhileOneRunsIsRefused() {
    Project project = projectWithContent(2000);
    Fixture fixture(&project);
    const auto first = tempPath("sound-mind-test-export-first.mp4");
    const auto second = tempPath("sound-mind-test-export-second.mp4");
    fixture.controller.exportVideoAsync(first);
    fixture.controller.exportVideoAsync(second);
    QVERIFY(fixture.statusBar.currentMessage().contains(QStringLiteral("Already exporting")));
    fixture.controller.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isRunning(), 60000);
    QVERIFY(!std::filesystem::exists(second));
}

void ExportControllerTest::cancellingDeletesThePartialFile() {
    Project project = projectWithContent(4000);  // long enough not to finish first.
    Fixture fixture(&project);
    const auto path = tempPath("sound-mind-test-export-cancel.mp4");
    std::filesystem::remove(path);
    auto* cancel = fixture.statusBar.findChild<QPushButton*>(QStringLiteral("exportCancelButton"));
    QVERIFY(cancel != nullptr);

    fixture.controller.exportVideoAsync(path);
    QVERIFY(!cancel->isHidden());
    cancel->click();
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isRunning(), 60000);
    QVERIFY(cancel->isHidden());
    QVERIFY(!std::filesystem::exists(path));
    QVERIFY(fixture.statusBar.currentMessage().contains(QStringLiteral("cancelled")));
}

void ExportControllerTest::aMacroThatCantBeRebuiltIsReported() {
    Project project = projectWithContent(64);
    Fixture fixture(&project);
    fixture.controller.exportMacroVideoAsync(tempPath("sound-mind-test-export-macro.mp4"), 7);
    QVERIFY(!fixture.controller.isRunning());
    QCOMPARE(fixture.errors.size(), 1);
    QVERIFY(fixture.errors.front().contains(QStringLiteral("no such macro")));
}

// v0.1.52.1 - File -> Export Image..., in the background like the others.
void ExportControllerTest::anImageExportWritesTheFileAndSaysSo() {
    Project project = projectWithContent(64);
    Fixture fixture(&project);
    const auto path = tempPath("sound-mind-test-export-controller.png");
    std::filesystem::remove(path);

    sound_mind::studio::ImageExportOptions options;
    options.width = 32;
    options.height = 16;
    fixture.controller.exportImageAsync(path, options);
    QVERIFY(fixture.controller.isRunning());
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isRunning(), 60000);
    QCOMPARE(QImage(QString::fromStdWString(path.wstring())).size(), QSize(32, 16));
    QVERIFY(fixture.statusBar.currentMessage().contains(QStringLiteral("Exported image")));
    QVERIFY(fixture.errors.isEmpty());
    std::filesystem::remove(path);
}

void ExportControllerTest::anImageExportWithNothingVisibleIsReported() {
    Project project = Project::createNew(sound_mind::core::ProjectSettings{});  // no content anywhere.
    Fixture fixture(&project);
    fixture.controller.exportImageAsync(tempPath("sound-mind-test-export-nothing.png"), {});
    QVERIFY(!fixture.controller.isRunning());
    QCOMPARE(fixture.errors.size(), 1);
    QVERIFY(fixture.errors.front().contains(QStringLiteral("Nothing to export")));
}
