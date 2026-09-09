#pragma once

#include <QObject>

class RecordPanelTest : public QObject {
    Q_OBJECT

private slots:
    void toggleButtonEmitsToggleRequested();
    void setRecordingUpdatesButtonAndDisablesTheDevicePicker();
    void setInputDevicesListsSystemDefaultFirst();
    void changingTheInputDeviceEmitsInputDeviceChanged();
};
