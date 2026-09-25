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
    void superpositionControlsStayHiddenWithAnEmptyStack();
    void addingTheFirstStackMemberRevealsTheSuperpositionControls();
    void removingTheLastStackMemberHidesTheSuperpositionControlsAgain();
    void selectingARowWithAnExistingStackShowsTheSuperpositionControlsImmediately();
    void editingASelectedStackMemberUpdatesThatIndexAndEmits();
    void changingBlendModeEmits();
    void selectMindWaveSelectsAnExistingRowAndNoOpsForUnknownId();
    void setMindWavesPreservesSelectionAndRedisplaysFromTheNewData();
    void setMindWavesDropsSelectionWhenTheIdIsGone();
    void clearSelectionDisablesBothEditorsAndEmits();
    void contentIsInAResizableScrollAreaSoThePanelCanShrinkBelowItsFullHeight();
    void freshPanelHasPreviewOff();
    void clickingThePreviewButtonEmitsPreviewToggled();
    void setPreviewEnabledChangesTheButtonWithoutEmitting();

    // Always-on per-row mini-preview, real-world testing pass finding #21.
    void freshRowsShowAPlaceholderPreviewBeforeSetPreviewImagesIsCalled();
    void setPreviewImagesShowsThePreviewOnTheMatchingRow();
    void setPreviewImagesLeavesANonMatchingRowsPreviewAsAPlaceholder();
    void setPreviewImagesPreservesTheCurrentSelectionAndDoesNotEmit();

    // Warp (v0.Y.39.1 Installment A).
    void enablingWarpEmitsAndSetsHasWarpSourceOnTheComposite();
    void disablingWarpClearsTheWarpSourceAndEmits();
    void editingTheWarpSourceEditorUpdatesItAndEmits();
    void changingWarpStrengthEmitsWhileWarpIsEnabled();
    void selectingARowWithNoWarpSourceLeavesTheWarpControlsDisabled();
    void selectingARowWithAnExistingWarpSourcePopulatesTheCheckboxStrengthAndEditor();
};
