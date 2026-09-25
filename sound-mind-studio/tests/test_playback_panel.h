#pragma once

#include <QObject>

class PlaybackPanelTest : public QObject {
    Q_OBJECT

private slots:
    void playPauseStopButtonsEmitTheirSignals();
    void positionSliderStartsAtZero();
    void movingThePositionSliderEmitsSeekRequested();
    void setPositionSecondsDoesNotEmitSeekRequested();
    void setPositionSecondsUpdatesTheTimeLabel();
    void repeatCheckBoxEmitsRepeatChanged();
    void setRepeatCheckedDoesNotEmitRepeatChanged();
    void scopeComboDefaultsToTrackAndEmitsScopeChanged();
};
