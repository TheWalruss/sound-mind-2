#pragma once

#include <QObject>

class DeviceComboHelpersTest : public QObject {
    Q_OBJECT

private slots:
    void populateDeviceComboListsSystemDefaultFirst();
    void populateDeviceComboWithNoDevicesOnlyHasTheDefaultEntry();
    void populateDeviceComboPreservesSelectionWhenStillPresent();
    void populateDeviceComboFallsBackToDefaultWhenSelectionDisappears();
    void populateDeviceComboDoesNotEmitCurrentIndexChanged();
    void setSelectedDeviceInComboSelectsAMatchingDevice();
    void setSelectedDeviceInComboFallsBackToDefaultForAnUnknownName();
    void setSelectedDeviceInComboFallsBackToDefaultForAnEmptyName();
    void setSelectedDeviceInComboDoesNotEmitCurrentIndexChanged();
};
