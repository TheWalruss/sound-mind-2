#pragma once

#include <QObject>

class DeviceConfigurationWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void refreshButtonEmitsRefreshRequested();
    void setInputDevicesListsSystemDefaultFirst();
    void setOutputDevicesListsSystemDefaultFirst();
    void changingTheInputDeviceEmitsInputDeviceChanged();
    void changingTheOutputDeviceEmitsOutputDeviceChanged();
    void gainSlidersStartAtUnityAndAllowAboveIt();
    void movingTheInputGainSliderEmitsInputGainPercentChanged();
    void setInputGainPercentDoesNotEmitInputGainPercentChanged();
    void testButtonsEmitTestToggledWithTheirCheckedState();
    void setInputLevelUpdatesTheLevelBarClamped();
    void setTestingInputAndOutputChangeCheckedStateWithoutEmittingSignals();
    void setInputDeviceSelectionEnabledTogglesTheInputComboOnly();
    void setSelectedInputDeviceSelectsItWithoutEmittingInputDeviceChanged();
    void setSelectedOutputDeviceSelectsItWithoutEmittingOutputDeviceChanged();
};
