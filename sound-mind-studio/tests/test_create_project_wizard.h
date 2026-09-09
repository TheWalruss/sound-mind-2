#pragma once

#include <QObject>

class CreateProjectWizardTest : public QObject {
    Q_OBJECT

private slots:
    void settingsMatchProjectSettingsDefaultsInitially();
    void okIsDisabledUntilNameAndLocationAreBothSet();
    void pathAppendsSmprojExtensionWhenMissing();
    void pathKeepsAnAlreadyPresentSmprojExtension();
    void advancedFieldsAreHiddenUntilToggled();
    void changingAdvancedFieldsChangesSettings();
    void durationDrivesCanvasWidthAtTheCurrentTimestep();
    void canvasHeightStaysEqualToBinCount();
};
