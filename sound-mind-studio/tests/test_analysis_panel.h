#pragma once

#include <QObject>

class AnalysisPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelShowsNoReport();
    void setReportFillsTheSummaryGraphsAndLayerTable();
    void clippingAndOverTruePeakAreFlaggedInRed();
    void readingsAtThePlayheadFollowThePosition();
    void staleAndClearAndAnalyzingUpdateTheStatus();
    void analyzeButtonEmitsAnalyzeRequested();
    void formatLevelShowsADashAtTheFloor();
    void setReportFillsTheStereoTab();
    void negativeCorrelationIsFlaggedInRed();
    void theGoniometerPlotsMonoAsAVerticalLineAroundThePlayhead();
};
