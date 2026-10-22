#include "test_analysis_controller.h"

#include <algorithm>
#include <filesystem>
#include <memory>

#include <QSettings>
#include <QtTest/QtTest>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/analysis_controller.h"
#include "sound_mind/studio/analysis_panel.h"

using sound_mind::core::Project;
using sound_mind::studio::AnalysisController;
using sound_mind::studio::AnalysisPanel;

namespace {

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

/// A controller over `project` with its own settings file.
struct Fixture {
    explicit Fixture(const Project* project)
        : settingsPath(std::filesystem::temp_directory_path() / "sound-mind-test-analysis-controller.ini"),
          settings((std::filesystem::remove(settingsPath), QString::fromStdWString(settingsPath.wstring())),
                   QSettings::IniFormat),
          controller(&panel, [project]() { return project; }, &settings) {}

    std::filesystem::path settingsPath;
    QSettings settings;
    AnalysisPanel panel;
    AnalysisController controller;
};

}  // namespace

// v0.1.48.5 - the Analysis panel's runs, split out of MainWindow.
void AnalysisControllerTest::analyzingShowsAReport() {
    Project project = projectWithContent(100);
    Fixture fixture(&project);
    QVERIFY(!fixture.panel.hasReport());
    fixture.controller.analyze();
    QVERIFY(fixture.controller.isAnalyzing());
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isAnalyzing(), 60000);
    QVERIFY(fixture.panel.hasReport());
}

void AnalysisControllerTest::anEditDuringAutoUpdateCancelsAndReanalyzes() {
    Project project = projectWithContent(3000);  // long enough to still be running.
    Fixture fixture(&project);
    fixture.controller.setAutoAnalyze(true);  // no report yet: analyzes at once.
    QVERIFY(fixture.controller.isAnalyzing());
    fixture.controller.noteProjectChanged();
    QCOMPARE(fixture.controller.cancelledCount(), 1);
    // The cancelled run winds down, then the debounce starts a fresh one.
    QTRY_VERIFY_WITH_TIMEOUT(fixture.panel.hasReport() && !fixture.controller.isAnalyzing(), 120000);
}

void AnalysisControllerTest::anotherProjectDropsTheReport() {
    Project project = projectWithContent(100);
    Fixture fixture(&project);
    fixture.controller.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isAnalyzing(), 60000);
    QVERIFY(fixture.panel.hasReport());
    fixture.controller.projectChanged();
    QVERIFY(!fixture.panel.hasReport());
}

void AnalysisControllerTest::autoUpdateIsRemembered() {
    Project project = projectWithContent(100);
    {
        Fixture fixture(&project);
        fixture.controller.setAutoAnalyze(true);
        QVERIFY(fixture.settings.value(QStringLiteral("analysisAutoUpdate")).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.isAnalyzing(), 60000);
    }
}
