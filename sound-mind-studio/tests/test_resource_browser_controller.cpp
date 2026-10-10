#include "test_resource_browser_controller.h"

#include <filesystem>
#include <fstream>
#include <functional>

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QtTest>

#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/resonant_instrument.h"
#include "sound_mind/core/resource_file.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/resource_browser_controller.h"
#include "sound_mind/studio/resource_browser_panel.h"

using sound_mind::core::CurveGraph;
using sound_mind::core::CurvePoint;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::NamedConvolutionKernel;
using sound_mind::core::NamedFilterPreset;
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
    // A MindWave now also offers an audio preview - see
    // selectingAMindWaveRendersAPreviewRaster()/
    // playingAMindWavePreviewSynthesizesAndDecodesAudio() for the
    // dedicated coverage; this test's own concern is the inspector's
    // other fields, so just confirms Play is shown, not hidden.
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
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

void ResourceBrowserControllerTest::playingAMindShotDecodesLazilyOnFirstPlayAndStaysPlayableOnASecondPlay() {
    // The actual decode (sound_mind::codec::decode(), a full inverse STFT)
    // used to run eagerly in refreshInspector(), on mere selection - direct
    // user feedback ("very very slow") - rather than being deferred to the
    // first Play click. This exercises the real decode path end to end
    // (not just canPlay's own visibility, which
    // selectingAMindShotRendersARasterAndEnablesPlay() already covers):
    // Play must still actually work on first click despite the deferral,
    // and a second Play on the same selection must still work too (the
    // cached-decode path).
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

    auto* playButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* stopButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);

    QTest::mouseClick(playButton, Qt::LeftButton);
    QVERIFY(!stopButton->isHidden());

    QTest::mouseClick(stopButton, Qt::LeftButton);
    QVERIFY(!playButton->isHidden());

    // Second Play on the same selection - exercises the cached decode
    // (currentPreviewAudio_ already populated from the first Play above).
    QTest::mouseClick(playButton, Qt::LeftButton);
    QVERIFY(!stopButton->isHidden());
}

void ResourceBrowserControllerTest::playingAMindWavePreviewSynthesizesAndDecodesAudio() {
    // Direct user feedback: "practically wherever there is a visual
    // preview of something, give the user the ability to play an audio
    // preview" - a MindWave's own preview is a default Instrument bound
    // to it as vibrato, painted as a representative stroke and decoded,
    // exercising that synthesis end to end (not just canPlay's own
    // visibility, which selectingAMindWaveRendersAPreviewRaster() already
    // covers).
    Fixture fixture;
    fixture.project.addMindWave("My Wave", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    auto* playButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* stopButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);

    QTest::mouseClick(playButton, Qt::LeftButton);
    QVERIFY(!stopButton->isHidden());
}

void ResourceBrowserControllerTest::playingAToolPresetPreviewSynthesizesAndDecodesAudio() {
    // A Tool Preset's own real configuration, painted as the same
    // representative stroke and decoded.
    Fixture fixture;
    fixture.project.addToolPreset("My Brush", ProceduralConfiguration{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ToolPreset);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    auto* playButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* stopButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);

    QTest::mouseClick(playButton, Qt::LeftButton);
    QVERIFY(!stopButton->isHidden());
}

