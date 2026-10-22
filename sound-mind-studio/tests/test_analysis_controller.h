#pragma once

#include <QObject>

class AnalysisControllerTest : public QObject {
    Q_OBJECT

private slots:
    void analyzingShowsAReport();
    void anEditDuringAutoUpdateCancelsAndReanalyzes();
    void anotherProjectDropsTheReport();
    void autoUpdateIsRemembered();
};
