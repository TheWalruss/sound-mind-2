#pragma once

#include <QObject>

class CreateProjectWizardTest : public QObject {
    Q_OBJECT

private slots:
    void settingsMatchProjectSettingsDefaultsInitially();
    void okIsDisabledUntilNameAndLocationAreBothSet();
    void pathCombinesLocationAndName();
    void pathStripsAnAlreadyPresentSmprojExtensionFromName();
    void advancedFieldsAreHiddenUntilToggled();
    void changingAdvancedFieldsChangesSettings();
    void durationDrivesCanvasWidthAtTheCurrentTimestep();
    void canvasHeightStaysEqualToBinCount();
    void collapsingAdvancedShrinksTheDialogBackDown();
};
