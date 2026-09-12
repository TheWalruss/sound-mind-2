#pragma once

#include <QObject>

class FilterConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasAFullyTransparentDefaultConfiguration();
    void changingAStartSpinBoxUpdatesStop0AndEmitsFilterConfigurationChanged();
    void changingAnEndSpinBoxUpdatesStop1AndEmitsFilterConfigurationChanged();
    void setFilterConfigurationSyncsAllEightSpinBoxesWithoutEmitting();
};
