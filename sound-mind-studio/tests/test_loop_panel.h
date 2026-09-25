#pragma once

#include <QObject>

class LoopPanelTest : public QObject {
    Q_OBJECT

private slots:
    void toggleButtonEmitsToggleRequested();
    void setRunningUpdatesButtonLabelAndCheckedState();
    void keepLoopingCheckBoxEmitsKeepLoopingChanged();
    void keepLoopingCheckBoxIsLabeledFreezeLoop();
    void setKeepLoopingCheckedDoesNotEmitKeepLoopingChanged();
};
