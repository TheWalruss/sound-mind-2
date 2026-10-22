#pragma once

#include <QObject>

class ImportControllerTest : public QObject {
    Q_OBJECT

private slots:
    void aBackgroundAudioImportAddsLayersWithTheirSourceAndOneEntry();
    void cancellingABackgroundImportAddsNothing();
    void anImageImportAddsALayerAndNotifies();
    void withNoProjectNothingIsImported();
};
