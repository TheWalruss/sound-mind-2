#pragma once

#include <QObject>

class ExportControllerTest : public QObject {
    Q_OBJECT

private slots:
    void anAudioExportWritesTheFileAndSaysSo();
    void nothingToExportIsReportedAndNothingStarts();
    void aSecondExportWhileOneRunsIsRefused();
    void cancellingDeletesThePartialFile();
    void aMacroThatCantBeRebuiltIsReported();
    void anImageExportWritesTheFileAndSaysSo();
    void anImageExportWithNothingVisibleIsReported();
};
