#pragma once

#include <QObject>

class PlaybackPanelTest : public QObject {
    Q_OBJECT

private slots:
    void playPauseStopButtonsEmitTheirSignals();
    void setOutputDevicesListsSystemDefaultFirst();
    void changingTheOutputDeviceEmitsOutputDeviceChanged();
    void volumeSliderStartsAtUnityAndAllowsAboveIt();
    void movingTheVolumeSliderEmitsVolumePercentChanged();
    void setVolumePercentDoesNotEmitVolumePercentChanged();
    void positionSliderStartsAtZero();
    void movingThePositionSliderEmitsSeekRequested();
    void setPositionSecondsDoesNotEmitSeekRequested();
    void setPositionSecondsUpdatesTheTimeLabel();
};
