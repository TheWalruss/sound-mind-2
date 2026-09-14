#pragma once

#include <QObject>

class MindWaveControllerTest : public QObject {
    Q_OBJECT

private slots:
    void withNoProjectEveryMutationIsANoOp();
    void addMindWaveAddsWithADefaultNameAndSelectsIt();
    void addMindWaveNamesSequentialEntriesDistinctly();
    void removeMindWaveRemovesAndReturnsToNoSelection();
    void removeMindWaveNoOpsForUnknownId();
    void renameMindWaveToRenamesAndRejectsEmptyNameOrUnknownId();
    void updateMindWaveWritesBackTheGivenWave();
    void refreshMindWavesPanelPushesTheLibraryIntoBothPanels();
};
