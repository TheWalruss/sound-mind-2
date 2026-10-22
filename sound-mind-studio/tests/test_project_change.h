#pragma once

#include <QObject>

class ProjectChangeTest : public QObject {
    Q_OBJECT

private slots:
    void settingsOnlyChangesNothingShown();
    void addedLayersRefreshEverythingListingLayers();
    void layerContentRefoldsFromThatLayerAndItsThumbnail();
    void layerPropertiesLeaveCanvasAndPlaybackToTheLayerController();
};
