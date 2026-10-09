#include "test_resource_browser_controller.h"

#include <filesystem>
#include <fstream>

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/resource_file.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/resource_browser_controller.h"
#include "sound_mind/studio/resource_browser_panel.h"

using sound_mind::core::MindWave;
using sound_mind::core::NamedConvolutionKernel;
using sound_mind::core::NamedMindWave;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::studio::ResourceBrowserController;
using sound_mind::studio::ResourceBrowserPanel;
using sound_mind::studio::ResourceCategory;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 20;
    settings.canvasHeight = 10;
    settings.binCount = 10;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

std::filesystem::path scratchPath(const std::string& nameWithExtension) {
    return std::filesystem::temp_directory_path() / ("resource-browser-controller-test-" + nameWithExtension);
}

struct Fixture {
    Project project = Project::createNew(testSettings());
    ResourceBrowserPanel panel;
    ResourceBrowserController controller{&panel};

    Fixture() {
        controller.setProject(&project);
        // QTest::mouseClick() (used by a few of this class's own tests)
        // needs real, laid-out geometry to hit-test against - see
        // test_resource_browser_panel.cpp's own identical comment.
        panel.show();
    }

    void selectCategory(ResourceCategory category) {
        auto* combo = panel.findChild<QComboBox*>(QStringLiteral("categoryCombo"));
        for (int i = 0; i < combo->count(); ++i) {
            if (static_cast<ResourceCategory>(combo->itemData(i).toInt()) == category) {
                combo->setCurrentIndex(i);
                return;
            }
        }
        QFAIL("Category not found in combo");
    }
};

}  // namespace

void ResourceBrowserControllerTest::refreshPanelWithNoProjectShowsNoEntries() {
    ResourceBrowserPanel panel;
    ResourceBrowserController controller(&panel);
    controller.setProject(nullptr);
    controller.refreshPanel();
    QCOMPARE(panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->count(), 0);
}

void ResourceBrowserControllerTest::refreshPanelListsEachCategorysOwnLibrary() {
    Fixture fixture;
    fixture.project.addMindWave("My Wave", MindWave{});
    fixture.project.addToolPreset("My Preset", ProceduralConfiguration{});
    fixture.controller.refreshPanel();

    fixture.selectCategory(ResourceCategory::MindWave);
    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("My Wave"));

    fixture.selectCategory(ResourceCategory::ToolPreset);
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->text().contains(QStringLiteral("My Preset")));
    QVERIFY(list->item(0)->text().contains(QStringLiteral("Procedural")));
}

void ResourceBrowserControllerTest::selectingAnEntryPopulatesTheInspector() {
    Fixture fixture;
    fixture.project.addMindWave("My Wave", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);

    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    list->setCurrentRow(0);

    QCOMPARE(fixture.panel.findChild<QLabel*>(QStringLiteral("inspectorNameLabel"))->text(),
             QStringLiteral("My Wave"));
    QVERIFY(!fixture.panel.findChild<QPlainTextEdit*>(QStringLiteral("inspectorParametersEdit"))->toPlainText().isEmpty());
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("exportButton"))->isHidden());
    QVERIFY(fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
}

void ResourceBrowserControllerTest::selectingAMindShotRendersARasterAndEnablesPlay() {
    Fixture fixture;
    sound_mind::core::Clip clip;
    clip.frameCount = 4;
    clip.binCount = fixture.project.settings().binCount;
    clip.leftMagnitudeDb.assign(clip.frameCount * clip.binCount, -20.0f);
    clip.rightMagnitudeDb.assign(clip.frameCount * clip.binCount, -20.0f);
    clip.sharedPhaseRadians.assign(clip.frameCount * clip.binCount, 0.0f);
    fixture.project.addMindShot("My Shot", clip);
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindShot);

    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    QVERIFY(!fixture.panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
}

