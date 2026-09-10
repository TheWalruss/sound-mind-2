#pragma once

#include <QObject>

class ToolConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelIsAnOpaqueCircularBrush();
    void freshPanelHasBothOverlayCheckboxesOff();
    void changingTheTipShapeEmitsToolConfigurationChanged();
    void changingFalloffEmitsToolConfigurationChanged();
    void changingSizeEmitsToolConfigurationChanged();
    void changingIntensitySetsBothGradientStopsIntensity();
    void changingOpacitySetsBothGradientStopsOpacity();
    void togglingShowBoundingBoxesEmitsItsOwnSignal();
    void togglingShowPathGeometryEmitsItsOwnSignal();
};
