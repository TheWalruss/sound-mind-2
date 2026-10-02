#pragma once

#include <QObject>

class CheckListDialogTest : public QObject {
    Q_OBJECT

private slots:
    void buildsOneCheckBoxPerLabelWithTheGivenInitialStates();
    void aShorterInitiallyCheckedListLeavesTheMissingTailUnchecked();
    void selectAllChecksEveryBox();
    void deselectAllUnchecksEveryBox();
    void checkedStatesReflectsManualToggles();
};
