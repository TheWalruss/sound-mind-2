#pragma once

#include <QObject>

class ChordGeneratorControllerTest : public QObject {
    Q_OBJECT

private slots:
    void defaultParamsPreviewACMajorTriad();
    void setParamsEmitsPreviewChanged();
    void previewFrequenciesHzIsAscendingAndDeduplicated();
    void stampAtDoesNothingWithNoProjectSet();
    void stampAtDoesNothingForAnOutOfRangeChordIndex();
    void stampAtAppendsASequenceOperationAndEmitsContentChanged();
    void stampAtClonesThePaintControllersCurrentToolConfiguration();
    void stampAtResolvesNotesAtTheGivenStartTime();
};
