#pragma once

#include <QObject>

class LayerControllerTest : public QObject {
    Q_OBJECT

private slots:
    void withNoProjectLookupsReturnNulloptOrNullptr();
    void setProjectMakesLookupsWork();
    void toggleLayerVisibilityChangesVisibilityAndEmitsLayersChanged();
    void setLayerOpacityChangesOpacity();
    void setLayerOpacityMindWaveChangesBindingAndTheRowDataReflectsIt();
    void setLayerTranslationChangesTranslation();
    void setLayerRescaleChangesRescale();
    void toggleLayerVisibilityIsUndoableAndRedoable();
    void setLayerOpacityIsUndoableAndRedoable();
    void setLayerOpacityMindWaveIsUndoableAndRedoable();
    void setLayerTranslationIsUndoableAndRedoable();
    void setLayerRescaleIsUndoableAndRedoable();
    void settingTheSameValueAgainDoesNotPushAnUndoEntry();
    void renameLayerToRenamesAndRejectsEmptyName();
    void deleteLayerRemovesALayerButRefusesALockedOne();
    void duplicateLayerCopiesContentAndPropertiesButRefusesALockedOne();
    void duplicateLayerNamesTheCopyUniquelyAndSelectsIt();
    void cleanUpLayerPhaseZeroesPhaseInSilentCellsOnlyButRefusesALayerWithNoContent();
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
