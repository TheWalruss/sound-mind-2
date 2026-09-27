#pragma once

#include <QObject>

class MindCaptureDialogTest : public QObject {
    Q_OBJECT

private slots:
    void freshDialogHasTheGivenNameAndZeroForBothNumericFields();
    void windowTitleMatchesWhatWasPassedIn();
    void nameIsTrimmedOfLeadingAndTrailingWhitespace();
    void fundamentalFrequencyHzAndStartTimeOffsetSecondsReflectTheSpinBoxes();
};
