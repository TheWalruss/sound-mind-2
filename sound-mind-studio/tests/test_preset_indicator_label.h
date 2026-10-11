#pragma once

#include <QObject>

class PresetIndicatorLabelTest : public QObject {
    Q_OBJECT

private slots:
    void startsWithNoActivePreset();
    void setActivePresetShowsTheNameAndClearsModified();
    void markModifiedAddsAnAsteriskAndIsIgnoredWithoutAnActivePreset();
    void setActivePresetAgainResetsModified();
    void clearActivePresetReturnsToTheInactiveState();
};
