#pragma once

#include <QObject>

class RecordPanelTest : public QObject {
    Q_OBJECT

private slots:
    void toggleButtonEmitsToggleRequested();
    void setRecordingUpdatesButtonLabelAndCheckedState();
    void theTargetDefaultsToANewLayerNamedRecording();
    void recordedLayersAreListedAndSelectable();
    void theNameFieldShowsOnlyForANewLayer();
    void aRemovedTargetFallsBackToANewLayer();
    void targetControlsAreLockedWhileRecording();
    void liveViewCheckBoxEmitsLiveViewToggled();
};
