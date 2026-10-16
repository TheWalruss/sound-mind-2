#pragma once

#include <QObject>

class TextToolPanelTest : public QObject {
    Q_OBJECT

private slots:
    void editingAControlEmitsTheCompleteSettings();
    void setSettingsShowsThemWithoutEmitting();
    void applyAndResetAreEnabledOnlyWithABox();
};
