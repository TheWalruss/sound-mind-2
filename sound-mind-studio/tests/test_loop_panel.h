#pragma once

#include <QObject>

class LoopPanelTest : public QObject {
    Q_OBJECT

private slots:
    void toggleButtonEmitsToggleRequested();
    void setRunningUpdatesButtonAndDisablesDevicePickers();
    void keepLoopingCheckBoxEmitsKeepLoopingChanged();
    void setKeepLoopingCheckedDoesNotEmitKeepLoopingChanged();
    void setInputDevicesListsSystemDefaultFirst();
    void changingTheInputDeviceEmitsInputDeviceChanged();
    void changingTheOutputDeviceEmitsOutputDeviceChanged();
};
