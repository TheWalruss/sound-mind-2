#pragma once

#include <QObject>

class WarpDialogTest : public QObject {
    Q_OBJECT

private slots:
    void freshDialogDefaultsToFrequencyAxisAndDisplaceMode();
    void selectingTimeAxisAndStretchModeUpdatesTheAccessors();
};
