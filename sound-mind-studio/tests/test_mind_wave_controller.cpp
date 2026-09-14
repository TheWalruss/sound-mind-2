#include "test_mind_wave_controller.h"

#include <QComboBox>
#include <QListWidget>
#include <QtTest/QtTest>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/filter_configuration_panel.h"
#include "sound_mind/studio/layers_panel.h"
#include "sound_mind/studio/mind_wave_controller.h"
#include "sound_mind/studio/mind_waves_panel.h"

using sound_mind::core::Layer;
using sound_mind::core::LayerType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::FilterConfigurationPanel;
using sound_mind::studio::LayersPanel;
using sound_mind::studio::MindWaveController;
using sound_mind::studio::MindWavesPanel;

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

/// @brief Owns every collaborator a MindWaveController needs, matching
/// LayerControllerTest's own Fixture shape.
struct Fixture {
    MindWavesPanel mindWavesPanel;
    LayersPanel layersPanel;
    FilterConfigurationPanel filterConfigurationPanel;
    MindWaveController controller{&mindWavesPanel, &layersPanel, &filterConfigurationPanel};
};

}  // namespace

void MindWaveControllerTest::withNoProjectEveryMutationIsANoOp() {
    Fixture fixture;
    fixture.controller.addMindWave();
    fixture.controller.removeMindWave(1);
    QVERIFY(!fixture.controller.renameMindWaveTo(1, QStringLiteral("New Name")));
    fixture.controller.updateMindWave(1, MindWave{});
    // Nothing to assert on directly (no project to inspect) - this test's
    // whole point is that none of these crash with no project set.
}

void MindWaveControllerTest::addMindWaveAddsWithADefaultNameAndSelectsIt() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);

    fixture.controller.addMindWave();

    QCOMPARE(project.mindWaves().size(), std::size_t{1});
    QCOMPARE(project.mindWaves().front().name, std::string("MindWave 1"));
    QVERIFY(fixture.mindWavesPanel.selectedMindWaveId().has_value());
    QCOMPARE(*fixture.mindWavesPanel.selectedMindWaveId(), project.mindWaves().front().id);
}

void MindWaveControllerTest::addMindWaveNamesSequentialEntriesDistinctly() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);

    fixture.controller.addMindWave();
    fixture.controller.addMindWave();

    QCOMPARE(project.mindWaves().size(), std::size_t{2});
    QCOMPARE(project.mindWaves()[0].name, std::string("MindWave 1"));
    QCOMPARE(project.mindWaves()[1].name, std::string("MindWave 2"));
}

void MindWaveControllerTest::removeMindWaveRemovesAndReturnsToNoSelection() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    fixture.controller.addMindWave();
    const MindWaveId id = project.mindWaves().front().id;

    fixture.controller.removeMindWave(id);

    QVERIFY(project.mindWaves().empty());
    QVERIFY(!fixture.mindWavesPanel.selectedMindWaveId().has_value());
}

void MindWaveControllerTest::removeMindWaveNoOpsForUnknownId() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    fixture.controller.addMindWave();

    fixture.controller.removeMindWave(999999);

    QCOMPARE(project.mindWaves().size(), std::size_t{1});
}

void MindWaveControllerTest::renameMindWaveToRenamesAndRejectsEmptyNameOrUnknownId() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    fixture.controller.addMindWave();
    const MindWaveId id = project.mindWaves().front().id;

    QVERIFY(fixture.controller.renameMindWaveTo(id, QStringLiteral("Slow Pulse")));
    QCOMPARE(project.mindWaves().front().name, std::string("Slow Pulse"));

    QVERIFY(!fixture.controller.renameMindWaveTo(id, QString()));
    QVERIFY(!fixture.controller.renameMindWaveTo(999999, QStringLiteral("Nope")));
}

void MindWaveControllerTest::updateMindWaveWritesBackTheGivenWave() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    fixture.controller.addMindWave();
    const MindWaveId id = project.mindWaves().front().id;

    MindWave edited;
    edited.setPeriod(5.0);
    fixture.controller.updateMindWave(id, edited);

    QCOMPARE(project.mindWaves().front().wave.period(), 5.0);
}

void MindWaveControllerTest::refreshMindWavesPanelPushesTheLibraryIntoBothPanels() {
    Fixture fixture;
    Project project = Project::createNew(testSettings());
    fixture.controller.setProject(&project);
    fixture.controller.addMindWave();

    LayersPanel::RowData row;
    row.id = 42;
    row.name = QStringLiteral("Test Layer");
    row.type = LayerType::Normal;
    fixture.layersPanel.setLayers({row});

    fixture.controller.refreshMindWavesPanel();
    // rebuildRows()'s own row widgets are only scheduled via deleteLater()
    // (see LayersPanel::rebuildRows()'s own docs) - the setLayers() call
    // above and refreshMindWavesPanel()'s own setAvailableMindWaves() call
    // each rebuild the row widgets, leaving the first pass's own combo
    // still alive (pending deletion) unless the event loop gets a chance
    // to actually run it - QTest::qWait(0) does that, matching this
    // codebase's own established precedent for the same LayersPanel
    // gotcha (see e.g. test_layers_panel.cpp's own qWait(0) calls).
    QTest::qWait(0);

    QCOMPARE(fixture.mindWavesPanel.findChild<QListWidget*>(QStringLiteral("mindWavesList"))->count(), 1);
    const auto layerCombos = fixture.layersPanel.findChildren<QComboBox*>(QStringLiteral("opacityMindWaveCombo"));
    QCOMPARE(layerCombos.size(), 1);
    // "None" plus the one MindWave just added.
    QCOMPARE(layerCombos.front()->count(), 2);
    // FilterConfigurationPanel gets the same list too (v0.Y.31.1
    // Installment D3) - its own combos exist regardless of the currently
    // selected FilterType, so no equivalent setLayers()-style setup is
    // needed first.
    QCOMPARE(fixture.filterConfigurationPanel.findChild<QComboBox*>(QStringLiteral("blurSigmaMindWaveCombo"))->count(),
             2);
}
