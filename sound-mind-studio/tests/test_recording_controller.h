#pragma once

#include <QObject>

class RecordingControllerTest : public QObject {
    Q_OBJECT

private slots:
    void commitAddsANamedRecordedLayer();
    void commitOverTheChosenTargetReplacesItsContent();
    void commitAsyncAddsTheLayerWhenTheSaveIsDone();
    void nothingCapturedChangesNothing();
    void toggleStartsAndStopsWithoutAddingALayerWhenNothingWasCaptured();
    void toggleRefusesWhileNotAllowedOrWithNoProject();
};