void ResourceBrowserControllerTest::exportThenImportFromFileRoundTripsAMindWaveIntoTheProject() {
    Fixture fixture;
    fixture.project.addMindWave("Exported Wave", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    const std::filesystem::path path = scratchPath("wave.smwave");
    QString errorMessage;
    QVERIFY(fixture.controller.exportSelectedEntry(path, &errorMessage));
    QVERIFY(errorMessage.isEmpty());

    QSignalSpy spy(&fixture.controller, &ResourceBrowserController::resourcesChanged);
    QVERIFY(fixture.controller.importFromFile(path, &errorMessage));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(fixture.project.mindWaves().size(), std::size_t{2});

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::browseOtherProjectShowsItsOwnLibraryWithoutMutatingTheCurrentProject() {
    Project other = Project::createNew(testSettings());
    other.addMindWave("Other Project's Wave", MindWave{});
    const std::filesystem::path path = scratchPath("other.smproj");
    other.save(path);

    Fixture fixture;
    fixture.project.addMindWave("My Project's Wave", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);

    QString errorMessage;
    QVERIFY(fixture.controller.browseOtherProjectAt(path, &errorMessage));

    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("Other Project's Wave"));
    // The current project itself is untouched.
    QCOMPARE(fixture.project.mindWaves().size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(fixture.project.mindWaves().front().name), QStringLiteral("My Project's Wave"));

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::importEntryCopiesFromTheBrowsedProjectIntoTheCurrentOne() {
    Project other = Project::createNew(testSettings());
    other.addMindWave("Borrowed Wave", MindWave{});
    const std::filesystem::path path = scratchPath("other-import.smproj");
    other.save(path);

    Fixture fixture;
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    QVERIFY(fixture.controller.browseOtherProjectAt(path));

    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    list->setCurrentRow(0);
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("importEntryButton"))->isHidden());

    QSignalSpy spy(&fixture.controller, &ResourceBrowserController::resourcesChanged);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("importEntryButton")), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(fixture.project.mindWaves().size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(fixture.project.mindWaves().front().name), QStringLiteral("Borrowed Wave"));

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::returnToCurrentProjectRestoresTheCurrentProjectsOwnView() {
    Project other = Project::createNew(testSettings());
    other.addMindWave("Other's Wave", MindWave{});
    const std::filesystem::path path = scratchPath("other-return.smproj");
    other.save(path);

    Fixture fixture;
    fixture.project.addMindWave("Mine", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    QVERIFY(fixture.controller.browseOtherProjectAt(path));

    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("returnToCurrentProjectButton")),
                       Qt::LeftButton);

    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("Mine"));
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton"))->isHidden());

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::mindGrainCategoryNeverOffersImportEvenWhileBrowsing() {
    Project other = Project::createNew(testSettings());
    other.addMindGrain("Other's Grain", other.layers().front().id(), sound_mind::core::TimeFrequencyRect{});
    const std::filesystem::path path = scratchPath("other-grain.smproj");
    other.save(path);

    Fixture fixture;
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindGrain);
    QVERIFY(fixture.controller.browseOtherProjectAt(path));

    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 1);
    list->setCurrentRow(0);

    QVERIFY(fixture.panel.findChild<QPushButton*>(QStringLiteral("importEntryButton"))->isHidden());
    QVERIFY(fixture.panel.findChild<QPushButton*>(QStringLiteral("exportButton"))->isHidden());

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::addToToolkitThenExportThenImportRoundTripsMixedResourcesAndClearsTheDraft() {
    Fixture fixture;
    fixture.project.addMindWave("Wave For Toolkit", MindWave{});
    NamedConvolutionKernel kernel;
    kernel.name = "Kernel For Toolkit";
    fixture.project.addConvolutionKernel(kernel.name, kernel.size, kernel.coefficients, kernel.normalize);
    fixture.controller.refreshPanel();

    fixture.selectCategory(ResourceCategory::MindWave);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton")), Qt::LeftButton);

    fixture.selectCategory(ResourceCategory::ConvolutionKernel);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton")), Qt::LeftButton);

    auto* toolkitList = fixture.panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"));
    QCOMPARE(toolkitList->count(), 2);

    const std::filesystem::path path = scratchPath("roundtrip.smtoolkit");
    QString errorMessage;
    QVERIFY(fixture.controller.exportToolkitAt(QStringLiteral("My Toolkit"), path, &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    // A successful export clears the draft.
    QCOMPARE(toolkitList->count(), 0);

    QSignalSpy spy(&fixture.controller, &ResourceBrowserController::resourcesChanged);
    QVERIFY(fixture.controller.importToolkitFrom(path, &errorMessage));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(fixture.project.mindWaves().size(), std::size_t{2});
    QCOMPARE(fixture.project.convolutionKernels().size(), std::size_t{2});

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::exportToolkitFailsWhenTheDraftIsEmpty() {
    Fixture fixture;
    QString errorMessage;
    QVERIFY(!fixture.controller.exportToolkitAt(QStringLiteral("Empty"), scratchPath("empty.smtoolkit"), &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void ResourceBrowserControllerTest::removingADraftEntryTakesItOutOfTheNextExport() {
    Fixture fixture;
    fixture.project.addMindWave("Keep Me", MindWave{});
    fixture.project.addMindWave("Remove Me", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);

    auto* entriesList = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    entriesList->setCurrentRow(0);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton")), Qt::LeftButton);
    entriesList->setCurrentRow(1);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton")), Qt::LeftButton);

    auto* toolkitList = fixture.panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"));
    QCOMPARE(toolkitList->count(), 2);
    toolkitList->setCurrentRow(1);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("removeFromToolkitButton")),
                       Qt::LeftButton);
    QCOMPARE(toolkitList->count(), 1);
    QCOMPARE(toolkitList->item(0)->text(), QStringLiteral("Keep Me (MindWave)"));

    const std::filesystem::path path = scratchPath("after-remove.smtoolkit");
    QVERIFY(fixture.controller.exportToolkitAt(QStringLiteral("Toolkit"), path));
    const auto imported = sound_mind::core::importToolkit(path);
    QCOMPARE(imported.entries.size(), std::size_t{1});

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::importToolkitIsAllOrNothingOnAMalformedEntry() {
    Fixture fixture;
    fixture.project.addMindWave("Pre-existing", MindWave{});

    // Hand-crafted: a valid MindWave entry followed by a MindShot entry
    // whose own resource is missing required fields - importToolkit()
    // itself parses the envelope fine (it's well-formed JSON), but
    // applyToolkitEntry()'s own resource.get<NamedMindShot>() throws once
    // it actually tries to deserialize the second entry's payload.
    nlohmann::json envelope;
    envelope["soundMindToolkit"] = true;
    envelope["formatVersion"] = 1;
    envelope["name"] = "Malformed";
    envelope["entries"] = nlohmann::json::array();
    envelope["entries"].push_back(
        {{"resourceType", "mindWave"}, {"resource", nlohmann::json(NamedMindWave{})}});
    envelope["entries"].push_back({{"resourceType", "mindShot"}, {"resource", nlohmann::json::object()}});

    const std::filesystem::path path = scratchPath("malformed.smtoolkit");
    {
        std::ofstream file(path);
        file << envelope.dump(2);
    }

    QString errorMessage;
    QVERIFY(!fixture.controller.importToolkitFrom(path, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    // Nothing was added - the first, valid entry's own mutation was
    // discarded along with the trial copy it happened on.
    QCOMPARE(fixture.project.mindWaves().size(), std::size_t{1});

    std::filesystem::remove(path);
}
