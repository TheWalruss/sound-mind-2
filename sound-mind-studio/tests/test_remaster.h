#pragma once

#include <QObject>

class RemasterTest : public QObject {
    Q_OBJECT

private slots:
    void remasteringAtTheSameSettingsReproducesTheLiveProject();
    void aFinerTimestepKeepsDurationEditsAndPlacement();
    void aReplacedSourceIsUsedInsteadOfTheOriginal();
    void theResultHasItsOwnCompleteHistory();
    void aHistoryNotStartingAtCreationCantBeRemastered();
    void dialogPrefillsFromTheProjectAndNeedsANewOutputPath();
};
