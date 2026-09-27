#pragma once

#include <QObject>

class MidiImportDialogTest : public QObject {
    Q_OBJECT

private slots:
    void listsOneRowPerChannelAndOneRowPerSnippet();
    void allRowsStartCheckedAndSeparateLayerStartsChecked();
    void selectedChannelNumbersReturnsOnlyCheckedRows();
    void selectedSnippetIndicesReturnsOnlyCheckedRows();
    void selectAllChannelsCheckboxTogglesEveryChannelRow();
    void selectAllSnippetsCheckboxTogglesEverySnippetRow();
    void uncheckingSeparateLayerCheckboxIsReflected();
    void wholeFileStartsCheckedAndDisablesSnippetControls();
    void uncheckingWholeFileEnablesSnippetControls();
};
