#pragma once

#include <QObject>

class MidiConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasAnEmptyTableAndEveryProgramInTheAddCombo();
    void addMappingAddsARowAndRemovesTheProgramFromTheAddCombo();
    void changingTheToolPresetComboUpdatesTheProjectsMapping();
    void changingDurationScaleUpdatesTheProjectsMapping();
    void changingPitchOffsetUpdatesTheProjectsMapping();
    void deleteButtonRemovesTheMappingAndItsRowAndReAddsTheProgramToTheCombo();
    void setProjectPopulatesTheTableFromExistingMappings();
    void refreshToolPresetsPreservesEachRowsOwnSelection();
};
