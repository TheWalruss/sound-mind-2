#pragma once

#include <QObject>

class AudioSnippetPickerDialogTest : public QObject {
    Q_OBJECT

private slots:
    void listsOneRowPerSnippetWithItsTimespan();
    void allRowsStartChecked();
    void selectedIndicesReturnsOnlyCheckedRows();
    void selectAllCheckboxTogglesEveryRow();

    // Snippet offsets, real-world testing pass finding #23.
    void freshDialogHasAZeroOffset();
    void changingTheOffsetSpinBoxEmitsOffsetChanged();
    void setSnippetsReplacesTheListAndResetsEveryRowToChecked();
};
