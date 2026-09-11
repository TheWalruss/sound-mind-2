#pragma once

#include <QObject>

class GridPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasBothAxisLabelsOff();
    void changingTheVerticalAxisComboEmitsVerticalAxisLabelModeChanged();
    void changingTheHorizontalAxisComboEmitsHorizontalAxisLabelModeChanged();
};
