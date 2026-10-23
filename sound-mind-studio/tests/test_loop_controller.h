#pragma once

#include <QObject>

class LoopControllerTest : public QObject {
    Q_OBJECT

private slots:
    void toggleAddsALoopInputLayerAndStartsOnceTheBackingIsReady();
    void toggleStopsAndARestartReusesTheLoopInputLayer();
    void toggleRefusesWithoutAnEngineOrWhileNotAllowed();
    void aFinishedTakeBecomesTheLayersContent();
    void anEditWhileRunningPreparesTheBackingAgain();
    void keepLoopingFreezesTheEngineAndSyncsThePanel();
    void aNewEngineAbandonsTheSession();
};
