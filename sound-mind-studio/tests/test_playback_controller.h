#pragma once

#include <QObject>

class PlaybackControllerTest : public QObject {
    Q_OBJECT

private slots:
    void freshControllerIsNotLoadedOrPlaying();
    void loadMarksItLoaded();
    void invalidateClearsLoadedWithoutStoppingPlayback();
    void loadEmitsDurationChanged();
    void playStartsPlayback();
    void pauseStopsPlaybackButKeepsItLoaded();
    void stopClearsLoadedAndPlaying();
    void seekEmitsPositionChangedImmediately();
    void seekDoesNothingWhenNotLoaded();
    void setVolumeChangesVolume();
    void outputDeviceMethodsAreCallableWithoutCrashing();
};
