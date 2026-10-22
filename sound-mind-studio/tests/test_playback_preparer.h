#pragma once

#include <QObject>

class PlaybackPreparerTest : public QObject {
    Q_OBJECT

private slots:
    void preparingLoadsAndPlaysTheWholeTrack();
    void withNothingToPlayNothingStarts();
    void cancellingPreparationLeavesNothingLoaded();
    void deltaPlaysTheEditedRegionEvenFromAStop();
    void trackScopeWithoutRepeatIgnoresEdits();
};
