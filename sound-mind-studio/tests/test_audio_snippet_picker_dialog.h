#pragma once

#include <QObject>

class AudioSnippetPickerDialogTest : public QObject {
    Q_OBJECT

private slots:
    void listsOneRowPerSnippetWithItsTimespan();
    void allRowsStartChecked();
    void selectedIndicesReturnsOnlyCheckedRows();
    void selectAllCheckboxTogglesEveryRow();
};