void ResourceBrowserControllerTest::playingAResonanceProfilePreviewSynthesizesAndDecodesAudio() {
    // A default Resonance brush carrying this profile's own spectrum,
    // painted as the same representative stroke and decoded.
    Fixture fixture;
    fixture.project.addResonanceProfile("My Resonance", {0.1f, 0.5f, 1.0f}, CurveGraph{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ResonanceProfile);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    auto* playButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"));
    auto* stopButton = fixture.panel.findChild<QPushButton*>(QStringLiteral("stopButton"));
    QVERIFY(playButton != nullptr);
    QVERIFY(stopButton != nullptr);

    QTest::mouseClick(playButton, Qt::LeftButton);
    QVERIFY(!stopButton->isHidden());
}

void ResourceBrowserControllerTest::resonanceProfileWithAnEmptySpectrumOffersNoPlayButton() {
    // A degenerate entry (shouldn't exist via any real UI path, but
    // defended against anyway - see syntheticResonancePreviewConfig()'s
    // own call site's `if (!entry->spectrum.empty())` guard): nothing
    // sensible to paint or decode, so no Play button at all, rather than
    // a Play button that produces silence or a wasted paint operation.
    Fixture fixture;
    fixture.project.addResonanceProfile("Empty", {}, CurveGraph{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ResonanceProfile);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    QVERIFY(fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
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

    // Re-importing lands on the same, still-populated project, so the
    // re-imported "Exported Wave" collides with the one still there -
    // Keep Both (the closest match to this test's own pre-collision-
    // handling intent: both entries end up coexisting) via the same
    // QTimer::singleShot()-before-exec() technique
    // shownContextMenuActionTexts() (test_main_window.cpp) already
    // establishes for any modal shown from inside a blocking call.
    QTimer::singleShot(0, [&]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(box != nullptr);
        for (QAbstractButton* button : box->buttons()) {
            if (box->buttonRole(button) == QMessageBox::ActionRole) {
                button->click();
                return;
            }
        }
        QFAIL("No Keep Both button found.");
    });

    QSignalSpy spy(&fixture.controller, &ResourceBrowserController::resourcesChanged);
    QVERIFY(fixture.controller.importFromFile(path, &errorMessage));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(fixture.project.mindWaves().size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(fixture.project.mindWaves()[1].name), QStringLiteral("Exported Wave (2)"));

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

    // The target project still has both names - re-importing the toolkit
    // collides twice (MindWave, then ConvolutionKernel), each its own
    // blocking QMessageBox (Overwrite/Keep Existing/Keep Both) applied
    // one entry at a time - Keep Both both times is the closest match to
    // this test's own pre-collision-handling intent (both end up
    // duplicated), via the same QTimer::singleShot()-before-exec()
    // technique shownContextMenuActionTexts() (test_main_window.cpp)
    // already establishes.
    auto clickKeepBoth = [](std::function<void()> next) {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr) {
            QFAIL("Expected a collision QMessageBox, found none.");
            return;
        }
        for (QAbstractButton* button : box->buttons()) {
            if (box->buttonRole(button) == QMessageBox::ActionRole) {
                button->click();
                if (next) {
                    QTimer::singleShot(0, std::move(next));
                }
                return;
            }
        }
        QFAIL("No Keep Both button found.");
    };
    QTimer::singleShot(0, [&]() { clickKeepBoth([&]() { clickKeepBoth(nullptr); }); });

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

void ResourceBrowserControllerTest::setProjectStopsBrowsingAnyOtherProject() {
    Project other = Project::createNew(testSettings());
    other.addMindWave("Other's Wave", MindWave{});
    const std::filesystem::path path = scratchPath("other-setproject.smproj");
    other.save(path);

    Fixture fixture;
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    QVERIFY(fixture.controller.browseOtherProjectAt(path));
    // Actively browsing: "Browse Other Project..." is hidden in favor of
    // "Return to This Project".
    QVERIFY(fixture.panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton"))->isHidden());

    Project replacement = Project::createNew(testSettings());
    replacement.addMindWave("Replacement's Wave", MindWave{});
    fixture.controller.setProject(&replacement);
    fixture.controller.refreshPanel();

    // setProject() itself stops browsing the other project - the panel
    // now reflects `replacement`, not the one browseOtherProjectAt()
    // loaded, confirming browsedProject_/browsedProjectPath_ were both
    // actually cleared, not just shadowed.
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton"))->isHidden());
    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("Replacement's Wave"));

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::toolkitDraftEntryIsASnapshotUnaffectedByLaterEditsToItsSource() {
    Fixture fixture;
    MindWave wave;
    wave.setPeriod(1.0);
    const auto id = fixture.project.addMindWave("Mutable Wave", wave);
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);
    QTest::mouseClick(fixture.panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton")), Qt::LeftButton);

    // Mutate the source *after* it was added to the draft.
    fixture.project.mindWaveById(id)->wave.setPeriod(999.0);

    const std::filesystem::path path = scratchPath("snapshot.smtoolkit");
    QVERIFY(fixture.controller.exportToolkitAt(QStringLiteral("Snapshot Test"), path));
    const auto imported = sound_mind::core::importToolkit(path);

    QCOMPARE(imported.entries.size(), std::size_t{1});
    const auto exportedWave = imported.entries[0].resource.get<NamedMindWave>();
    // The draft captured the wave's own period (1.0) at "Add to Toolkit"
    // time - the later mutation to 999.0 never reaches the exported file.
    QCOMPARE(exportedWave.wave.period(), 1.0);

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::filterPresetCategoryListsExportsAndImports() {
    Fixture fixture;
    FilterConfiguration config;
    config.setType(FilterType::Sharpen);
    config.setSharpenAmount(0.6f);
    fixture.project.addFilterPreset("My Sharpen Preset", config);
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::FilterPreset);

    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("My Sharpen Preset"));
    list->setCurrentRow(0);

    const std::filesystem::path path = scratchPath("preset.smfilter");
    QString errorMessage;
    QVERIFY(fixture.controller.exportSelectedEntry(path, &errorMessage));

    // Re-importing lands on the same, still-populated project - see
    // exportThenImportFromFileRoundTripsAMindWaveIntoTheProject()'s own
    // identical comment for why Keep Both is armed here.
    QTimer::singleShot(0, [&]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(box != nullptr);
        for (QAbstractButton* button : box->buttons()) {
            if (box->buttonRole(button) == QMessageBox::ActionRole) {
                button->click();
                return;
            }
        }
        QFAIL("No Keep Both button found.");
    });

    QVERIFY(fixture.controller.importFromFile(path, &errorMessage));
    QCOMPARE(fixture.project.filterPresets().size(), std::size_t{2});
    QCOMPARE(fixture.project.filterPresets()[1].config.type(), FilterType::Sharpen);

    std::filesystem::remove(path);
}

void ResourceBrowserControllerTest::selectingAMindWaveRendersAPreviewRaster() {
    Fixture fixture;
    fixture.project.addMindWave("My Wave", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);

    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    QVERIFY(!fixture.panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
    // Direct user feedback: "wherever there is a visual preview of
    // something, give the user the ability to play an audio preview" -
    // a MindWave's own preview is synthesized (a default Instrument
    // bound to it as vibrato, painted as a representative stroke).
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
}

void ResourceBrowserControllerTest::selectingAToolPresetRendersAStrokePreviewRaster() {
    Fixture fixture;
    fixture.project.addToolPreset("My Brush", ProceduralConfiguration{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ToolPreset);

    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    QVERIFY(!fixture.panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
}

void ResourceBrowserControllerTest::selectingAResonanceProfileRendersItsSourceCurveAlongsideTheSpectrum() {
    Fixture fixture;
    CurveGraph graph;
    const std::size_t a = graph.addNode(CurvePoint{0.0, 0.0});
    const std::size_t b = graph.addNode(CurvePoint{1.0, 1.0});
    graph.addEdge(a, b);
    fixture.project.addResonanceProfile("My Resonance", {0.1f, 0.5f, 1.0f}, graph);
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ResonanceProfile);

    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    QVERIFY(!fixture.panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
    QVERIFY(fixture.panel.findChild<QPlainTextEdit*>(QStringLiteral("inspectorParametersEdit"))
                ->toPlainText()
                .contains(QStringLiteral("sourceCurve: 2 nodes")));
    QVERIFY(!fixture.panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
}

void ResourceBrowserControllerTest::addingAMindWaveBoundToolPresetToToolkitAlsoAddsTheMindWaveAndNotifies() {
    Fixture fixture;
    const MindWaveId waveId = fixture.project.addMindWave("Driving Wave", MindWave{});
    ProceduralConfiguration config;
    config.setOpacityMindWave(waveId);
    fixture.project.addToolPreset("Bound Brush", config);
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ToolPreset);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    const QStringList autoAdded = fixture.controller.addSelectedEntryToToolkit();

    QCOMPARE(autoAdded.size(), 1);
    QVERIFY(autoAdded.front().contains(QStringLiteral("Driving Wave")));
    auto* toolkitList = fixture.panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"));
    QCOMPARE(toolkitList->count(), 2);
}

void ResourceBrowserControllerTest::addingAnEntryWithNoDependenciesNeverNotifies() {
    Fixture fixture;
    fixture.project.addMindWave("Lone Wave", MindWave{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::MindWave);
    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    const QStringList autoAdded = fixture.controller.addSelectedEntryToToolkit();

    QVERIFY(autoAdded.isEmpty());
    QCOMPARE(fixture.panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"))->count(), 1);
}

void ResourceBrowserControllerTest::addingTheSameDependencyTwiceDoesNotDuplicateIt() {
    Fixture fixture;
    const MindWaveId waveId = fixture.project.addMindWave("Shared Wave", MindWave{});
    ProceduralConfiguration first;
    first.setOpacityMindWave(waveId);
    ProceduralConfiguration second;
    second.setSizeMindWave(waveId);
    fixture.project.addToolPreset("Brush One", first);
    fixture.project.addToolPreset("Brush Two", second);
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ToolPreset);
    auto* list = fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"));

    list->setCurrentRow(0);
    const QStringList firstAutoAdded = fixture.controller.addSelectedEntryToToolkit();
    list->setCurrentRow(1);
    const QStringList secondAutoAdded = fixture.controller.addSelectedEntryToToolkit();

    QCOMPARE(firstAutoAdded.size(), 1);
    // The shared MindWave was already in the draft from the first add - no
    // second copy, and nothing to report this time.
    QVERIFY(secondAutoAdded.isEmpty());
    auto* toolkitList = fixture.panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"));
    QCOMPARE(toolkitList->count(), 3);  // Brush One, Brush Two, Shared Wave (once).
}

void ResourceBrowserControllerTest::
    toolPresetPreviewStillRendersWhenTheProjectsOwnMaxFrequencyIsBelowOneKilohertz() {
    // The preview is clipped vertically at 1 kHz - a project whose own
    // encoded range never reaches 1 kHz at all must still render a real
    // (non-empty, non-crashing) raster rather than an empty crop.
    ProjectSettings lowRangeSettings = testSettings();
    lowRangeSettings.minFrequencyHz = 20.0f;
    lowRangeSettings.maxFrequencyHz = 500.0f;

    Fixture fixture;
    fixture.project = Project::createNew(lowRangeSettings);
    fixture.project.addToolPreset("Low-Range Brush", ProceduralConfiguration{});
    fixture.controller.refreshPanel();
    fixture.selectCategory(ResourceCategory::ToolPreset);

    fixture.panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);

    QVERIFY(!fixture.panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
}
