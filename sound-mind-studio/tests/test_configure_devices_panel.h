#pragma once

#include <QObject>

class ConfigureDevicesPanelTest : public QObject {
    Q_OBJECT

private slots:
    void refreshButtonEmitsRefreshRequested();
    void setInputDevicesListsSystemDefaultFirst();
    void setOutputDevicesListsSystemDefaultFirst();
    void changingTheInputDeviceEmitsInputDeviceChanged();
    void changingTheOutputDeviceEmitsOutputDeviceChanged();
    void gainSlidersStartAtUnityAndAllowAboveIt();
    void movingTheInputGainSliderEmitsInputGainPercentChanged();
    void movingTheOutputGainSliderEmitsOutputGainPercentChanged();
    void setInputGainPercentDoesNotEmitInputGainPercentChanged();
    void setOutputGainPercentDoesNotEmitOutputGainPercentChanged();
    void testButtonsEmitTestToggledWithTheirCheckedState();
    void setInputLevelUpdatesTheLevelBarClamped();
    void setTestingInputAndOutputChangeCheckedStateWithoutEmittingSignals();
    void setInputDeviceSelectionEnabledTogglesTheInputComboOnly();
    void setOutputDeviceSelectionEnabledTogglesTheOutputComboOnly();
};
