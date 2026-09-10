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
    void changingOpacitySetsBothGradientStopsOpacity();
    void togglingShowBoundingBoxesEmitsItsOwnSignal();
    void togglingShowPathGeometryEmitsItsOwnSignal();
    void setColorSetsBothGradientStopsIntensityAndEmitsChange();
    void colorRoundTripsThroughSetColor();
    void colorButtonExistsForOpeningTheRealDialog();
};
