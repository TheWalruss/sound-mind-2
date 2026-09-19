#pragma once

#include <QObject>

class ChordGeneratorPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelDefaultsToACMajorTriadInBlockMode();
    void changingCategoryRebuildsTheChordComboAndEmitsParamsChanged();
    void changingRootOrOctaveUpdatesRootMidiNote();
    void switchingToArpeggioModeShowsTheArpeggioGroupAndHidesTheBlockGroup();
    void customOrderOnlyShowsTheCustomSequenceLineEdit();
    void randomOrderOnlyShowsTheRandomSeedSpinBox();
    void editingCustomSequenceParsesCommaSeparatedIndices();
    void subdivisionComboSetsStepBeats();
};
