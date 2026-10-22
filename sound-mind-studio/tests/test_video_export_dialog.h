#pragma once

#include <QObject>

class VideoExportDialogTest : public QObject {
    Q_OBJECT

private slots:
    void offersOnlyAvailableFormatsAndPrefersH264();
    void theExtensionFollowsTheFormat();
    void resolutionPresetsCanvasSizeAndCustomSize();
    void scrollingAndThePolarBanner();
    void choicesAreRememberedForTheNextExport();
};
