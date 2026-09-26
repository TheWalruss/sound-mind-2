#pragma once

#include <QObject>

class LayerControllerTest : public QObject {
    Q_OBJECT

private slots:
    void withNoProjectLookupsReturnNulloptOrNullptr();
    void setProjectMakesLookupsWork();
    void cycleLayerVisibilityStateCyclesVisibleMutedInvisibleAndEmitsLayersChanged();
    void setLayerOpacityChangesOpacity();
    void setLayerBalanceChangesBalance();
    void setLayerOpacityMindWaveChangesBindingAndTheRowDataReflectsIt();
    void setLayerTranslationChangesTranslation();
    void setLayerRescaleChangesRescale();
    void cycleLayerVisibilityStateIsUndoableAndRedoable();
    void setLayerOpacityIsUndoableAndRedoable();
    void setLayerBalanceIsUndoableAndRedoable();
    void setLayerOpacityMindWaveIsUndoableAndRedoable();
    void setLayerTranslationIsUndoableAndRedoable();
    void setLayerRescaleIsUndoableAndRedoable();
    void settingTheSameValueAgainDoesNotPushAnUndoEntry();
    void renameLayerToRenamesAndRejectsEmptyName();
    void deleteLayerRemovesALayerButRefusesALockedOne();
    void duplicateLayerCopiesContentAndPropertiesButRefusesALockedOne();
    void duplicateLayerNamesTheCopyUniquelyAndSelectsIt();
    void cleanUpLayerPhaseZeroesPhaseInSilentCellsOnlyButRefusesALayerWithNoContent();
    void selectLayerAboveAndBelowNavigateTheStackAndNoOpAtTheEnds();
    void moveSelectedLayerUpAndDownReorderTheStackAndRefuseAtLockedBoundaries();
    void nudgeSelectedLayerOpacityClampsAndIsUndoable();
    void addEmptyLayerAddsAndSelectsANormalLayer();
    void addFilterLayerAddsAndSelectsAFilterLayer();
    void handleLayerSelectionChangedSyncsFilterConfigurationPanel();
    void applyFilterConfigurationAppliesOnlyToAFilterLayer();
    void applyFilterConfigurationWithNoFilterLayerSelectedUpdatesThePendingConfiguration();
    void addFilterLayerSeedsFromThePendingFilterConfiguration();
    void setProjectResetsThePendingFilterConfiguration();
    void reorderLayersRefreshesEitherWay();
    void paintTargetLayerIdFallsBackToTheBottommostLayer();

    // Blend Mode (v0.Y.37.1).
    void setLayerBlendModeChangesBlendModeAndTheRowDataReflectsIt();
    void setLayerBlendModeIsUndoableAndRedoable();
};
