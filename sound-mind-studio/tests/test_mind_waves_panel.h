#pragma once

#include <QObject>

class MindWavesPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasNoRowsAndNoSelection();
    void setMindWavesPopulatesTheListAndClickingARowSelectsIt();
    void addButtonEmitsAddRequested();
    void deletingARowEmitsDeleteRequestedWithItsId();
    void renamingARowEmitsRenameRequestedWithItsId();
    void editingTheTopEditorEmitsMindWaveChangedWithTheSelectedId();
    void addingAStackMemberGrowsTheStackAndEmits();
    void removingASelectedStackMemberShrinksTheStackAndEmits();
    void editingASelectedStackMemberUpdatesThatIndexAndEmits();
    void changingBlendModeEmits();
    void selectMindWaveSelectsAnExistingRowAndNoOpsForUnknownId();
    void setMindWavesPreservesSelectionAndRedisplaysFromTheNewData();
    void setMindWavesDropsSelectionWhenTheIdIsGone();
    void clearSelectionDisablesBothEditorsAndEmits();
    void contentIsInAResizableScrollAreaSoThePanelCanShrinkBelowItsFullHeight();
};
