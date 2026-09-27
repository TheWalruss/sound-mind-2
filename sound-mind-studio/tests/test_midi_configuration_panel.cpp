#include "test_midi_configuration_panel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QtTest/QtTest>

#include "sound_mind/core/midi_program_mapping.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/midi_configuration_panel.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::MidiProgramMapping;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::ToolPresetId;
using sound_mind::studio::MidiConfigurationPanel;

namespace {

constexpr int kProgramColumn = 0;
constexpr int kToolPresetColumn = 1;
constexpr int kDurationScaleColumn = 2;
constexpr int kPitchOffsetColumn = 3;
constexpr int kDeleteColumn = 4;

}  // namespace

void MidiConfigurationPanelTest::freshPanelHasAnEmptyTableAndEveryProgramInTheAddCombo() {
    const MidiConfigurationPanel panel;

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    auto* addCombo = panel.findChild<QComboBox*>(QStringLiteral("addProgramCombo"));
    QVERIFY(table != nullptr);
    QVERIFY(addCombo != nullptr);
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(addCombo->count(), 128);
}

void MidiConfigurationPanelTest::addMappingAddsARowAndRemovesTheProgramFromTheAddCombo() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    panel.setProject(&project);
    auto* addCombo = panel.findChild<QComboBox*>(QStringLiteral("addProgramCombo"));
    addCombo->setCurrentIndex(4);  // Program 4: Electric Piano 1.
    const int programNumber = addCombo->currentData().toInt();
    auto* addButton = panel.findChild<QPushButton*>(QStringLiteral("addMappingButton"));

    addButton->click();

    QCOMPARE(project.midiProgramMappings().size(), static_cast<std::size_t>(1));
    QCOMPARE(project.midiProgramMappings().front().programNumber, programNumber);
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, kProgramColumn)->data(Qt::UserRole).toInt(), programNumber);
    QCOMPARE(addCombo->count(), 127);
    QCOMPARE(addCombo->findData(programNumber), -1);
}

void MidiConfigurationPanelTest::changingTheToolPresetComboUpdatesTheProjectsMapping() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration config;
    config.setTipShape(BrushTipShape::Diamond);
    const ToolPresetId presetId = project.addToolPreset("My Diamond", config);
    panel.setProject(&project);
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    project.setMidiProgramMapping(mapping);
    panel.refreshMappings();

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    auto* presetCombo = qobject_cast<QComboBox*>(table->cellWidget(0, kToolPresetColumn));
    QVERIFY(presetCombo != nullptr);
    const int index = presetCombo->findData(QVariant::fromValue(static_cast<qulonglong>(presetId)));
    QVERIFY(index >= 0);

    presetCombo->setCurrentIndex(index);

    QCOMPARE(project.midiProgramMappingForProgram(4)->toolPresetId, std::optional<ToolPresetId>(presetId));
}

void MidiConfigurationPanelTest::changingDurationScaleUpdatesTheProjectsMapping() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    panel.setProject(&project);
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    project.setMidiProgramMapping(mapping);
    panel.refreshMappings();

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    auto* durationSpinBox = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, kDurationScaleColumn));
    QVERIFY(durationSpinBox != nullptr);

    durationSpinBox->setValue(2.5);

    QCOMPARE(project.midiProgramMappingForProgram(4)->durationScale, 2.5);
}

void MidiConfigurationPanelTest::changingPitchOffsetUpdatesTheProjectsMapping() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    panel.setProject(&project);
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    project.setMidiProgramMapping(mapping);
    panel.refreshMappings();

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    auto* pitchSpinBox = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, kPitchOffsetColumn));
    QVERIFY(pitchSpinBox != nullptr);

    pitchSpinBox->setValue(-12.0);

    QCOMPARE(project.midiProgramMappingForProgram(4)->pitchOffsetSemitones, -12.0);
}

void MidiConfigurationPanelTest::deleteButtonRemovesTheMappingAndItsRowAndReAddsTheProgramToTheCombo() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    panel.setProject(&project);
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    project.setMidiProgramMapping(mapping);
    panel.refreshMappings();

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    auto* deleteButton = qobject_cast<QPushButton*>(table->cellWidget(0, kDeleteColumn));
    QVERIFY(deleteButton != nullptr);

    deleteButton->click();

    QVERIFY(project.midiProgramMappingForProgram(4) == nullptr);
    QCOMPARE(table->rowCount(), 0);
    auto* addCombo = panel.findChild<QComboBox*>(QStringLiteral("addProgramCombo"));
    QVERIFY(addCombo->findData(4) >= 0);
}

void MidiConfigurationPanelTest::setProjectPopulatesTheTableFromExistingMappings() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    project.setMidiProgramMapping(mapping);

    panel.setProject(&project);

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, kProgramColumn)->data(Qt::UserRole).toInt(), 4);
}

void MidiConfigurationPanelTest::refreshToolPresetsPreservesEachRowsOwnSelection() {
    MidiConfigurationPanel panel;
    Project project = Project::createNew(ProjectSettings{});
    const ToolPresetId presetId = project.addToolPreset("My Brush", ProceduralConfiguration{});
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    mapping.toolPresetId = presetId;
    project.setMidiProgramMapping(mapping);
    panel.setProject(&project);

    project.addToolPreset("Another Brush", ProceduralConfiguration{});
    panel.refreshToolPresets();

    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("midiMappingTable"));
    auto* presetCombo = qobject_cast<QComboBox*>(table->cellWidget(0, kToolPresetColumn));
    QVERIFY(presetCombo != nullptr);
    QCOMPARE(presetCombo->count(), 3);  // "None" + 2 presets.
    QCOMPARE(presetCombo->currentText(), QStringLiteral("My Brush"));
}
