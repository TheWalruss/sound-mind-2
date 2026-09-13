#pragma once

#include <QObject>

class LayerControllerTest : public QObject {
    Q_OBJECT

private slots:
    void withNoProjectLookupsReturnNulloptOrNullptr();
    void setProjectMakesLookupsWork();
    void toggleLayerVisibilityChangesVisibilityAndEmitsLayersChanged();
    void setLayerOpacityChangesOpacity();
    void setLayerTranslationChangesTranslation();
    void setLayerRescaleChangesRescale();
    void renameLayerToRenamesAndRejectsEmptyName();
    void deleteLayerRemovesALayerButRefusesALockedOne();
    void addEmptyLayerAddsAndSelectsANormalLayer();
    void addFilterLayerAddsAndSelectsAFilterLayer();
    void handleLayerSelectionChangedSyncsFilterConfigurationPanel();
    void applyFilterConfigurationAppliesOnlyToAFilterLayer();
    void reorderLayersRefreshesEitherWay();
    void paintTargetLayerIdFallsBackToTheBottommostLayer();
};
